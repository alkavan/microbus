#ifndef MICROBUS_MICROBUS_HPP
#define MICROBUS_MICROBUS_HPP

/*
MIT License

Copyright (c) 2024 Igal Alkon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <memory>
#include <tuple>
#include <type_traits>
#include <queue>
#include <thread>
#include <condition_variable>

namespace microbus {

    /**
     * @brief A base class for type-erased handlers.
     */
    struct type_erased_handler {
        virtual ~type_erased_handler() = default;

        /**
         * @brief Invokes the handler with the provided arguments.
         * @param args Pointer to the arguments.
         */
        virtual void invoke(void* args) = 0;
    };

    /**
     * @brief A concrete handler that binds a function to a given set of arguments.
     * @tparam Fn Type of the function.
     * @tparam Args Argument types.
     */
    template<typename Fn, typename... Args>
    struct concrete_handler : type_erased_handler {
        /**
         * @brief Constructs a concrete handler.
         * @param fn The function to be handled.
         */
        explicit concrete_handler(Fn&& fn) : fn_(std::forward<Fn>(fn)) {}

        /**
         * @brief Invokes the handler with the provided arguments.
         * @param args Pointer to the arguments.
         */
        void invoke(void* args) override {
            auto tuple_args = static_cast<std::tuple<Args...>*>(args);
            std::apply(fn_, *tuple_args);
        }

        Fn fn_; ///< The function to be invoked.
    };

    /**
     * @brief A class representing an event bus for managing subscriptions and event notifications.
     */
    class event_bus : public std::enable_shared_from_this<event_bus> {
    public:
        template <typename... Args>
        using event_handler = std::function<void(Args...)>;

        /**
         * @brief Subscribes to an event with a given handler.
         * @tparam Args Argument types for the handler.
         * @param event_name The name of the event.
         * @param handler The handler to be called when the event is triggered.
         * @return A subscription ID.
         */
        template <typename... Args>
        int subscribe(const std::string& event_name, event_handler<Args...> handler) {
            std::unique_lock lock(mutex_);
            int id = next_id_++;

            auto concreteHandler = std::make_unique<concrete_handler<event_handler<Args...>, Args...>>(std::move(handler));
            subscribers_[event_name].emplace_back(id, std::move(concreteHandler));
            return id;
        }

        /**
         * @brief Unsubscribes from an event.
         * @param event_name The name of the event.
         * @param id The subscription ID.
         */
        void unsubscribe(const std::string& event_name, int id) {
            std::unique_lock lock(mutex_);
            if (subscribers_.count(event_name)) {
                auto& handlers = subscribers_[event_name];
                handlers.erase(
                        std::remove_if(
                                handlers.begin(), handlers.end(),
                                [id](const auto& pair) { return pair.first == id; }
                        ),
                        handlers.end()
                );
                if (handlers.empty()) {
                    subscribers_.erase(event_name);
                }
            }
        }

        /**
         * @brief Triggers an event and calls all subscribed handlers.
         * @tparam Args Argument types for the event.
         * @param event_name The name of the event.
         * @param params The arguments to pass to the handlers.
         */
        template <typename... Args>
        void trigger(const std::string& event_name, Args&&... params) {
            std::shared_lock lock(mutex_);
            if (subscribers_.count(event_name)) {
                auto tuple_args = std::make_tuple(std::forward<Args>(params)...);
                for (const auto& [id, handler] : subscribers_[event_name]) {
                    handler->invoke(&tuple_args);
                }
            }
        }

        /**
         * @brief Clears all subscriptions.
         */
        void clear() {
            std::unique_lock lock(mutex_);
            subscribers_.clear();
        }

    private:
        using handler_ptr = std::unique_ptr<type_erased_handler>;
        std::unordered_map<std::string, std::vector<std::pair<int, handler_ptr>>> subscribers_; ///< Container for event subscribers.
        mutable std::shared_mutex mutex_; ///< Mutex for thread safety.

        friend class event_loop;

        /**
         * @brief Internal method to trigger an event.
         * @param event_name The name of the event.
         * @param args Pointer to the arguments.
         */
        void trigger_impl(const std::string& event_name, void* args) {
            std::shared_lock lock(mutex_);
            if (subscribers_.count(event_name)) {
                for (const auto& [id, handler] : subscribers_[event_name]) {
                    handler->invoke(args);
                }
            }
        }

        int next_id_ = 0; ///< The next subscription ID.
    };

    /**
     * @brief A class representing an event loop for processing asynchronous events.
     */
    class event_loop {
    public:
        /**
         * @brief Constructs an event loop and starts the loop thread.
         */
        event_loop() : stop_flag_(false), event_loop_thread_(&event_loop::process_event_loop, this) {}

        /**
         * @brief Destroys the event loop and stops the loop thread.
         */
        ~event_loop() {
            stop();
            if (event_loop_thread_.joinable())
                event_loop_thread_.join();
        }

        /**
         * @brief Enqueues an event to be processed asynchronously.
         * @tparam Args Argument types for the event.
         * @param bus Shared pointer to the event bus.
         * @param event_name The name of the event.
         * @param params The arguments to pass to the event handlers.
         */
        template <typename... Args>
        void enqueue_event(std::shared_ptr<event_bus> &bus, const std::string& event_name, Args&&... params) {
            auto tuple_args = std::make_shared<std::tuple<std::decay_t<Args>...>>(std::forward<Args>(params)...);
            {
                std::unique_lock lock(queue_mutex_);
                async_event_queue_.emplace([bus, event_name, tuple_args] {
                    bus->trigger_impl(event_name, tuple_args.get());
                });
            }
            queue_condition_.notify_one();
        }

        /**
         * @brief Waits until all events in the queue are processed.
         */
        void wait_until_finished() {
            std::unique_lock lock(wait_mutex_);
            wait_condition_.wait(lock, [this] {
                std::unique_lock q_lock(queue_mutex_);
                return async_event_queue_.empty();
            });
        }

        /**
         * @brief Stops the event loop.
         */
        void stop() {
            {
                std::unique_lock lock(queue_mutex_);
                stop_flag_ = true;
            }
            queue_condition_.notify_all();
        }

    private:
        std::queue<std::function<void()>> async_event_queue_; ///< Queue for asynchronous events.
        std::mutex queue_mutex_; ///< Mutex for queue operations.
        std::condition_variable queue_condition_; ///< Condition variable for queue notifications.
        bool stop_flag_; ///< Flag to stop the event loop.
        std::thread event_loop_thread_; ///< Thread running the event loop.

        std::mutex wait_mutex_; ///< Mutex for wait operations.
        std::condition_variable wait_condition_; ///< Condition variable for wait notifications.

        /**
         * @brief Processes the event loop by handling events in the queue.
         */
        void process_event_loop() {
            while (true) {
                std::function<void()> event_task;

                {
                    std::unique_lock lock(queue_mutex_);
                    queue_condition_.wait(lock, [this] { return stop_flag_ || !async_event_queue_.empty(); });

                    if (stop_flag_ && async_event_queue_.empty()) {
                        break;
                    }

                    event_task = std::move(async_event_queue_.front());
                    async_event_queue_.pop();
                }

                event_task();

                // Notify that an event has been processed and possibly that the queue is empty
                {
                    std::unique_lock lock(wait_mutex_);
                    wait_condition_.notify_all();
                }
            }
        }
    };
}

#endif //MICROBUS_MICROBUS_HPP