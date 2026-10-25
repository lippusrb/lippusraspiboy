ROOT := ..
GAME ?=
ifeq ($(GAME),)
$(error indica il gioco: make GAME=garden_defender)
endif

CC := clang
CFLAGS := -O2 -Wall -Wextra -I$(ROOT)/sdk/include -I$(ROOT)/src -I.
OUT := $(ROOT)/build/host/$(GAME)
SRCS := app.m gfx.c audio.c sys.c orient.c $(ROOT)/src/font.c $(ROOT)/games/$(GAME)/main.c

.PHONY: all
all: $(OUT)

$(OUT): $(SRCS) host.h
	mkdir -p $(dir $(OUT))
	$(CC) $(CFLAGS) -fobjc-arc $(SRCS) -framework Cocoa -framework AudioToolbox -o $(OUT)
