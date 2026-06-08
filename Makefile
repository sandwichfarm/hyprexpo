# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS = -shared -fPIC -g -std=c++2b -Wno-c++11-narrowing -Wno-narrowing
LUA_PKG_CONFIG ?= $(shell if pkg-config --exists lua5.4; then printf 'lua5.4'; elif pkg-config --exists lua; then printf 'lua'; else printf 'lua5.4'; fi)
PKG_CONFIG_DEPS = pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon $(LUA_PKG_CONFIG)
LINK_DEPS = pangocairo xkbcommon $(LUA_PKG_CONFIG)
INCLUDES = $(shell pkg-config --cflags $(PKG_CONFIG_DEPS))
LIBS = $(shell pkg-config --libs $(LINK_DEPS))

SRC = main.cpp Dispatchers.cpp PluginConfig.cpp Overview.cpp OverviewInteraction.cpp OverviewRender.cpp ExpoGesture.cpp OverviewPassElement.cpp HyprexpoLogic.cpp
HEADERS = globals.hpp Dispatchers.hpp PluginConfig.hpp HyprlandConfigCompat.hpp Overview.hpp OverviewInternal.hpp ExpoGesture.hpp OverviewPassElement.hpp HyprexpoConfig.hpp HyprexpoLogic.hpp
TARGET = hyprexpo.so
TEST_TARGET = HyprexpoLogicTests
INSTALL_DIR = /var/cache/hyprpm/$(USER)/hyprexpo
INSTALL_NAME = hyprexpo.so

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(INCLUDES) $(SRC) -o $@ $(LIBS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(INSTALL_DIR)/$(INSTALL_NAME)

clean:
	rm -f ./$(TARGET) ./$(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): HyprexpoLogic.cpp HyprexpoLogic.hpp HyprexpoConfig.hpp tests/HyprexpoLogicTests.cpp
	$(CXX) -std=c++2b -Wall -Wextra -Werror HyprexpoLogic.cpp tests/HyprexpoLogicTests.cpp -o $@

.PHONY: all clean install test
