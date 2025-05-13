CXX = clang++
STD_VERSION = -std=c++2a

COMMON_FLAGS = -Wall -Wextra -I..

TEST_DEBUG_FLAGS = -g
TEST_SANITIZE_FLAGS = -fsanitize=address -fno-omit-frame-pointer
TEST_OPTIMIZE_FLAGS = -O1
TEST_CXXFLAGS = $(STD_VERSION) $(COMMON_FLAGS) $(TEST_DEBUG_FLAGS) $(TEST_SANITIZE_FLAGS) $(TEST_OPTIMIZE_FLAGS)
TEST_LDFLAGS = $(TEST_SANITIZE_FLAGS) # Sanitizer flags often needed at link time too
TEST_LIBS = -pthread # For std::thread, std::atomic

# Performance-specific flags
PERF_OPTIMIZE_FLAGS = -O3 -DNDEBUG # -DNDEBUG to disable asserts and other debug code
PERF_CXXFLAGS = $(STD_VERSION) $(COMMON_FLAGS) $(PERF_OPTIMIZE_FLAGS)
PERF_LDFLAGS =
PERF_LIBS = -pthread

TEST_RUNNER = test_runner
PERF_RUNNER = perf_runner

TEST_SOURCES = test/index.cxx
PERF_SOURCES = perf/index.cxx # Assuming your perf tests are here

.PHONY: all test perf clean

all: test perf

### Build and run tests
test: $(TEST_RUNNER)
	@echo "Running tests..."
	./$(TEST_RUNNER)

### Build the test runner
$(TEST_RUNNER): $(TEST_SOURCES) index.hxx
	@echo "Building test runner..."
	$(CXX) $(TEST_CXXFLAGS) $(TEST_SOURCES) -o $(TEST_RUNNER) $(TEST_LDFLAGS) $(TEST_LIBS)

### Build and run performance benchmarks
perf: $(PERF_RUNNER)
	@echo "Running performance benchmarks..."
	./$(PERF_RUNNER)

### Build the performance runner
$(PERF_RUNNER): $(PERF_SOURCES) index.hxx
	@echo "Building performance runner..."
	$(CXX) $(PERF_CXXFLAGS) $(PERF_SOURCES) -o $(PERF_RUNNER) $(PERF_LDFLAGS) $(PERF_LIBS)

### Clean up build artifacts
clean:
	@echo "Cleaning up..."
	rm -f $(TEST_RUNNER) $(PERF_RUNNER)
	# Add any other object files or build artifacts if necessary: rm -f *.o

### Usage:
# make          -> builds and runs tests, then builds and runs perf benchmarks
# make test     -> builds and runs only tests
# make perf     -> builds and runs only perf benchmarks
# make clean    -> removes executables

