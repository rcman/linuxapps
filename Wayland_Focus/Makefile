# Wayland Window Monitor
# Requires: SDL2, SDL2_ttf, GIO (for D-Bus)

CXX = g++
CXXFLAGS = -Wall -Wextra -O2
LIBS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf gio-2.0)

TARGET = wayland_monitor
SRC = wayland_monitor.cpp

EXTENSION_UUID = window-introspect@focus
EXTENSION_DIR = $(HOME)/.local/share/gnome-shell/extensions/$(EXTENSION_UUID)

.PHONY: all clean install-extension enable-extension install uninstall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET)

install-extension:
	mkdir -p $(EXTENSION_DIR)
	cp extension/extension.js $(EXTENSION_DIR)/
	cp extension/metadata.json $(EXTENSION_DIR)/
	@echo "Extension installed. Please log out and log back in, then run:"
	@echo "  gnome-extensions enable $(EXTENSION_UUID)"

enable-extension:
	gnome-extensions enable $(EXTENSION_UUID)

disable-extension:
	gnome-extensions disable $(EXTENSION_UUID)

install: all install-extension
	@echo "Build complete. Run ./$(TARGET) to start the monitor."

uninstall: disable-extension
	rm -rf $(EXTENSION_DIR)
	rm -f $(TARGET)
