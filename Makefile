CC=gcc
CFLAGS=-Wall -g
LDFLAGS=
SOURCES=server.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=myapp

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)