CC=gcc
CFLAGS=-Wall -g  # Add any other flags you might need
SRC=myshell.c myshell_parser.c
OBJ=$(SRC:.c=.o)
TARGET=myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

