# SYNOPSIS
A minimal, thread-safe event emitter for `C++`.

# DESCRIPTION
This emitter allows you to emit and receive arbitrary/variadic paramaters of 
equal type. Captures are also allowed.

```c++
#include "index.hxx"

int main() {

  EventEmitter ee;

  ee.on("hello", [&](string name, int num) {
    // ...do something with the values.
  });

  ee.emit("hello", "beautiful", 100);
}
```

# MOTIVATION
- **Decoupling Code:** Components can communicate without direct dependencies. An event emitter acts as a central hub, allowing different parts of your application to react to events without needing to know about the originator of the event or other listeners. This promotes cleaner, more modular, and maintainable code.
- **Simplified Event Handling:** Provides a straightforward API for registering listeners and emitting events, abstracting away the complexities of manual callback management.
- **Asynchronous-like Patterns:** Facilitates an event-driven architecture, which is excellent for handling responses to actions, state changes, or any scenario where multiple parts of an application need to react to a specific occurrence.
- **Flexibility:** Supports various C++ callable types for listeners, including lambdas, free functions, functors, and `std::function` objects.
- **Familiarity:** Implements a pattern common in many other programming environments (like Node.js and browser JavaScript), making it intuitive for event-driven async programs.
- **Modern C++:** Built using modern C++ features (`std::function`, `std::any`, templates, type traits) for a balance of type safety (within the bounds of type erasure) and flexibility.
- **Header-Only:** Easy to integrate into any C++ project by simply including the header file.

# FEATURES
- Register multiple listeners for the same event name.
- Register listeners that are automatically removed after being called once (`once`).
- Emit events with an arbitrary number of arguments of various types.
- Remove all listeners for a specific event name (`off(eventName)`).
- Remove all listeners from the emitter for all events (`off()`).
- Query the current number of active listeners (`listeners()`).
- Configurable `maxListeners` threshold with a warning for potential memory leaks if too many listeners are added.
- Header-only library for easy integration.

# INSTALL

```
clib install heapwolf/cxx-eventemitter
```

# TEST

```
cmake test
cmake perf
```

# USAGE

## Creating an EventEmitter

First, include the header and create an instance of EventEmitter:

```c++
#include "index.hxx" // Or your path to events.h
// ...

EventEmitter ee;
```

## Listening for Events: `on(eventName, callback)`
Registers a callback function to be executed when an event with the specified eventName is emitted. You can register multiple listeners for the same event.

- `eventName (std::string)`: The name of the event to listen for.

- `callback (Callable)`: A function-like object (lambda, functor, function pointer, std::function) that will be invoked when the event is emitted. The arguments of the callback should match the arguments passed during emit. Callback return values are ignored.

```c++
ee.on("user_login", [](int userId, const std::string& username) { // Listener 1
  std::cout << "User logged in: ID=" << userId << ", Name=" << username << std::endl;
});

ee.on("user_login", [](int userId, const std::string& /*username*/) { // Listener 2 for the same event
  std::cout << "Logging login activity for user ID: " << userId << std::endl;
});
```

## Listening for an Event Once: `once(eventName, callback)`
Registers a callback function that will be executed at most once for the specified eventName. After the callback is invoked for the first time, it is automatically unregistered.

- `eventName (std::string)`: The name of the event.

- `callback (Callable)`: The listener to execute once.

```c++
ee.once("app_initialized", []() {
  std::cout << "Application initialized (this message appears only once)." << std::endl;
});
```

## Emitting Events: `emit(eventName, args...)`

Emits an event with the given eventName, optionally passing arguments (args...) to all registered listeners for that event.

- `eventName (std::string)`: The name of the event to emit.

- `args... (Variadic)`: The arguments to pass to the listener callbacks. The types of these arguments should correspond to what the listeners expect.

```c++
// Emit the "user_login" event (both registered listeners will be called)
int currentUserId = 101;
std::string currentUsername = "Alice";
ee.emit("user_login", currentUserId, currentUsername);

// Emit the "app_initialized" event (the 'once' listener will be called and then removed)
ee.emit("app_initialized");
ee.emit("app_initialized"); // The 'once' listener will not be called again
```

## Removing Listeners: `off(eventName)`

Removes all listeners registered for the specified eventName.

- `eventName (std::string)`: The name of the event whose listeners should be removed.

```c++
ee.off("user_login"); // All listeners for "user_login" are removed.
ee.emit("user_login", 202, "Bob"); // No listeners will be called.

off()
```

Removes all listeners for all events from the EventEmitter instance.

```c++
ee.on("eventA", [](){});
ee.on("eventB", [](){});
std::cout << "Listeners before global off(): " << ee.listeners() << std::endl;
ee.off();
std::cout << "Listeners after global off(): " << ee.listeners() << std::endl; // Should be 0
ee.emit("eventA"); // No listeners called
ee.emit("eventB"); // No listeners called
```

## Getting Listener Count: `listeners()`

Returns the total number of active listeners currently registered with the EventEmitter across all event names.

int count = ee.listeners();
std::cout << "Total active listeners: " << count << std::endl;

## Preventing leaks

A public integer property that defaults to 10. If you add more than maxListeners listeners (total across all events), a warning will be printed to std::cout suggesting a potential memory leak. This is a safeguard, not a hard limit; listeners can still be added beyond this count.

```c++
EventEmitter myEmitter;
std::cout << "Default maxListeners: " << myEmitter.maxListeners << std::endl; // Outputs 10
myEmitter.maxListeners = 5; // You can change it

for (int i = 0; i < 6; ++i) {
  // Assuming a unique event name for each to demonstrate the total count leading to warning
  myEmitter.on("some_event_" + std::to_string(i), [](){});
  // A warning will be printed when the 6th listener is added.
}
```

## Callback Signatures and Argument Handling

Argument Matching: When you emit an event with certain arguments (e.g., emit("event", 10, "hello")), your listeners registered for "event" should expect compatible arguments (e.g., [](int i, std::string s){...}). The library uses std::decay_t on argument types for internal storage and retrieval matching, which generally means value types, pointers, or references will be handled robustly. For instance, if a listener expects const std::string&, passing a std::string literal or std::string object to emit will work.

Return Values: Any value returned by a listener callback is ignored by the EventEmitter. Callbacks are typically used for their side effects.

Supported Callables: You can use lambdas (recommended for conciseness and capturing context), free functions, function objects (functors), and std::function objects as callbacks.
