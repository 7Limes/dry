CC := gcc
CFLAGS ?= -O2 -Wall
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

SRCS := \
	src/main.c src/globals.c \
	src/util/util.c src/util/da.c src/util/map.c \
	src/frontend/lexer.c src/frontend/parser.c \
	src/stdlib/stdlib.c \
	src/compiler/compiler.c src/compiler/codegen.c

OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET := $(BUILD_DIR)/dry

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean