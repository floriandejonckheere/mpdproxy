CC	:= gcc
#~ CFLAGS	:= -fPIC -O3 -Wall -Werror
CFLAGS	:= -g -fPIC -Wall -Werror -pthread 
LDFLAGS	:= --wrap=pthread_exit

EXEC	:= mpdproxy
SOURCES	:= $(wildcard *.c)
OBJECTS	:= $(SOURCES:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXEC) -Wl,$(shell echo "${LDFLAGS}" | sed -e 's/ /,/g')

%.o: %.cpp
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	$(RM) $(EXEC) $(OBJECTS)
