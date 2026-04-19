# build SDL

include ../../ffbase/conf.mk
CURL := curl -L

URL := https://github.com/libsdl-org/SDL/releases/download/release-3.2.28/SDL3-3.2.28.tar.gz
PKG := ../SDL/$(notdir $(URL))
PKG_CSUM := 7eedab7982f37bfdf342dc6d2dbd24ce
DIR := SDL3-3.2.28

default: libSDL3.$(SO)

$(PKG):
	$(CURL) -o $(PKG) $(URL)

$(DIR): $(PKG)
	echo "$(PKG_CSUM) *$(PKG)" | md5sum -c -
	tar xf $(PKG)
	touch $@

CMAKE_FLAGS :=
ifeq "$(OS)" "windows"
	CMAKE_FLAGS := -DCMAKE_TOOLCHAIN_FILE=build-scripts/cmake-toolchain-mingw64-x86_64.cmake
endif
CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=RelWithDebInfo
CMAKE_FLAGS += -DSDL_SHARED=ON -DSDL_STATIC=OFF \
	-DSDL_AUDIO=OFF \
	-DSDL_CAMERA=OFF \
	-DSDL_HAPTIC=OFF \
	-DSDL_POWER=OFF \
	-DSDL_SENSOR=OFF \
	-DSDL_DIALOG=OFF

$(DIR)/build/Makefile: | $(DIR)
	cd $(DIR) && cmake -S . -B build $(CMAKE_FLAGS)

LINKFLAGS += -shared \
	-Wl,--version-script=$(DIR)/src/dynapi/SDL_dynapi.sym \
	$(DIR)/build/libSDL_uclibc.a
OBJS := *.o
ifeq "$(OS)" "linux"
	LINKFLAGS += -lm
else
	LINKFLAGS += -static-libgcc \
		-lkernel32 -luser32 -lgdi32 -limm32 -lwinmm -lshell32 -lole32 -lversion -lsetupapi
	OBJS := *.obj
endif

link:
	$(LINK) $(shell find $(DIR)/build/CMakeFiles/SDL3-shared.dir -name '$(OBJS)') $(LINKFLAGS) -o libSDL3.$(SO)

libSDL3.$(SO): | $(DIR)
	$(SUBMAKE) $(DIR)/build/Makefile
	cd $(DIR) && cmake --build build -v
	$(SUBMAKE) link
	$(SUBMAKE) $@.debug

%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@
