TARGET_EXEC ?= bf

TARGET_TESTS ?= glados

CCC	?=	gcc

BUILD_DIR := build
BUILD_DIR_TESTS := build_tests
SRC_DIRS := src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

OBJS_TESTS := $(SRCS:%=$(BUILD_DIR_TESTS)/%.o)

DEPS := $(OBJS:.o=.d)

CPPFLAGS := $(INC_FLAGS) -MMD -MP -D_GNU_SOURCE

CFLAGS := -std=c2x

CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Wfloat-equal
CFLAGS += -Wundef
CFLAGS += -Wshadow
CFLAGS += -Wpointer-arith
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wstrict-overflow=5
CFLAGS += -Wwrite-strings
CFLAGS += -Wcast-qual
CFLAGS += -Wswitch-default
CFLAGS += -Wswitch-enum
CFLAGS += -Wconversion
CFLAGS += -Wunreachable-code
CFLAGS += -Wformat=2
CFLAGS += -Werror=vla-larger-than=0
CFLAGS += -Wduplicated-branches
CFLAGS += -Wduplicated-cond
CFLAGS += -Werror-implicit-function-declaration
CFLAGS += -Wdisabled-optimization
CFLAGS += -Werror=return-type
CFLAGS += -Winit-self
CFLAGS += -Winline
CFLAGS += -Wredundant-decls
CFLAGS += -Wno-missing-field-initializers
CFLAGS += -Wlogical-op
CFLAGS += -Wno-format-nonliteral

LDFLAGS	:=

ifeq ($(DEBUG), 1)
CFLAGS += -Og -ggdb3
else
CFLAGS += -O3 -DNDEBUG
LDFLAGS += -s
endif

ifeq ($(LTO), 1)
CFLAGS += -flto
# This will break with DEBUG=1, but who the hell builds with
# LTOs and debug at the same time?
# I could also make it throw an error if DEBUG and LTO are activated
# at the same time but for now this will do
LDFLAGS += -flto -O3
endif

ifeq ($(ASAN), 1)
CFLAGS += -fsanitize=address,leak,undefined
LDFLAGS += -fsanitize=address,leak,undefined
endif

ifeq ($(NATIVE), 1)
CFLAGS += -march=native -mtune=native
endif

ifeq ($(ANALYZER), 1)
CFLAGS += -fanalyzer
endif

.PHONY: all
all: $(TARGET_EXEC)

.PHONY: tests_run
tests_run:
	$(MAKE) $(TARGET_TESTS)
	./$(TARGET_TESTS)

$(TARGET_EXEC): $(BUILD_DIR)/$(TARGET_EXEC)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(TARGET_TESTS): $(BUILD_DIR_TESTS)/$(TARGET_TESTS)
	cp $(BUILD_DIR_TESTS)/$(TARGET_TESTS) $(TARGET_TESTS)

$(BUILD_DIR_TESTS)/$(TARGET_TESTS): CPPFLAGS += -DUNIT_TESTS=1
$(BUILD_DIR_TESTS)/$(TARGET_TESTS): CFLAGS += --coverage
$(BUILD_DIR_TESTS)/$(TARGET_TESTS): LDFLAGS += -lcriterion --coverage
$(BUILD_DIR_TESTS)/$(TARGET_TESTS): $(OBJS_TESTS)
	$(CC) $(OBJS_TESTS) -o $@ $(LDFLAGS)

$(BUILD_DIR_TESTS)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BUILD_DIR_TESTS)

.PHONY: fclean
fclean: clean
	rm -f $(TARGET_EXEC)
	rm -f $(TARGET_TESTS)

.PHONY: re
re: fclean
	$(MAKE) all

.PHONY: wtf
wtf:
	echo | $(CC) -E -dM -

-include $(DEPS)
