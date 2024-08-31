# microbus - A simple event-bus and event-loop header-only library

The `microbus` namespace provides a simple yet efficient event-bus and event-loop implementation in C++17.
This library includes classes to manage subscriptions, event notifications, and asynchronous event processing.
The bus component can be used in a simple subscribe/trigger scenario, or, in a complex multi-threading scenario.

## Key Features

- **Thread-Safe**: Both `event_bus` and `event_loop` classes use mutexes to ensure thread-safety.
- **Type Erasure**: Handlers are type-erased, allowing storage of various callable objects in a unified manner.
- **Asynchronous Processing**: Events can be processed asynchronously using the `event_loop` class.
- **Automatic Cleanup**: Event handlers are automatically cleaned up when unsubscribed or when the event bus is cleared.
- **Flexible Subscription Management**: Supports multiple subscribers per event and manages them using unique subscription IDs.
- **Context Helper Class**: Helps operate the bus and loop pair in common application use-cases.

## Overview

### Classes

- **type_erased_handler**: An abstract base class for type-erased handlers.
- **concrete_handler**: A concrete handler that binds a function to a given set of arguments.
- **event_bus**: Manages event subscriptions and notifications.
- **event_loop**: Processes asynchronous events.

## Detailed Description

### `type_erased_handler`

This is a base class for handlers that erase type information. It provides a virtual `invoke` method which derived classes must implement.

### `concrete_handler`

A templated class that derives from `type_erased_handler`. It binds a specific function (handler) to a set of arguments. It overrides the `invoke` method to call the stored function with the provided arguments.

### `event_bus`

This class manages event subscriptions and notifications. It allows users to subscribe to events, unsubscribe, and trigger events.

- **subscribe**: Allows a client to subscribe to an event with a specified handler. Returns a unique subscription ID.
- **unsubscribe**: Unsubscribes a client from an event using the event name and subscription ID.
- **trigger**: Triggers an event, calling all subscribed handlers with provided arguments.
- **clear**: Clears all event subscriptions.

### `event_loop`

This class manages the processing of asynchronous events. It runs an internal thread to process events queued for execution.

- **enqueue_event**: Enqueues an event for asynchronous processing. The event will be processed by calling the relevant handlers from the `event_bus`.
- **wait_until_finished**: Blocks until all events in the queue are processed.
- **stop**: Stops the event loop and joins the internal thread.

### `shared_context`

This class is optional, it helps to operate the bus nad loop pair
in common development scenarios.
It allows subscribing and queuing event as well as operating the loop.

```cpp
#include "microbus.hpp"
#include <iostream>

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
```

## Example Usage

This example demonstrates subscribing to an event,
triggering it synchronously and asynchronously,
and then stopping the event loop.

```cpp
#include <iostream>
#include <memory>
#include "microbus.hpp"

int main() {
    auto bus = std::make_shared<microbus::event_bus>();
    microbus::event_loop loop;

    // Subscribe to event
    int subscription_id = bus->subscribe<int>("test_event", [](int value) {
        std::cout << "Received event with value: " << value << std::endl;
    });

    // Trigger the event synchronously
    bus->trigger("test_event", 42);

    // Queue the event for asynchronous processing
    loop.enqueue_event(bus, "test_event", 100);

    // Wait for all events to be processed
    loop.wait_until_finished();

    // Unsubscribe from the event
    bus->unsubscribe("test_event", subscription_id);

    // Stop the event loop
    loop.stop();

    return 0;
}
```
