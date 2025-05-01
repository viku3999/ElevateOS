LIB_DIRS = 
CC = g++
CFLAGS = -std=c++23 -Wall -Werror -pedantic -pthread
LIBS = -lrt

# Include both .hpp and .h files
HFILES = $(wildcard *.hpp) $(wildcard *.h)

# Include both .cpp and .c files
CPPFILES = $(wildcard *.cpp)
CFILES = $(wildcard *.c)

# Generate object files for both .cpp and .c files
OBJS = $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)

# Target to build the final executable
all: Sequencer

# Rule to link object files into the final executable
Sequencer: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

# Rule to compile .cpp files into .o files
%.o: %.cpp $(HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile .c files into .o files
%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	-rm -f *.o Sequencer