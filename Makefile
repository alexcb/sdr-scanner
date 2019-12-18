CC=gcc
CCFLAGS=-std=gnu11 -Wuninitialized -Wall -Werror -Wno-unused-label `pkg-config --cflags ayatana-appindicator3-0.1`
LDFLAGS=-lm -lpthread -lrtlsdr -lpulse -lpulse-simple -lncurses `pkg-config --libs ayatana-appindicator3-0.1`

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

