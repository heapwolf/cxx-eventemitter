# SYNOPSIS
A minimalist event emitter for `C++`.

# DESCRIPTION
This emitter allows you to emit and receive arbitrary/variadic paramaters of 
equal type. Captures are also allowed.

# BUILD STATUS
![build-status](https://travis-ci.org/datcxx/cpp-eventemitter.svg)

# EXAMPLE

```c++
#include "events.h"

int main() {

  EventEmitter ee;

  ee.on("hello", [&](string name, int num) {
    // ...do something with the values.
  });

  ee.emit("hello", "beautiful", 100);
}
```

# API

## INSTANCE METHODS

### `void` on(string eventName, lambda callback);
Listen for an event multiple times.

### `void` once(string eventName, lambda callback);
Listen for an event once.

### `void` emit(string eventName [, arg1 [, arg2...]]);
Emit data for a particular event.

### `void` off(string eventName);
Remove listener for a particular event.

### `void` off();
Remove all listeners on the emitter.

### `int` listeners();
Number of listeners an emitter has.

## INSTANCE MEMBERS

### maxListeners
Number of listeners an event emitter should have
before considering that there is a memory leak.

