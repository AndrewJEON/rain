CC = g++
CFLAGS = -Wall -Wextra -g -rdynamic -std=c++0x
LIBS = -pthread
CMD = $(CC) $(CFLAGS)

NAME = inbalanced_sum
ODIR = ./obj/
SRCDIR = ./src/
SOURCES = $(shell ls $(SRCDIR)*.c)
OBJECTS = $(SOURCES:$(SRCDIR)%.c=$(ODIR)%.o)

all: librain.so many_threads_unbalanced inbalanced_sum deadlock deadlock2 self_deadlock

many_threads_unbalanced: many_threads_unbalanced.o librain.so
	$(CMD) $^ -o $@ $(LIBS) $(INCLUDE)

many_threads_unbalanced.o: many_threads_unbalanced.c
	$(CMD) -c $< -o $@ $(LIBS) $(INCLUDE)

inbalanced_sum: inbalanced_sum.o librain.so
	$(CMD) $^ -o $@ $(LIBS) $(INCLUDE)

inbalanced_sum.o: inbalanced_sum.c
	$(CMD) -c $< -o $@ $(LIBS) $(INCLUDE)

deadlock: deadlock.o librain.so
	$(CMD) $^ -o $@ $(LIBS) $(INCLUDE)

deadlock.o: deadlock.c
	$(CMD) -c $< -o $@ $(LIBS) $(INCLUDE)

deadlock2: deadlock2.o librain.so
	$(CMD) $^ -o $@ $(LIBS) $(INCLUDE)

deadlock2.o: deadlock2.c
	$(CMD) -c $< -o $@ $(LIBS) $(INCLUDE)

self_deadlock: self_deadlock.o librain.so
	$(CMD) $^ -o $@ $(LIBS) $(INCLUDE)

self_deadlock.o: self_deadlock.c
	$(CMD) -c $< -o $@ $(LIBS) $(INCLUDE)

librain.so: librain.cpp
	g++ -std=c++0x -Wall -shared -fPIC librain.cpp -o librain.so -ldl -pthread

clean:
	rm -f many_threads_unbalanced many_threads_unbalanced.o inbalanced_sum inbalanced_sum.o deadlock deadlock.o librain.so

new:
	make clean
	make all
