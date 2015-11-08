CC=g++
CFLAGS= -Wall -Wextra -g -rdynamic -pthread -std=c++11
LDFLAGS=

SOURCES=$(wildcard *.c)
NAMES=$(SOURCES:%.c=%)

# use built-in default rules
all: $(NAMES)

clean:
	rm -f $(NAMES)

new:
	make clean
	make all
