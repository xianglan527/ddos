MKDIR = mkdir -p

APP_CC     = gcc
APP_CFLAGS = -Wall -O2 -g
APP_DEFS   =

APP_SRCS_PATH =
APP_INCLUDE   =

include ping/Makefile
# include ftp/Makefile

APP_SRCS := $(foreach dir, $(APP_SRCS_PATH), $(wildcard $(dir)/*.c))


define APP_compile_c_file
	$(APP_CC) $(APP_CFLAGS) $(APP_DEFS) $(APP_INCLUDE) \
	-c -o $(1:.c=.o) $(1)
endef

APP_compile_c: $(APP_SRCS)
	$(foreach src, $(APP_SRCS), $(call APP_compile_c_file,$(src));)


define APP_link_program
$(1)/$(notdir $(1)): $(wildcard $(1)/*.o)
	$(APP_CC) -o $$@ $$^
endef


APP_link_all: APP_compile_c \
              $(foreach dir,$(APP_SRCS_PATH),$(dir)/$(notdir $(dir)))

$(foreach dir,$(APP_SRCS_PATH),$(eval $(call APP_link_program,$(dir))))


clean:
	@$(foreach dir, $(APP_SRCS_PATH), \
		rm -f $(dir)/*.o; \
		rm -f $(dir)/$(notdir $(dir)); \
	)


app: clean APP_link_all
	@echo "Done building app commands."

