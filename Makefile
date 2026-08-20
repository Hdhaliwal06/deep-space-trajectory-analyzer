CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
PYTHON = python3
BUILD_DIR = build
BINARY = $(BUILD_DIR)/voyager

.PHONY: all clean update-data test web

all: $(BINARY)

$(BINARY): main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) main.c -o $(BINARY) -lm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

update-data:
	$(PYTHON) scripts/update_data.py

test: $(BINARY)
	$(PYTHON) -m unittest discover -s tests -v

web: $(BINARY)
	$(PYTHON) scripts/web.py

clean:
	rm -rf $(BUILD_DIR)
