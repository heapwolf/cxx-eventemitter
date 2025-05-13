#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <iomanip>

#include "../index.hxx"

std::atomic<long long> perf_callback_counter(0);

const int NUM_PERF_THREADS = 4;
const int EMITS_PER_THREAD_PERF = 10000;
const int LISTENERS_PER_THREAD_PERF = 5;

void perf_worker(EventEmitter& emitter, int thread_id) {
  // Each thread will work with a set of unique event names
  // to simulate more diverse activity, though they could also share event names.
  for (int i = 0; i < LISTENERS_PER_THREAD_PERF; ++i) {
    std::string event_name = "perf_event_t" + std::to_string(thread_id) + "_l" + std::to_string(i);
    emitter.on(event_name, []() {
      perf_callback_counter++; // Atomically increment the counter
    });
  }

  // Now, each thread emits events that its listeners will catch
  for (int i = 0; i < EMITS_PER_THREAD_PERF; ++i) {
    for (int j = 0; j < LISTENERS_PER_THREAD_PERF; ++j) {
       std::string event_name = "perf_event_t" + std::to_string(thread_id) + "_l" + std::to_string(j);
      emitter.emit(event_name);
    }
  }
}

int main() {
  std::cout << "Starting Performance Test..." << std::endl;

  EventEmitter perf_emitter;
  // Set maxListeners high enough if you add many persistent listeners
  // Each thread adds LISTENERS_PER_THREAD_PERF 'on' listeners.
  perf_emitter.maxListeners = NUM_PERF_THREADS * LISTENERS_PER_THREAD_PERF + 100;

  perf_callback_counter = 0;

  std::vector<std::thread> threads;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Launch threads
  for (int i = 0; i < NUM_PERF_THREADS; ++i) {
    threads.emplace_back(perf_worker, std::ref(perf_emitter), i);
  }

  // Join threads
  for (std::thread& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration_ms = end_time - start_time;

  long long expected_callbacks = (long long)NUM_PERF_THREADS * LISTENERS_PER_THREAD_PERF * EMITS_PER_THREAD_PERF;
  long long actual_callbacks = perf_callback_counter.load();
  double duration_seconds = duration_ms.count() / 1000.0;

  std::cout << "Performance Test Finished." << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  std::cout << std::fixed << std::setprecision(2); // Set precision for floating point output
  std::cout << "Threads:         " << NUM_PERF_THREADS << std::endl;
  std::cout << "Listeners per Thread:  " << LISTENERS_PER_THREAD_PERF << std::endl;
  std::cout << "Emits per Listener/Thread: " << EMITS_PER_THREAD_PERF << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  std::cout << "Total Callbacks Expected: " << expected_callbacks << std::endl;
  std::cout << "Total Callbacks Executed: " << actual_callbacks << std::endl;
  std::cout << "Total Listeners in Emitter: " << perf_emitter.listeners() << std::endl;
  std::cout << "Duration:         " << duration_ms.count() << " ms" << std::endl;

  // Calculate and display throughput
  if (duration_seconds > 0) {
    double callbacks_per_second = actual_callbacks / duration_seconds;
    // Total emits = NUM_PERF_THREADS * EMITS_PER_THREAD_PERF * LISTENERS_PER_THREAD_PERF (same as expected_callbacks in this setup)
    double emits_per_second = ( (double)NUM_PERF_THREADS * EMITS_PER_THREAD_PERF * LISTENERS_PER_THREAD_PERF) / duration_seconds;

    std::cout << "Callbacks per second:   " << callbacks_per_second << std::endl;
    std::cout << "Emits per second:     " << emits_per_second << std::endl;
  } else {
    std::cout << "Duration too short to calculate meaningful throughput." << std::endl;
  }
  std::cout << "----------------------------------------" << std::endl;

  if (actual_callbacks == expected_callbacks) {
    std::cout << "Callback count: CORRECT" << std::endl;
  } else {
    std::cout << "Callback count: INCORRECT" << std::endl;
    std::cerr << "Error: Expected " << expected_callbacks << " callbacks, but got " << actual_callbacks << std::endl;
    return 1;
  }

  return 0;
}

