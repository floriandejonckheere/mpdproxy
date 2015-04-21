CC	:= gcc
CFLAGS	:= -fPIC -Wall -Werror -Wconversion -O3 -g
LDFLAGS	:= -lpthread

EXEC	:= mpdproxy
SOURCES	:= $(wildcard *.c)
OBJECTS	:= $(SOURCES:.c=.o)

all: $(EXEC)

.PHONY: all install

$(EXEC): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXEC) -Wl,$(shell echo "${LDFLAGS}" | sed -e 's/ /,/g')

%.o: %.cpp
	$(CC) -c $(CCFLAGS) $< -o $@

install:
	cp mpdproxy /usr/bin/mpdproxy
	cp mpdproxy.conf /etc/mpdproxy.conf

clean:
	$(RM) $(EXEC) $(OBJECTS)
