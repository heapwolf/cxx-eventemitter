.PHONY: test

all:
	c++ -o test ./test.cc -std=c++1y

test:
	./test

