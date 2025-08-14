CXX=clang++
CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

all: test_blockqueue

test_blockqueue: test_blockqueue.cpp BlockQueueLemma33.h
	$(CXX) $(CXXFLAGS) test_blockqueue.cpp -o test_blockqueue

clean:
	rm -f test_blockqueue
