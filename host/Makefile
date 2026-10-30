ROOT := ..
GAME ?=
ifeq ($(GAME),)
$(error indica il gioco: make GAME=garden_defender)
endif

CC := clang
CFLAGS := -O2 -Wall -Wextra -I$(ROOT)/sdk/include -I$(ROOT)/src -I.
OUT := $(ROOT)/build/host/$(GAME)
GAME_DIR := $(ROOT)/games/$(GAME)
SRCS := app.m gfx.c audio.c sys.c orient.c $(ROOT)/src/font.c $(GAME_DIR)/main.c
HDRS := $(wildcard $(GAME_DIR)/*.h)

.PHONY: all
all: $(OUT)

$(OUT): $(SRCS) host.h $(HDRS)
	mkdir -p $(dir $(OUT))
	$(CC) $(CFLAGS) -fobjc-arc $(SRCS) -framework Cocoa -framework AudioToolbox -o $(OUT)
