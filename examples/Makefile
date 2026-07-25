# Minimal example: build a ctt test suite with plain Make (no CMake).
#
#   make          build and run the example suite
#   make clean    remove the binary
#
# ctt is a header plus a tiny implementation stub, so a suite is just your
# test file + ctt's src/ctt.c, with -I pointing at ctt's include/.
#
# Sanitizers are on by default for leak/UB detection. If the sanitizer runtime
# isn't installed, disable them with:  make SAN=

CC     := cc
SAN    ?= -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS := -I../include -g -Wall -Wextra $(SAN)
BIN    := example

$(BIN): example_tests.c ../src/ctt.c ../include/ctt.h
	$(CC) $(CFLAGS) example_tests.c ../src/ctt.c -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: run clean
.DEFAULT_GOAL := run
