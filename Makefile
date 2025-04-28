LIB_DIRS = 
CC = g++

CFLAGS = -std=c++23 -Wall -Werror -pedantic -pthread
LIBS = -lrt

HFILES = $(wildcard *.hpp)
CFILES = $(wildcard *.cpp)

OBJS = $(CFILES:.cpp=.o)

# Target to build the final executable
all: Sequencer

# Rule to link object files into the final executable
Sequencer: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

# Rule to compile .cpp files into .o files
%.o: %.cpp $(HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	-rm -f *.o Sequencer