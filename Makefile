CC = g++
CFLAGS = -Wall -Wextra -shared -fPIC -std=c++11
LIBS = -ldl -pthread
CMD = $(CC) $(CFLAGS)

all: librain.so rain_tests

librain.so: librain.cpp
	$(CMD) librain.cpp -o librain.so $(LIBS)

rain_tests:
	$(MAKE) -C ./tests/

clean:
	rm -f librain.so
	$(MAKE) -C ./tests/ clean

new:
	make clean
	make all
