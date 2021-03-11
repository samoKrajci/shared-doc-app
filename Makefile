CC=g++
CXXFLAGS= \
	-Wall \
	-Wextra \
	-pedantic \
	-g \
	-std=c++17 \
	-MMD \
	-L /usr/lib/ \
	-lboost_system \
	-lboost_thread \
	-lpthread \
	-lncurses

SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
DEPENDS=$(SOURCES:.cpp=.d)

.PHONY: all clean distclean check

all: client server

$(BINARY): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(CXXFLAGS)

server: document.o server.o
	$(CXX) document.o server.o -o $@ $(CXXFLAGS) 

client: document.o client.o
	$(CXX) document.o client.o -o $@ $(CXXFLAGS) 

-include $(DEPENDS)

clean:
	$(RM) $(OBJECTS) $(DEPENDS)

distclean: clean
	$(RM) $(BINARY)
