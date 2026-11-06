# Host-side unit tests for the portable application code.
#   make -C tests          build and run
#   make -C tests clean

CC      ?= cc
CFLAGS  += -std=c11 -Wall -Wextra -Werror -g -O1 \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -I../firmware/app
LDFLAGS += -fsanitize=address,undefined

APP_SRC  = ../firmware/app/program.c ../firmware/app/player.c \
           ../firmware/app/buttons.c ../firmware/app/app.c
TEST_SRC = test_main.c test_program.c test_player.c test_app.c

run: test_runner
	./test_runner

test_runner: $(APP_SRC) $(TEST_SRC) $(wildcard *.h ../firmware/app/*.h)
	$(CC) $(CFLAGS) -o $@ $(APP_SRC) $(TEST_SRC) $(LDFLAGS)

clean:
	rm -f test_runner

.PHONY: run clean
