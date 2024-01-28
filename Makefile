# util: tests makefile

ROOT := ..
UTIL_DIR := $(ROOT)/util
FFOS_DIR := $(ROOT)/ffos
FFBASE_DIR := $(ROOT)/ffbase

include $(FFBASE_DIR)/conf.mk

OBJ := \
	test.o \
	test-ipaddr.o

utiltest: $(OBJ)
	$(LINK) $+ $(LINKFLAGS) -o $@

CFLAGS := -I$(UTIL_DIR) -I$(FFOS_DIR) -I$(FFBASE_DIR) \
	-Wall -Wextra \
	-DFFBASE_HAVE_FFERR_STR -DFF_DEBUG -g
ifeq "$(DEBUG)" "1"
	CFLAGS += -O0
else
	CFLAGS += -O3 -fno-strict-aliasing
endif
CFLAGS += -std=gnu99

%.o: $(UTIL_DIR)/data/%.c
	$(C) $(CFLAGS) $< -o $@
%.o: $(UTIL_DIR)/net/%.c
	$(C) $(CFLAGS) $< -o $@
%.o: $(UTIL_DIR)/%.c
	$(C) $(CFLAGS) $< -o $@

ffssl.o: $(UTIL_DIR)/net/ffssl.c
	$(C) $(CFLAGS) -Wno-deprecated-declarations -Wno-unused-parameter $< -o $@
test-ssl.o: $(UTIL_DIR)/net/test-ssl.c
	$(C) $(CFLAGS) -Wno-deprecated-declarations $< -o $@
test-ssl: test-ssl.o \
		ffssl.o
	$(LINK) $+ $(LINKFLAGS) -L_$(OS)-amd64 -lcrypto -lssl -o $@
