CC = gcc
CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -Wno-unused-function -fopenmp
LIBS = -lm -lpthread -fopenmp

OBJS = whisper.o ggml.o tokenizer.o main.o

all: whisper

whisper: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o whisper $(LIBS)
	@echo ""
	@echo "Built skeleton implementation: ./whisper"
	@echo "Full implementation available: ./whisper-full"
	@echo ""

%.o: %.c whisper.h ggml.h tokenizer.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o whisper

test-skeleton:
	@echo "Testing skeleton implementation (limited output):"
	./whisper ~/models/ggml-base.en.bin ../whisper_clome/jfk.wav

test-full:
	@echo "Testing full implementation:"
	./whisper-full -m ~/models/ggml-base.en.bin -f ../whisper_clome/jfk.wav

test: test-full

.PHONY: clean test test-skeleton test-full
