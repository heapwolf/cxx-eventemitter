#include <assert.h>
#include "../index.hxx"

#define ASSERT(message, ...) do { \
  if(!(__VA_ARGS__)) { \
    std::cerr << "FAIL: " << message << std::endl; \
    exit(0);\
  } \
  else { \
    std::cout << "OK: " << message << std::endl; \
    testcount++; \
  } \
} while(0);

int main () {
  int testcount = 0;
  int testplan = 9;

  //
  // sanity test.
  //
  ASSERT("sanity: true is true", true == true);

  EventEmitter ee;

  ASSERT("maxListeners should be 10", ee.maxListeners == 10);

  ee.on("event1", [&](int a, std::string& b) {
    ASSERT("first arg should be equal to 10", a == 10);
    ASSERT("second arg should be foo", b == "foo");
  });

  ee.emit("event1", 10, (std::string) "foo");

  try {
    ee.on("event1", []() {});
  }
  catch(...) {
    ASSERT("duplicate listener", true);
  }

  ee.on("event2", [&]() {
    ASSERT("no params", true);
  });

  ee.emit("event2");

  int count1 = 0;
  ee.on("event3", [&]() {
    if (++count1 == 10) {
      ASSERT("event was emitted correct number of times", true);
    }
  });

  for (int i = 0; i < 10; i++) {
    ee.emit("event3");
  }

  int count2 = 0;
  ee.on("event4", [&](){
    count2++;
    if (count2 > 1) {
      ASSERT("event was fired after it was removed", false);
    }
  });

  ee.emit("event4");
  ee.off("event4");
  ee.emit("event4");
  ee.emit("event4");

  int count3 = 0;
  ee.on("event5", [&]() {
    count3++;
    ASSERT("events were fired even though all listeners were removed", false);
  });

  ASSERT("the correct number of listeners has been reported", ee.listeners() == 4);
  
  ee.off();

  ASSERT("no additional events fired after all listeners removed",
    count1 == 10 &&
    count2 == 1 &&
    count3 == 0
  );

  ASSERT("testcount == plan", testcount == testplan)
}
