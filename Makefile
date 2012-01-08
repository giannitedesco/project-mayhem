.SUFFIXES:

CONFIG_MAK := Config.mak
-include Config.mak

SUFFIX := 

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar

EXTRA_DEFS := -D_FILE_OFFSET_BITS=64
CFLAGS := -g -pipe -O2 -Wall \
	-Wsign-compare -Wcast-align \
	-Waggregate-return \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wmissing-noreturn \
	-finline-functions \
	-Wmissing-format-attribute \
	-fwrapv \
	-Iinclude \
	$(EXTRA_DEFS)

ifeq ($(OS), win32)
OS_OBJ := blob_win32.o os-win32.o
else
OS_OBJ := blob.o os-posix.o
endif

WMDUMP_BIN := wmdump$(SUFFIX)
WMDUMP_LIBS := 
WMDUMP_OBJ = $(OS_OBJ) \
		cvars.o \
		hexdump.o \
		netconn.o \
		rtmp.o \
		amf.o \
		mayhem.o \
		mayhem-amf.o \
		main.o

ALL_BIN := $(WMDUMP_BIN)
ALL_OBJ := $(WMDUMP_OBJ)
ALL_DEP := $(patsubst %.o, .%.d, $(ALL_OBJ))
ALL_TARGETS := $(ALL_BIN)

TARGET: all

.PHONY: all clean

all: $(ALL_BIN)

ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
CLEAN_DEP := clean
else
CLEAN_DEP :=
endif

%.o %.d: %.c $(CLEAN_DEP) $(CONFIG_MAK) Makefile
	@echo " [C] $<"
	@$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o, .%.d, $@) \
		-MT $(patsubst .%.d, %.o, $@) \
		-c -o $(patsubst .%.d, %.o, $@) $<

$(WMDUMP_BIN): $(WMDUMP_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(WMDUMP_OBJ) $(WMDUMP_LIBS)

clean:
	rm -f $(ALL_TARGETS) $(ALL_OBJ) $(ALL_DEP)

ifneq ($(MAKECMDGOALS),clean)
-include $(ALL_DEP)
endif
