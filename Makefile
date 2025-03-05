CXX = g++
CXXFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

server: server.cpp helper.h
	$(CXX) $(CXXFLAGS) -o server server.cpp

subscriber: subscriber.cpp helper.h
	$(CXX) $(CXXFLAGS) -o subscriber subscriber.cpp

.PHONY: clean

clean:
	rm -rf server subscriber *.o
