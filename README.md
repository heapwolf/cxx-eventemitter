# SYNOPSIS
A minimalist event emitter for C++

# DESCRIPTION
Event emitters are handy for evented programming, Node.js has a
[`nice one`](https://github.com/joyent/node/blob/master/lib/events.js).

Instead of a string, why not make the event a struct? Then you have
arbitrary events that emit the expected number of arguments and the
correct types.

The scope of this module is small so you can fork it and add the features
that you think you need. And no shared objects, it's header only!

# EXAMPLE

```cc
struct ExampleEvent {

  ExampleEvent(string name, int number) 
    : name(name), number(number) {};

  string name;
  int number;
};

int main() {

  Events::EventEmitter ee;

  ee.on<ExampleEvent>([](auto event) {

    cout << event.name << ": " << event.number << endl;
  });

  ee.emit(ExampleEvent("hello", 1));
  ee.emit(ExampleEvent("hello", 2));
  ee.emit(ExampleEvent("goodbye", 3));

  ee.off<ExampleEvent>();
  
  ee.emit(ExampleEvent("hello", 1));

  return 0;
}

```

