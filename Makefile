CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk+-2.0)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk+-2.0)

.PHONY: all install uninstall check clean
all: build/appearance.so

build/appearance.so: src/appearance.c
	@$(PKG_CONFIG) --exists gtk+-2.0 || { echo 'Install libgtk2.0-dev and pkg-config first.'; exit 1; }
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Wno-deprecated-declarations -fPIC $(GTK_CFLAGS) -shared -o $@ $< $(LDFLAGS) $(GTK_LIBS)

install: all
	python3 install.py install

uninstall:
	python3 install.py uninstall

check: all
	python3 -m unittest discover -s tests

clean:
	rm -rf build
