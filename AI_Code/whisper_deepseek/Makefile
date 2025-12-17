CC = gcc
CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -Wno-unused-function
LIBS = -lm -lpthread

OBJS = whisper.o ggml.o tokenizer.o main.o

all: whisper

whisper: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o whisper $(LIBS)

%.o: %.c whisper.h ggml.h tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o whisper

test:
	./whisper models/tiny.bin test.wav

.PHONY: clean test
