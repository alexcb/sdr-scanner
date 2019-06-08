CC=gcc
CCFLAGS=-std=gnu11 -Wuninitialized -Wall -Werror -Wno-unused-label
LDFLAGS=-lm -lpthread -lrtlsdr -lpulse -lpulse-simple

SERVERSRC=$(wildcard src/*.c)
SERVEROBJ=$(SERVERSRC:%.c=%.o)

all: scanner

scanner: $(SERVEROBJ)
	$(CC) $(CCFLAGS) -o scanner $(SERVEROBJ) $(LDFLAGS)

.PHONY: reformat
reformat:
	find -regex '.*/.*\.\(c\|h\)$$' -exec clang-format-7 -i {} \;

# To obtain object files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f scanner $(SERVEROBJ)

