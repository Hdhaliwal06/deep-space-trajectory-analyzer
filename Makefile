CC = gcc
CFLAGS = -Wall -Wextra -std=c11

voyager: src/main.c
	$(CC) $(CFLAGS) src/main.c -o voyager -lm

clean:
	rm -f voyager
