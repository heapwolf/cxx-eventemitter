#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <sstream>
#include <utility>

#include "../index.hxx"

int assertions_run = 0;
int assertions_passed = 0;

#define ASSERT(message, condition) do { \
  assertions_run++; \
  if(!(condition)) { \
    std::cerr << "FAIL: " << (message) \
      << " (Assertion failed: `" #condition "` was false at " << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
  } else { \
    std::cout << "OK: " << (message) << std::endl; \
    assertions_passed++; \
  } \
} while(0)

struct CallbackTracker {
  int called_count = 0;
  void trigger() { called_count++; }
  void reset() { called_count = 0; }
  int count() const { return called_count; }
};

struct ComplexArg {
  int id;
  std::string data;
  ComplexArg(int i = 0, std::string d = "") : id(i), data(std::move(d)) {}
  // Equality operator for assertion
  bool operator==(const ComplexArg& other) const {
    return id == other.id && data == other.data;
  }
};

struct TestFunctor {
  CallbackTracker& tracker;
  explicit TestFunctor(CallbackTracker& t) : tracker(t) {}
  void operator()() { tracker.trigger(); } // Overload for no arguments
  void operator()(int, const std::string&) { tracker.trigger(); } // Overload for specific arguments
};

void example_free_function(CallbackTracker& tracker, int /*value*/) {
  tracker.trigger();
}

int return_value_callback_side_effect = 0;
int callback_returning_int(int input) {
  return_value_callback_side_effect = input * 10;
  return input * 2;
}

class StreamRedirector {
  std::ostream& stream_ref;
  std::streambuf* original_buffer;
public:
  StreamRedirector(std::ostream& stream, std::streambuf* new_buffer)
    : stream_ref(stream), original_buffer(stream.rdbuf(new_buffer)) {}
  ~StreamRedirector() {
    stream_ref.rdbuf(original_buffer);
  }
  // Non-copyable!
  StreamRedirector(const StreamRedirector&) = delete;
  StreamRedirector& operator=(const StreamRedirector&) = delete;
};


// Test #21 Worker Function, These atonics will be shared by threads in the test
std::atomic<int> async_total_on_callbacks_fired(0);
std::atomic<int> async_total_once_callbacks_fired(0);
std::atomic<int> async_total_listeners_registered(0);

const int ASYNC_TEST_ITERATIONS_PER_THREAD = 50; // Number of operations per thread

void async_worker_function(EventEmitter& emitter, int thread_id) {
  for (int i = 0; i < ASYNC_TEST_ITERATIONS_PER_THREAD; ++i) {
    // Create unique event names for each listener to avoid unintended shared triggers
    // and simplify callback counting for this specific test.
    std::string on_event_name = "async_on_event_t" + std::to_string(thread_id) + "_i" + std::to_string(i);
    std::string once_event_name = "async_once_event_t" + std::to_string(thread_id) + "_i" + std::to_string(i);

    // Register an 'on' listener
    emitter.on(on_event_name, [&]() {
      async_total_on_callbacks_fired++;
    });
    async_total_listeners_registered++;

    // Register a 'once' listener
    emitter.once(once_event_name, [&]() {
      async_total_once_callbacks_fired++;
    });
    async_total_listeners_registered++;

    // Emit events that should trigger the listeners just added
    emitter.emit(on_event_name);    // Triggers the 'on' listener
    emitter.emit(once_event_name);  // Triggers the 'once' listener (it will then be removed)
  }
}


int main() {
  /// - Test #1: Sanity and maxListeners default
  ASSERT("Sanity: true is true", true == true);
  EventEmitter ee_default;
  ASSERT("Default maxListeners should be 10", ee_default.maxListeners == 10);
  ASSERT("Default listeners should be 0", ee_default.listeners() == 0);

  /// - Test #2: Basic 'on' and 'emit' with lambda and arguments
  EventEmitter ee;
  CallbackTracker tracker1;
  int event1_arg_a = 0;
  std::string event1_arg_b;
  ee.on("event1", [&](int a, const std::string& b) {
    tracker1.trigger();
    event1_arg_a = a;
    event1_arg_b = b;
  });
  ee.emit("event1", 10, std::string("foo"));
  ASSERT("Basic 'on'/'emit': listener called", tracker1.count() == 1);
  ASSERT("Basic 'on'/'emit': first arg correct", event1_arg_a == 10);
  ASSERT("Basic 'on'/'emit': second arg correct", event1_arg_b == "foo");
  ASSERT("Basic 'on'/'emit': listener count is 1", ee.listeners() == 1);

  /// - Test #3: Multiple listener registration for the same event
  CallbackTracker tracker1_duplicate;
  std::string event1_dup_arg_b;
  ee.on("event1", [&](int /*a*/, const std::string& b_dup) {
    tracker1_duplicate.trigger();
    event1_dup_arg_b = b_dup;
  });
  ASSERT("Multiple listeners: Adding second listener for same event name succeeded", true);
  ASSERT("Multiple listeners: Listener count is now 2 (for 'event1')", ee.listeners() == 2);

  tracker1.reset();
  event1_arg_a = 0; event1_arg_b = "";

  ee.emit("event1", 20, std::string("bar"));
  ASSERT("Multiple listeners: Original listener called on second emit", tracker1.count() == 1);
  ASSERT("Multiple listeners: Original listener arg 'a' correct on second emit", event1_arg_a == 20);
  ASSERT("Multiple listeners: Original listener arg 'b' correct on second emit", event1_arg_b == "bar");
  ASSERT("Multiple listeners: Second listener called on emit", tracker1_duplicate.count() == 1);
  ASSERT("Multiple listeners: Second listener arg 'b_dup' correct", event1_dup_arg_b == "bar");

  /// - Test #4: 'emit' for non-existent event
  ee.emit("non_existent_event", 123, "data");
  ASSERT("Emit non-existent: program continues (no crash)", true);

  /// - Test #5: 'on' and 'emit' with no arguments
  CallbackTracker tracker2;
  ee.on("event2", [&]() {
    tracker2.trigger();
  });
  ee.emit("event2");
  ASSERT("No-arg 'on'/'emit': listener called", tracker2.count() == 1);
  ASSERT("No-arg 'on'/'emit': listener count is 3 (2 for event1, 1 for event2)", ee.listeners() == 3);

  /// - Test #6: 'off(eventName)'
  ee.off("event1");
  tracker1.reset();
  tracker1_duplicate.reset();
  ee.emit("event1", 30, std::string("baz"));
  ASSERT("off(event1): original listener not called after removal", tracker1.count() == 0);
  ASSERT("off(event1): second listener for event1 not called after removal", tracker1_duplicate.count() == 0);
  ASSERT("off(event1): listener count is 1 (event2 remains)", ee.listeners() == 1);
  ee.off("non_existent_to_off");
  ASSERT("off(non_existent_event): listener count still 1", ee.listeners() == 1);

  /// - Test #7: 'once(eventName, callback)'
  EventEmitter ee_once;
  CallbackTracker tracker_once;
  ee_once.once("event_once", [&]() { tracker_once.trigger(); });
  ASSERT("once: listener count is 1 before emit", ee_once.listeners() == 1);
  ee_once.emit("event_once");
  ASSERT("once: listener called first time", tracker_once.count() == 1);
  ASSERT("once: listener count is 0 after first emit", ee_once.listeners() == 0);
  ee_once.emit("event_once");
  ASSERT("once: listener not called second time", tracker_once.count() == 1);

  /// - Test #8: 'once' with arguments
  CallbackTracker tracker_once_args; int once_arg_val = 0;
  ee_once.once("event_once_args", [&](int val) { tracker_once_args.trigger(); once_arg_val = val; });
  ASSERT("once_args: listener count is 1 (on ee_once)", ee_once.listeners() == 1);
  ee_once.emit("event_once_args", 99);
  ASSERT("once_args: listener called", tracker_once_args.count() == 1);
  ASSERT("once_args: argument correct", once_arg_val == 99);
  ASSERT("once_args: listener count is 0 after emit (on ee_once)", ee_once.listeners() == 0);
  ee_once.emit("event_once_args", 101);
  ASSERT("once_args: listener not called on second emit", tracker_once_args.count() == 1);

  /// - Test #9: 'off()' (remove all listeners)
  EventEmitter ee_off_all; CallbackTracker tracker_oa1, tracker_oa2, tracker_oa_once;
  ee_off_all.on("off_all_1", [&](){ tracker_oa1.trigger(); });
  ee_off_all.on("off_all_2", [&](){ tracker_oa2.trigger(); });
  ee_off_all.once("off_all_once", [&](){ tracker_oa_once.trigger(); });
  ASSERT("off_all: initial listener count is 3", ee_off_all.listeners() == 3);
  ee_off_all.off();
  ASSERT("off_all: listener count is 0 after off()", ee_off_all.listeners() == 0);
  ee_off_all.emit("off_all_1"); ee_off_all.emit("off_all_2"); ee_off_all.emit("off_all_once");
  ASSERT("off_all: listener 1 not called after off()", tracker_oa1.count() == 0);
  ASSERT("off_all: listener 2 not called after off()", tracker_oa2.count() == 0);
  ASSERT("off_all: once listener not called after off()", tracker_oa_once.count() == 0);

  /// - Test #10: maxListeners warning
  EventEmitter ee_max; ee_max.maxListeners = 2; CallbackTracker tracker_max_cb;
  std::stringstream captured_cout;
  {
    StreamRedirector redirect(std::cout, captured_cout.rdbuf());
    ee_max.on("max_event1", [&](){ tracker_max_cb.trigger(); });
    ee_max.on("max_event2", [&](){ tracker_max_cb.trigger(); });
    ee_max.on("max_event3", [&](){ tracker_max_cb.trigger(); });
  }
  std::string cout_output = captured_cout.str();
  ASSERT("maxListeners: warning message present in cout", cout_output.find("warning: possible EventEmitter memory leak detected") != std::string::npos);
  ASSERT("maxListeners: warning shows correct listener count (3)", cout_output.find("3 listeners added") != std::string::npos);
  ASSERT("maxListeners: warning shows correct maxListeners (2)", cout_output.find("max is 2") != std::string::npos);
  ASSERT("maxListeners: listener count is 3 after additions", ee_max.listeners() == 3);
  ee_max.emit("max_event1");
  ee_max.emit("max_event2");
  ee_max.emit("max_event3");
  ASSERT("maxListeners: all 3 listeners functional despite warning", tracker_max_cb.count() == 3);

  /// - Test #11: Complex argument types
  EventEmitter ee_complex; CallbackTracker tracker_complex; ComplexArg received_arg;
  ComplexArg sent_arg = {123, "test_data"};
  ee_complex.on("complex_event", [&](const ComplexArg& ca) {
    tracker_complex.trigger();
    received_arg = ca;
  });
  ee_complex.emit("complex_event", sent_arg);
  ASSERT("Complex Arg: listener called", tracker_complex.count() == 1);
  ASSERT("Complex Arg: argument received correctly", received_arg == sent_arg);

  /// - Test #12: Using a functor as a callback
  EventEmitter ee_functor; CallbackTracker tracker_functor;
  TestFunctor my_functor_instance(tracker_functor);
  ee_functor.on("functor_event_no_args", [&]() { my_functor_instance(); });
  ee_functor.emit("functor_event_no_args");
  ASSERT("Functor (no-args lambda): callback called", tracker_functor.count() == 1);

  tracker_functor.reset();
  ee_functor.on("functor_event_with_args", [&](int val, const std::string& str) { my_functor_instance(val, str); });
  ee_functor.emit("functor_event_with_args", 1, std::string("test"));
  ASSERT("Functor (with_args lambda): callback called", tracker_functor.count() == 1);

  /// - Test #13: Using a free function as a callback
  EventEmitter ee_free_func; CallbackTracker tracker_free_func;
  ee_free_func.on("free_func_event", [&](int val) { example_free_function(tracker_free_func, val); });
  ee_free_func.emit("free_func_event", 50);
  ASSERT("Free function (wrapped): callback called", tracker_free_func.count() == 1);

  /// - Test #14: Callback returning a value
  EventEmitter ee_return; CallbackTracker tracker_return_val_cb; return_value_callback_side_effect = 0;
  ee_return.on("return_event", [&](int x) {
    callback_returning_int(x);
    tracker_return_val_cb.trigger();
  });
  ee_return.emit("return_event", 7);
  ASSERT("Return value: callback was called", tracker_return_val_cb.count() == 1);
  ASSERT("Return value: internal side effect of callback function correct", return_value_callback_side_effect == 70);

  /// - Test #15: Emit with argument type mismatch
  EventEmitter ee_mismatch; CallbackTracker tracker_mismatch;
  ee_mismatch.on("mismatch_event", [&](int /*i*/) { tracker_mismatch.trigger(); });
  std::stringstream captured_cerr_mismatch;
  {
    StreamRedirector redirect(std::cerr, captured_cerr_mismatch.rdbuf());
    ee_mismatch.emit("mismatch_event", "this is not an int");
  }
  ASSERT("Type Mismatch: listener NOT called", tracker_mismatch.count() == 0);
  std::string cerr_output_mismatch = captured_cerr_mismatch.str();
  ASSERT("Type Mismatch: error message present in cerr", cerr_output_mismatch.find("Callback signature mismatch") != std::string::npos);

  /// - Test #16: Modifying emitter from within a callback (removing self via 'off')
  EventEmitter ee_modify_self_off; CallbackTracker tracker_mod_self_off;
  ee_modify_self_off.on("mod_self_off", [&]() {
    tracker_mod_self_off.trigger();
    ee_modify_self_off.off("mod_self_off");
  });
  ee_modify_self_off.emit("mod_self_off");
  ASSERT("Modify self (off): called once", tracker_mod_self_off.count() == 1);
  ee_modify_self_off.emit("mod_self_off");
  ASSERT("Modify self (off): not called after self-removal", tracker_mod_self_off.count() == 1);
  ASSERT("Modify self (off): listener count is 0", ee_modify_self_off.listeners() == 0);

  /// - Test #17: Modifying emitter from within a 'once' callback
  EventEmitter ee_modify_self_once; CallbackTracker tracker_mod_self_once;
  ee_modify_self_once.once("mod_self_once", [&]() {
    tracker_mod_self_once.trigger();
  });
  ee_modify_self_once.emit("mod_self_once");
  ASSERT("Modify self (once): called once", tracker_mod_self_once.count() == 1);
  ee_modify_self_once.emit("mod_self_once");
  ASSERT("Modify self (once): not called again", tracker_mod_self_once.count() == 1);
  ASSERT("Modify self (once): listener count is 0", ee_modify_self_once.listeners() == 0);

  /// - Test #18: Modifying emitter from within a callback (adding another listener)
  EventEmitter ee_modify_other; CallbackTracker tracker_mod_other1, tracker_mod_other2;
  ee_modify_other.on("mod_add", [&]() {
    tracker_mod_other1.trigger();
    if (tracker_mod_other1.count() == 1) {
      ee_modify_other.on("mod_added_event", [&]() { tracker_mod_other2.trigger(); });
    }
  });
  ee_modify_other.emit("mod_add");
  ASSERT("Modify other (add): first listener called", tracker_mod_other1.count() == 1);
  ASSERT("Modify other (add): listener count is 2 after add", ee_modify_other.listeners() == 2);
  ee_modify_other.emit("mod_added_event");
  ASSERT("Modify other (add): newly added listener called", tracker_mod_other2.count() == 1);

  /// - Test #19: `once` then `on` with same event name
  EventEmitter ee_once_on_combo;
  CallbackTracker tracker_oo_once, tracker_oo_on;
  ee_once_on_combo.once("combo_event", [&](){ tracker_oo_once.trigger(); });
  ee_once_on_combo.on("combo_event", [&](){ tracker_oo_on.trigger(); });
  ASSERT("Once then On combo: Listener count is 2", ee_once_on_combo.listeners() == 2);
  ee_once_on_combo.emit("combo_event");
  ASSERT("Once then On combo: 'once' listener called", tracker_oo_once.count() == 1);
  ASSERT("Once then On combo: 'on' listener called", tracker_oo_on.count() == 1);
  ASSERT("Once then On combo: Listener count is 1 after emit (once removed, on remains)", ee_once_on_combo.listeners() == 1);
  tracker_oo_once.reset(); tracker_oo_on.reset();
  ee_once_on_combo.emit("combo_event");
  ASSERT("Once then On combo: 'once' listener NOT called on second emit", tracker_oo_once.count() == 0);
  ASSERT("Once then On combo: 'on' listener called on second emit", tracker_oo_on.count() == 1);

  /// - Test #20: `on` then `once` with same event name
  EventEmitter ee_on_once_combo;
  CallbackTracker tracker_oo2_on, tracker_oo2_once;
  ee_on_once_combo.on("combo_event2", [&](){ tracker_oo2_on.trigger(); });
  ee_on_once_combo.once("combo_event2", [&](){ tracker_oo2_once.trigger(); });
  ASSERT("On then Once combo: Listener count is 2", ee_on_once_combo.listeners() == 2);
  ee_on_once_combo.emit("combo_event2");
  ASSERT("On then Once combo: 'on' listener called", tracker_oo2_on.count() == 1);
  ASSERT("On then Once combo: 'once' listener called", tracker_oo2_once.count() == 1);
  ASSERT("On then Once combo: Listener count is 1 after emit", ee_on_once_combo.listeners() == 1);
  tracker_oo2_on.reset(); tracker_oo2_once.reset();
  ee_on_once_combo.emit("combo_event2");
  ASSERT("On then Once combo: 'on' listener called on second emit", tracker_oo2_on.count() == 1);
  ASSERT("On then Once combo: 'once' listener NOT called on second emit", tracker_oo2_once.count() == 0);

  /// - Test #21: Async: Concurrent on, once, emit
  std::cout << "\nStarting Test #23: Async Operations...\n";
  EventEmitter ee_async_test;
  const int NUM_ASYNC_THREADS = 10; // Number of concurrent threads
  // Set maxListeners high enough to accommodate all 'on' listeners from this test
  // Each thread adds ASYNC_TEST_ITERATIONS_PER_THREAD 'on' listeners that persist.
  ee_async_test.maxListeners = NUM_ASYNC_THREADS * ASYNC_TEST_ITERATIONS_PER_THREAD + 10;

  // Reset atomic counters for this test
  async_total_on_callbacks_fired = 0;
  async_total_once_callbacks_fired = 0;
  async_total_listeners_registered = 0;

  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_ASYNC_THREADS; ++i) {
      threads.emplace_back(async_worker_function, std::ref(ee_async_test), i);
  }

  for (std::thread& t : threads) {
      if (t.joinable()) {
          t.join();
      }
  }

  int expected_on_callbacks = NUM_ASYNC_THREADS * ASYNC_TEST_ITERATIONS_PER_THREAD;
  int expected_once_callbacks = NUM_ASYNC_THREADS * ASYNC_TEST_ITERATIONS_PER_THREAD;
  // Each 'on' listener remains, each 'once' listener is removed after firing.
  int expected_final_listeners = NUM_ASYNC_THREADS * ASYNC_TEST_ITERATIONS_PER_THREAD;
  int expected_total_registrations = NUM_ASYNC_THREADS * ASYNC_TEST_ITERATIONS_PER_THREAD * 2;

  ASSERT("Async Test: Total 'on' callbacks fired correctly", async_total_on_callbacks_fired.load() == expected_on_callbacks);
  ASSERT("Async Test: Total 'once' callbacks fired correctly", async_total_once_callbacks_fired.load() == expected_once_callbacks);
  ASSERT("Async Test: Total listeners registered (sanity check)", async_total_listeners_registered.load() == expected_total_registrations);
  ASSERT("Async Test: Final listener count in emitter correct", ee_async_test.listeners() == expected_final_listeners);
  std::cout << "...Finished Test #23: Async Operations.\n";

  std::cout << "\nSummary\n-------" << std::endl;
  std::cout << "Total Assertions Run: " << assertions_run << std::endl;
  std::cout << "Assertions Passed:  " << assertions_passed << std::endl;
  std::cout << "Assertions Failed:  " << (assertions_run - assertions_passed) << std::endl;

  if (assertions_passed == assertions_run) {
    std::cout << "\nOK!" << std::endl;
    return 0;
  } else {
    std::cout << "\nFAILED!" << std::endl;
    return 1;
  }
}

