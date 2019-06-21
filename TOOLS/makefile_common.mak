ifdef V
Q =
else
Q = @
endif

CFLAGS := -I$(ROOT) -I$(BUILD) $(CFLAGS)

OBJECTS = $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.rc=.o)

TARGET = mpv

# The /./ -> / is for cosmetic reasons.
BUILD_OBJECTS = $(subst /./,/,$(addprefix $(BUILD)/, $(OBJECTS)))

BUILD_TARGET = $(addprefix $(BUILD)/, $(TARGET))$(EXESUF)
BUILD_DEPS = $(BUILD_OBJECTS:.o=.d)
CLEAN_FILES += $(BUILD_OBJECTS) $(BUILD_DEPS) $(BUILD_TARGET)

LOG = $(Q) printf "%s\t%s\n"

# Special rules.

all: $(BUILD_TARGET)

clean:
	$(LOG) "CLEAN"
	$(Q) rm -f $(CLEAN_FILES)
	$(Q) rm -rf $(BUILD)/generated/
	$(Q) (rmdir $(BUILD)/*/*/*  $(BUILD)/*/* $(BUILD)/*) 2> /dev/null || true

dist-clean:
	$(LOG) "DIST-CLEAN"
	$(Q) rm -rf $(BUILD)

# Generic pattern rules (used for most source files).

$(BUILD)/%.o: %.c
	$(LOG) "CC" "$@"
	$(Q) mkdir -p $(@D)
	$(Q) $(CC) $(CFLAGS) $< -c -o $@

$(BUILD)/%.o: %.rc
	$(LOG) "WINRC" "$@"
	$(Q) mkdir -p $(@D)
	$(Q) $(WINDRES) -I$(ROOT) -I$(BUILD) $< $@

$(BUILD_TARGET): $(BUILD_OBJECTS)
	$(LOG) "LINK" "$@"
	$(Q) $(CC) $(BUILD_OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

.PHONY: all clean .pregen

-include $(BUILD_DEPS)
