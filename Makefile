CXXFLAGS = -Wall -O2 -std=c++11 -pthread
CC = g++

all: bin/server bin/client

bin/%: %.o helper.o
	$(CC) $(CXXFLAGS) -o $@ $< helper.o

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm *.o bin/*
