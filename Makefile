CXXFLAGS = -Wall -O2 -std=c++11 -pthread
CC = g++

all: bin/server bin/client

bin/%: %.o helper.o
	$(CC) $(CXXFLAGS) -o $@ $< helper.o

helper.o: helper.h helper.cpp
	$(CC) $(CXXFLAGS) -c helper.cpp -o $@

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm *.o bin/*
