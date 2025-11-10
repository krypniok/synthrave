# Simple Makefile-based build for Synthrave

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror=return-type -pedantic -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?= -lopenal -lm

BUILD_DIR ?= build
TARGET ?= synthrave
BINARY := $(BUILD_DIR)/$(TARGET)

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))

REMOTE ?= origin
REPO_NAME ?= synthrave
VISIBILITY ?= public

.PHONY: all run clean push repo

all: $(BINARY)

run: $(BINARY)
	$(BINARY)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BINARY): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)

push:
	@branch=$$(git rev-parse --abbrev-ref HEAD); \
	if [ -z "$$branch" ]; then \
		echo "Could not determine current branch" >&2; exit 1; \
	fi; \
	echo "Pushing $$branch to $(REMOTE)…"; \
	git push $(REMOTE) $$branch

repo:
	@if [ -z "$(REPO_NAME)" ]; then \
		echo "Set REPO_NAME, e.g. make repo REPO_NAME=myuser/synthrave" >&2; \
		exit 1; \
	fi
	gh repo create $(REPO_NAME) --source=. --remote=$(REMOTE) --push --$(VISIBILITY)
