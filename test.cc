#include <iostream>
#include "events.h"

//
// g++ -o test ./test.cc -std=c++1y
//
using namespace std;

//
// example event type
//
struct ExampleEvent {

  //
  // define how the event can be called and
  // initialize its members with the values sent.
  //
  ExampleEvent(string name, int number) :
    name(name),
    number(number)
  { return;
  };

  //
  // define the members of the event.
  //
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
}

