CC = $(CXX)
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic
PREFIX = /usr/local

release_options = -O2
debug_options = -g -O0

-include build/config.mk

SRCS = $(wildcard src/*.cpp)
RELEASE_OBJS = $(SRCS:src/%.cpp=build/release/%.o)
DEBUG_OBJS = $(SRCS:src/%.cpp=build/debug/%.o)

RELEASE_BINS = $(patsubst build/release/%_main.o,build/release/%,$(filter build/release/%_main.o,$(RELEASE_OBJS)))
DEBUG_BINS = $(patsubst build/debug/%_main.o,build/debug/%,$(filter build/debug/%_main.o,$(DEBUG_OBJS)))
TEST_BINS = $(patsubst build/debug/%_test.o,build/debug/%,$(filter build/debug/%_test.o,$(DEBUG_OBJS)))

.PHONY: all
all: release

.PHONY: install
install: release
	@install -d $(PREFIX)/bin
	$(foreach bin,$(RELEASE_BINS),install -m 755 $(bin) $(PREFIX)/bin/$(notdir $(bin));)

.PHONY: configure
configure:
	@mkdir -p build
	@echo PREFIX=$(PREFIX) > build/config.mk
	@echo CXXFLAGS=$(CXXFLAGS) >> build/config.mk
	@echo CC=$(CC) >> build/config.mk
	@echo CXX=$(CXX) >> build/config.mk

.PHONY: release
release: $(RELEASE_BINS)

.PHONY: debug
debug: $(DEBUG_BINS)

.PHONY: check
check: $(TEST_BINS)
	$(foreach bin,$(TEST_BINS),./$(bin);)

.PHONY: clean
clean:
	$(RM) -r build

build/release/%.o: src/%.cpp
	@mkdir -p build/release
	$(COMPILE.cpp) $(OUTPUT_OPTION) $(release_options) $<

build/release/%: build/release/%_main.o
	@mkdir -p build/release
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(release_options) -o $@

build/debug/%.o: src/%.cpp
	@mkdir -p build/debug
	$(COMPILE.cpp) $(OUTPUT_OPTION) $(debug_options) $<

build/debug/%: build/debug/%_main.o
	@mkdir -p build/debug
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(debug_options) -o $@

.PHONY: dep
dep: build/Makefile.dep

build/Makefile.dep: $(SRCS)
	@mkdir -p build
	$(CXX) -MM $(SRCS) | sed 's_^\S\+\.o:_build/release/\0_' > build/Makefile.dep
	$(CXX) -MM $(SRCS) | sed 's_^\S\+\.o:_build/debug/\0_' >> build/Makefile.dep

-include build/Makefile.dep

define DEPENDS_ON
build/release/$(1): build/release/$(2).o
build/debug/$(1): build/debug/$(2).o
endef

$(eval $(call DEPENDS_ON,yuproc,file_util))
