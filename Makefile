# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

# --- Versioning -----------------------------------------------------------
# The VERSION file is the single source of truth (see scripts/version.sh).
# VERSION_BASE is the release version used for tagging/checks; VERSION is what
# gets baked into the binary (adds a -dev marker for non-release builds).
VERSION_FILE := VERSION
VERSION_BASE := $(shell sh scripts/version.sh --base)
VERSION      := $(shell sh scripts/version.sh)
VERSION_REGEX := ^v[0-9]+\.[0-9]+\.[0-9]+(\+[0-9]+)?$$
VERSION_DEFINE := -DHYPREXPO_VERSION='"$(VERSION)"'

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

# Rebuild when the version changes so the baked-in value stays correct.
$(TARGET): $(SRC) $(HEADERS) $(VERSION_FILE)
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(VERSION_DEFINE) $(INCLUDES) $(SRC) -o $@ $(LIBS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(INSTALL_DIR)/$(INSTALL_NAME)

clean:
	rm -f ./$(TARGET) ./$(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): HyprexpoLogic.cpp HyprexpoLogic.hpp HyprexpoConfig.hpp tests/HyprexpoLogicTests.cpp
	$(CXX) -std=c++2b -Wall -Wextra -Werror HyprexpoLogic.cpp tests/HyprexpoLogicTests.cpp -o $@

# --- Release ceremony -----------------------------------------------------
# 1. make version vX.Y.Z+N   set + commit the VERSION file
# 2. make tag                create the matching annotated git tag
# 3. make publish            push branch + tag (triggers the release workflow)

# Support positional syntax: `make version v0.55.2+2`.
# Turn the trailing word(s) into no-op goals so make doesn't error on them.
ifeq (version,$(firstword $(MAKECMDGOALS)))
  VERSION_ARG := $(strip $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS)))
  ifneq ($(VERSION_ARG),)
    $(eval $(VERSION_ARG):;@:)
  endif
endif
# Also accept `make version v=v0.55.2+2`.
SET_VERSION := $(if $(VERSION_ARG),$(VERSION_ARG),$(v))

version:
	@if [ -z '$(SET_VERSION)' ]; then \
		printf '%s\n' '$(VERSION)'; \
	else \
		printf '%s' '$(SET_VERSION)' | grep -Eq '$(VERSION_REGEX)' \
			|| { echo "error: '$(SET_VERSION)' must look like v1.2.3 or v1.2.3+4"; exit 1; }; \
		printf '%s\n' '$(SET_VERSION)' > $(VERSION_FILE); \
		git add $(VERSION_FILE); \
		git commit -m 'release $(SET_VERSION)' -- $(VERSION_FILE); \
		echo "VERSION -> $(SET_VERSION) (committed). next: make tag && make publish"; \
	fi

tag:
	@v='$(VERSION_BASE)'; \
	printf '%s' "$$v" | grep -Eq '$(VERSION_REGEX)' \
		|| { echo "error: VERSION file '$$v' must look like v1.2.3 or v1.2.3+4"; exit 1; }; \
	if [ -n "$$(git status --porcelain -- $(VERSION_FILE))" ]; then \
		echo "error: $(VERSION_FILE) has uncommitted changes; run 'make version ...' first"; exit 1; fi; \
	if git rev-parse -q --verify "refs/tags/$$v" >/dev/null; then \
		echo "error: tag $$v already exists"; exit 1; fi; \
	git tag -a "$$v" -m "$$v"; \
	echo "created tag $$v. next: make publish"

publish:
	@v='$(VERSION_BASE)'; \
	if ! git rev-parse -q --verify "refs/tags/$$v" >/dev/null; then \
		echo "error: tag $$v does not exist; run 'make tag' first"; exit 1; fi; \
	git push origin HEAD; \
	git push origin "$$v"; \
	echo "pushed branch + tag $$v; the release workflow will build and publish."

# Used by CI to guarantee the VERSION file matches the tag being released.
# Pass the tag via TAG=, otherwise the exact tag on HEAD is used.
check-version:
	@file_ver='$(VERSION_BASE)'; \
	tag_ver="$${TAG:-$$(git describe --tags --exact-match 2>/dev/null || true)}"; \
	echo "VERSION file: $$file_ver"; \
	echo "git tag:      $$tag_ver"; \
	[ -n "$$tag_ver" ] || { echo "error: no exact tag on HEAD (set TAG=...)"; exit 1; }; \
	[ "$$file_ver" = "$$tag_ver" ] \
		|| { echo "::error::VERSION ($$file_ver) does not match tag ($$tag_ver)"; exit 1; }; \
	echo "version aligned: $$file_ver"

.PHONY: all clean install test version tag publish check-version
