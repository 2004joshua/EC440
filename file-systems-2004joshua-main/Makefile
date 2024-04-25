CC = gcc
CFLAGS = -Wall -Werror -std=gnu99 -pedantic -g

TESTS = test_8_9 test_8_10 test_9_3 test_9_6 test_9_7
OBJECTS = $(TESTS:=.o)
EXECUTABLES = $(TESTS)

all: $(EXECUTABLES)

$(EXECUTABLES): %: %.o fs.o disk.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

fs.o: fs.c fs.h
	$(CC) $(CFLAGS) -c $<

disk.o: disk.c disk.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) $(EXECUTABLES) fs.o disk.o
