TARGET_EXEC ?= vulkan_renderer

CXX	?=	gcc

BUILD_DIR := build
SRC_DIRS := src

SRCS := $(shell find $(SRC_DIRS) -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

INC_DIRS := $(SRC_DIRS)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS := $(INC_FLAGS) -MMD -MP
CPPFLAGS += -DLOG_USE_COLOR

CFLAGS := -Wall
CFLAGS += -Wextra
CFLAGS += -Wconversion
CFLAGS += -std=c17
CFLAGS += -Wp,-U_FORTIFY_SOURCE
CFLAGS += -Wformat=2
CFLAGS += -MMD -MP
CFLAGS += -fanalyzer
CFLAGS += -pipe
CFLAGS += -march=native -mtune=native
CFLAGS += -Wcast-qual
CFLAGS += -Wconversion
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wduplicated-branches
CFLAGS += -Wduplicated-cond
CFLAGS += -Werror=return-type
CFLAGS += -Werror=vla-larger-than=0
CFLAGS += -Winit-self
CFLAGS += -Winline
CFLAGS += -Wlogical-op
CFLAGS += -Wredundant-decls
CFLAGS += -Wshadow
CFLAGS += -Wsuggest-attribute=pure
CFLAGS += -Wsuggest-attribute=const
CFLAGS += -Wundef
CFLAGS += -Wunreachable-code
CFLAGS += -Wwrite-strings
CFLAGS += -Wno-missing-field-initializers

LDFLAGS	:= -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

ifeq ($(DEBUG), 1)
	CFLAGS	+=	-O0 -ggdb
else
	CFLAGS	+=	-O3 -flto
	LDFLAGS	+=	-s -flto -O3
endif

ifeq ($(ASAN), 1)
	CFLAGS	+=	-fsanitize=address,leak,undefined
	LDFLAGS	+=	-lasan -lubsan -fsanitize=address,leak,undefined
endif

$(TARGET_EXEC): $(BUILD_DIR)/$(TARGET_EXEC)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: fclean
fclean: clean
	rm -f $(TARGET_EXEC)

.PHONY: re
re: fclean
	$(MAKE) $(TARGET_EXEC)

.PHONY: all
all: $(TARGET_EXEC)

-include $(DEPS)
