CC = gcc
CFLAGS = -Wall -Wextra -std=c11
PYTHON = python3

.PHONY: clean update-data

voyager: main.c
	$(CC) $(CFLAGS) main.c -o voyager -lm

update-data:
	$(PYTHON) scripts/update_data.py

clean:
	rm -f voyager
