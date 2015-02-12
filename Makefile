CC	:= gcc
CFLAGS	:= -g -fPIC -g -O3 -Wall -Werror
LDFLAGS	:= -pthread

EXEC	:= mpdproxy
SOURCES	:= $(wildcard *.c)
OBJECTS	:= $(SOURCES:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

%.o: %.cpp
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	$(RM) $(EXEC) $(OBJECTS)
