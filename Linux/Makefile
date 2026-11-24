CC       = gcc
PKG      = gtk4
CFLAGS   = -Wall -Wextra -O2 -march=native -mclflushopt $(shell pkg-config --cflags $(PKG)) \
           -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2 -MMD -MP
LDFLAGS  = $(shell pkg-config --libs $(PKG)) -pie -lpthread -lgmp
SRCS     = $(wildcard src/*.c)
OBJS     = $(SRCS:.c=.o)
TARGET   = tuxtimings

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Header deps (.d) — without these, editing types.h/ui.h links a mixed-ABI
# binary (stale .o files read moved struct fields — broke FANs + VCORE once).
-include $(OBJS:.o=.d)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Build the tuxbench kernel module (optional — app works without it)
kmod:
	$(MAKE) -C src/tuxbench

kmod-install: kmod
	$(MAKE) -C src/tuxbench install

kmod-clean:
	$(MAKE) -C src/tuxbench clean

# Unit tests for the pure decoders (no hardware access)
test: tests/test_decode tests/test_spd
	./tests/test_decode
	./tests/test_spd

tests/test_decode: tests/test_decode.c src/dram_decode.c src/util.c src/pm_table.c src/pm_layouts.c
	$(CC) -Wall -Wextra -O1 -o $@ $^ -lm

tests/test_spd: tests/test_spd.c src/spd_decode.c
	$(CC) -Wall -Wextra -O1 -o $@ $^

clean: kmod-clean
	rm -f src/*.o src/*.d $(TARGET) tests/test_decode tests/test_spd

.PHONY: all clean kmod kmod-install kmod-clean test
