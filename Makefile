SERVER_EXEC ?= jobExecutorServer
CLIENT_EXEC ?= jobCommander

BUILD_DIR ?= build
SRC_DIRS ?= .

CC := $(shell \
	for compiler in cc gcc clang; \
	do \
		if command -v $$compiler; then \
			break;\
		fi \
	done \
)

WARNINGS := -Wall -Wextra -Wconversion
DEBUG := # -fsanitize=thread

OPTIMIZE := -O2
# If the code is underperforming, try recompiling it with some of these flags:
# -falign-functions=32 -falign-loops=32 -march=native -mtune=native

LFLAGS := # -Wl,--gc-sections
LIBS := #-lm

SRCS := $(shell find $(SRC_DIRS) -name *.c)

S_SRCS := $(filter-out ./src/jobCommander.c, $(SRCS))
S_OBJS := $(S_SRCS:%=$(BUILD_DIR)/%.o)
S_DEPS := $(S_OBJS:.o=.d)

C_SRCS := $(filter-out ./src/jobExecutorServer.c, $(SRCS))
C_OBJS := $(C_SRCS:%=$(BUILD_DIR)/%.o)
C_DEPS := $(C_OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS := $(filter-out ./.git/%, $(INC_DIRS))
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP

all: $(SERVER_EXEC) $(CLIENT_EXEC)

$(SERVER_EXEC): $(S_OBJS) Makefile ./include/configuration.h ./include/configuration_adv.h cleantmp
	$(CC) $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(LFLAGS) $(S_OBJS) -o bin/$@ $(LDFLAGS) $(LIBS)

$(CLIENT_EXEC): $(C_OBJS) Makefile ./include/configuration.h ./include/configuration_adv.h cleantmp
	$(CC) $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(LFLAGS) $(C_OBJS) -o bin/$@ $(LDFLAGS) $(LIBS)

# c source
$(BUILD_DIR)/%.c.o: %.c Makefile ./include/configuration.h ./include/configuration_adv.h cleantmp
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(WARNINGS) $(DEBUG) $(OPTIMIZE) -c $< -o $@

cleantmp:
	rm -f handshake
	rm -f pipes/*
	rm -f jobExecutorServer.txt
	rm -f vgcore*
	rm -f ./*.output

.PHONY: clean

clean: cleantmp
	$(RM) -r $(BUILD_DIR) bin/$(SERVER_EXEC) bin/$(CLIENT_EXEC)

-include $(DEPS)

MKDIR_P ?= mkdir -p
