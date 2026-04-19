# ffpack: deflate compression from zlib
# 2020, Simon Zolin

include ../config.mk

VER := 1.2.11
URL := https://www.zlib.net/zlib-$(VER).tar.xz
PKG := $(FFPACK)/zlib/$(notdir $(URL))
PKGDIR := zlib-$(VER)
LIB := libz-ffpack.$(SO)

default: $(PKGDIR)
	$(SUBMAKE) $(LIB)
	$(SUBMAKE) libz.a

# download
$(PKG):
	$(CURL) -o $@ $(URL)

# unpack
$(PKGDIR): $(PKG)
	$(UNTAR_XZ) $(PKG)

# build
SRC := adler32.c \
	deflate.c \
	infback.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	trees.c \
	zutil.c
OBJ := zlib-ff.o $(SRC:.c=.o)

CFLAGS := -I$(PKGDIR) \
	-DZ_EXP -DNO_GZIP -DHAVE_UNISTD_H -DHAVE_STDARG_H -D_LARGEFILE64_SOURCE=1 \
	-O3 -flto -fvisibility=hidden -fno-asynchronous-unwind-tables -fpic
ifneq ($(OS),win)
	CFLAGS += -DHAVE_HIDDEN
endif

LDFLAGS := $(LINK_INSTALLNAME_LOADERPATH) -shared -s

zlib-ff.o: $(FFPACK)/zlib/zlib-ff.c $(FFPACK)/zlib/zlib-ff.h
	$(C) $(CFLAGS) $< -o $@

%.o: $(PKGDIR)/%.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) $(LDFLAGS) $+ -o $@

libz.a: $(OBJ)
	$(AR) rcs $@ $+

clean:
	$(RM) $(OBJ) zlib-$(VER)

distclean: clean
	$(RM) libz-ffpack.so libz-ffpack.dll libz-ffpack.dylib
