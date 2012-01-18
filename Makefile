# fuck off default makefile rules
.SUFFIXES:
SUFFIX := 

# get the build configuration (./configure)
CONFIG_MAK := Config.mak
ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
ifeq ($(filter clean, $(MAKECMDGOALS)),mrproper)
-include $(CONFIG_MAK)
else
-include $(CONFIG_MAK)
endif
else
include $(CONFIG_MAK)
endif

# commands
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar
RM := rm

# the obvious
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

AMFPARSE_BIN := amfparse$(SUFFIX)
AMFPARSE_LIBS :=
AMFPARSE_OBJ := $(OS_OBJ) \
		amf3.o \
		amf.o \
		hexdump.o \
		amfparse.o

MAYHEM_BIN := mayhem$(SUFFIX)
MAYHEM_LIBS := 
MAYHEM_OBJ := $(OS_OBJ) \
		cvars.o \
		hexdump.o \
		netstatus.o \
		rtmp.o \
		amf3.o \
		amf.o \
		flv.o \
		mayhem.o \
		mayhem-amf.o \
		main.o

ALL_BIN := $(MAYHEM_BIN) $(AMFPARSE_BIN)
ALL_OBJ := $(MAYHEM_OBJ) $(AMFPARSE_OBJ)
ALL_DEP := $(patsubst %.o, .%.d, $(ALL_OBJ))
ALL_TARGETS := $(ALL_BIN)

TARGET: all

.PHONY: all clean

all: $(ALL_BIN)

# if clean is one of the goals, make sure clean comes before everything else
ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
CLEAN_DEP := clean
else
CLEAN_DEP :=
endif

# build C files and track deps
%.o %.d: %.c $(CLEAN_DEP) $(CONFIG_MAK) Makefile
	@echo " [C] $<"
	@$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o, .%.d, $@) \
		-MT $(patsubst .%.d, %.o, $@) \
		-c -o $(patsubst .%.d, %.o, $@) $<

$(MAYHEM_BIN): $(MAYHEM_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(MAYHEM_OBJ) $(MAYHEM_LIBS)

$(AMFPARSE_BIN): $(AMFPARSE_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(AMFPARSE_OBJ) $(AMFPARSE_LIBS)

clean:
	$(RM) -f $(ALL_TARGETS) $(ALL_OBJ) $(ALL_DEP)

# clean everything and delete config too
mrproper: clean
	$(RM) -f $(CONFIG_MAK)

# include whatever dep files we have
# ignore them if we're cleaning up
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),mrproepr)
-include $(ALL_DEP)
endif
endif
