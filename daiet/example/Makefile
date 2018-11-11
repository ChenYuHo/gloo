# DAIET project
# author: amedeo.sapio@kaust.edu.sa

# all source are stored in SRCS-y

TARGET = example
CC = gcc

CXXFLAGS += -std=c++11
LDFLAGS += -lstdc++ -pthread -Wl,--whole-archive -ldaiet -ldpdk -Wl,--no-whole-archive -ldl -lnuma -lboost_chrono -lboost_system -lboost_thread -lboost_program_options -lm
ifeq ($(debug),y)
$(info "Debug")
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O3
endif

.PHONY: default all clean

default: $(TARGET)

all: default

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
HEADERS = $(wildcard *.hpp)

%.o: %.cpp $(HEADERS)
	$(CC) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LDFLAGS) -o $@
	-rm -f *.o

clean:
	-rm -f *.o
	-rm -f $(TARGET)
