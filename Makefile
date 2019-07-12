TARGET_EXEC ?= ccc

BUILD_DIR ?= ./build
SRC_DIR ?= ./src

SRCS := $(shell find $(SRC_DIR) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS ?= -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(BUILD_DIR)/$(TARGET_EXEC)
	./test/test.sh

.PHONY: style
style:
	clang-format -i $(SRC_DIR)/*.c $(SRC_DIR)/*.h

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)
