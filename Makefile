VERSION = 0.0
CC = gcc
CFLAGS = -Wall -g3 -DVERSION=\"$(VERSION)\"
LDFLAGS = -lm -pthread 
BIN = pthread

OBJ = main.o dllist.o pthread_worker.o

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -rf $(BIN) $(OBJ)

