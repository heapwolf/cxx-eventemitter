.PHONY: test

all:
	g++ -o test ./test.cc -std=c++1y

test:
	./test

