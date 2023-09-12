CC=g++
CFLAGS=-std=c++11 -c -Wall
LDFLAGS=-pthread
SOURCES=main.cpp led.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=andon

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o *~ $(EXECUTABLE)
