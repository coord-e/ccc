BUILD_DIR ?= ./build
SRC_DIR ?= ./src

CFLAGS ?= -Wall -std=c11 -pedantic
CPPFLAGS ?= -MMD -MP

DEBUG ?= 1
ifeq ($(DEBUG), 1)
  CC = clang
  CFLAGS += -O0 -g3 -fsanitize=memory -fPIE
  LDFLAGS += -fsanitize=memory
  OBJ_SUFFIX = .debug
else
  CFLAGS += -O3
  OBJ_SUFFIX =
endif

TARGET_EXEC ?= ccc$(OBJ_SUFFIX)

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%$(OBJ_SUFFIX).o)
DEPS := $(OBJS:.o=.d)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c$(OBJ_SUFFIX).o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(BUILD_DIR)/$(TARGET_EXEC)
	./test/test.sh $(TARGET_EXEC)

.PHONY: style
style:
	clang-format -i $(SRC_DIR)/*.c $(SRC_DIR)/*.h

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)
