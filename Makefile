# tool macros
CC := gcc
CCFLAGS := -O0
DBGFLAGS := -g

# compile macros
TARGET_NAME := challenge
TARGET := $(TARGET_NAME)
TARGET_DEBUG := $(TARGET_NAME)_DEBUG
COMPILE_COMMAND := $(CC) $(CCFLAGS) $(DBGFLAGS) -o $(TARGET_NAME)  $(addsuffix .c, $(TARGET_NAME))

# default rule
default: $(TARGET)

# non-phony targets
$(TARGET): FORCE
	$(CC) $(CCFLAGS) -o $@  $(addsuffix .c, $(TARGET_NAME))

$(TARGET_DEBUG)_DEBUG:
	$(CC) $(CCFLAGS) $(DBGFLAGS) -o $(TARGET_NAME) $(addsuffix .c, $(TARGET_NAME))

.PHONY: all
all: default

.PHONY: $(TARGET)
debug: $(TARGET_DEBUG)

FORCE: ;

