#include "microbus.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <numeric>
#include <vector>

static int64_t factorial(int n);

int main() {
    microbus::event_bus events;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Simple event bus example

    // Define event handler types.
    using calc_event_handler = microbus::event_bus::event_handler<double, int>;
    using message_event_handler = microbus::event_bus::event_handler<std::string>;

    // Multiply PI event handler
    calc_event_handler multiply_by_pi = [](double value, int multiply_by) {
        std::cout << "Multiplying " << value << " by " << multiply_by << " is " << (value * multiply_by) << std::endl;
    };
    int number_event_subscription_id = events.subscribe<double, int>("OnCalc", multiply_by_pi);

    // Message variable from main scope captured by lambda
    std::string greeting = "Hello, ";

    // Message event handler
    message_event_handler on_message = [&](const std::string &message) {
        std::cout << greeting << message << std::endl;
    };
    int message_event_subscription_id = events.subscribe<std::string>("OnMessage", on_message);

    auto pie = 3.14159265;

    // Trigger events
    events.trigger("OnCalc", pie, 4);
    events.trigger("OnMessage", std::string("Joe"));

    // Unsubscribe the message event
    events.unsubscribe("OnMessage", message_event_subscription_id);

    // Trigger events after unsubscribing
    events.trigger("OnCalc", pie, 8);
    events.trigger("OnMessage", std::string("Jane"));  // This won't produce output

    // Clear all events
    events.clear();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Smart pointer passed off-thread example

    using smart_pointer_event_handler = microbus::event_bus::event_handler<std::shared_ptr<std::string>>;

    // Message event handler
    smart_pointer_event_handler on_thread_message = [&](const std::shared_ptr<std::string>& message) {
        std::cout << "Received message: " << *message << std::endl;
    };

    int smart_pointer_event_subscription_id = events.subscribe<std::shared_ptr<std::string>>("OnSmartPtrMessage", on_thread_message);

    // Function to trigger the event from another thread
    auto trigger_event_from_thread = [&events]() {
        auto message = std::make_shared<std::string>("Hello from another thread!");
        events.trigger("OnSmartPtrMessage", message);
    };

    // Create and launch a new thread
    std::thread event_thread(trigger_event_from_thread);

    // Wait for the event thread to finish
    event_thread.join();

    // Clear event
    events.unsubscribe("OnSmartPtrMessage", smart_pointer_event_subscription_id);
    events.clear();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shared event bus and event loop example

    std::shared_ptr<microbus::event_bus> shared_event_bus = std::make_shared<microbus::event_bus>(); // Ensure the use of shared_ptr
    microbus::event_loop event_loop;

    using factorial_event_handler = microbus::event_bus::event_handler<int>;

    factorial_event_handler compute_factorial = [](int number) {
        auto result = factorial(number);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "Factorial of " << number << " is " << result << std::endl;
    };

    shared_event_bus->subscribe<int>("OnFactorial", compute_factorial);

    std::vector<uint64_t> numbers_to_factorial1 = {15, 17, 19};
    std::vector<uint64_t> numbers_to_factorial2 = {16, 18, 20};

    for (uint64_t number : numbers_to_factorial1) {
        event_loop.enqueue_event(shared_event_bus, "OnFactorial", number);
    }

    for (uint64_t number : numbers_to_factorial2) {
        event_loop.enqueue_event(shared_event_bus, "OnFactorial", number);
    }

    // Wait until loop finishes handling all events
    event_loop.wait_until_finished();
    event_loop.stop();

    shared_event_bus->clear();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shared event bus and event loop context example
    using my_event_handler = microbus::event_bus::event_handler<int>;
    auto context = microbus::shared_context();

    int on_number_id = context.subscribe("OnNumber", (my_event_handler)[](int number) {
        auto result = factorial(number);
        std::cout << "Number " << number << " was passed to event." << std::endl;
    });
    context.enqueue_event("OnNumber", 69);

    context.wait_until_finished();
    context.unsubscribe("OnNumber", on_number_id);
    context.stop();
}

static int64_t factorial(int n) {
    std::vector<int> numbers(n);
    std::iota(numbers.begin(), numbers.end(), 1);
    return std::accumulate(numbers.begin(), numbers.end(), 1LL, std::multiplies<>());
}
