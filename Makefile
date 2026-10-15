CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
# mandatory flags, kept even if CFLAGS is overridden on the command line;
# pass extra flags (e.g. -Werror) via EXTRA_CFLAGS without losing these.
override CFLAGS += -pthread $(shell pkg-config --cflags libcurl) $(EXTRA_CFLAGS)
LDFLAGS += -pthread
LDLIBS  += $(shell pkg-config --libs libcurl) -ldl

BIN  = scour
SRCS = main.c args.c wordlist.c http.c plugins.c
OBJS = $(SRCS:.c=.o)
DEPS = scour.h colors.h plugin.h plugins.h

# example/contributed plugins: every plugins/*.c builds to plugins/*.so
PLUGIN_SRCS = $(wildcard plugins/*.c)
PLUGIN_SOS  = $(PLUGIN_SRCS:.c=.so)

all: $(BIN) plugins

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

# build all plugins
plugins: $(PLUGIN_SOS)

plugins/%.so: plugins/%.c plugin.h
	$(CC) $(CFLAGS) -fPIC -shared -I. -o $@ $<

clean:
	rm -f $(OBJS) $(BIN) $(PLUGIN_SOS)

.PHONY: all clean plugins
