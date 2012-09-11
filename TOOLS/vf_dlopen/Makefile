FILTERS = showqscale telecine tile rectangle
COMMON = filterutils.o

OBJECTS = $(patsubst %,%.o,$(FILTERS)) $(COMMON)
HEADERS = $(wildcard *.h)
OUT = $(patsubst %,%.so,$(FILTERS))

CFLAGS ?= -Wall -Wextra -O3 -march=native -mtune=native -ffast-math

CPPFLAGS += -I../../libmpcodecs
CFLAGS += -fPIC
LDFLAGS += -shared -fPIC

ifneq ($(LTO),)
CFLAGS += -flto
LDFLAGS += $(CFLAGS) -flto
endif

all: $(OUT)

clean:
	$(RM) $(OBJECTS) $(OUT)

%.so: %.o $(COMMON)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $(COMMON) $<

# FIXME replace this by real dependency tracking
%.o: %.c $(HEADERS)

