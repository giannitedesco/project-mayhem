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

ifeq ($(OS), win32)
NBIO_MOD := nbio-iocp.o
OS_DEFS := -DHAVE_IOCP
else
ifeq ($(OS), linux)
NBIO_MOD := nbio-epoll.o nbio-poll.o
OS_DEFS := -DHAVE_ACCEPT4 -DHAVE_EPOLL -DHAVE_POLL
else
NBIO_MOD := nbio-poll.o
OS_DEFS := -DHAVE_POLL
endif
endif

# the obvious
EXTRA_DEFS := -D_FILE_OFFSET_BITS=64 $(OS_DEFS)
WFLAGS := \
	-Wall \
	-Wsign-compare -Wcast-align \
	-Waggregate-return \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wmissing-noreturn \
	-finline-functions \
	-Wmissing-format-attribute \
	-Iinclude

ifeq ($(HAVE_PYTHON), yes)
CFLAGS := $(PYTHON_CFLAGS) $(EXTRA_DEFS) $(WFLAGS) -fPIC
else
CFLAGS := -g -pipe -O2 $(EXTRA_DEFS) $(WFLAGS)
endif

ifeq ($(OS), win32)
OS_OBJ := blob_win32.o os-win32.o
else
OS_OBJ := blob.o os-posix.o
endif

# Internal "static libraries"
NBIO_OBJ := nbio.o nbio-connecter.o $(NBIO_MOD)
AMF_OBJ := amf3.o \
		amf.o \
		hexdump.o
MAYHEM_OBJ := $(AMF_OBJ) \
		$(NBIO_OBJ) \
		cvars.o \
		netstatus.o \
		rtmp.o \
		flv.o \
		mayhem.o \
		mayhem-amf.o

### Targets and their objects
AMFPARSE_BIN := amfparse$(SUFFIX)
AMFPARSE_LIBS :=
AMFPARSE_OBJ := $(OS_OBJ) \
		$(AMF_OBJ) \
		amfparse.o

DUMP_BIN := wmdump$(SUFFIX)
DUMP_LIBS := 
DUMP_OBJ := $(OS_OBJ) \
		$(MAYHEM_OBJ) \
		dump.o

PYPM_LIB := mayhem$(LIBSUFFIX)
PYPM_LIBS := $(PYTHON_LDFLAGS)
PYPM_OBJ := $(OS_OBJ) \
		$(MAYHEM_OBJ) \
		pypm.o \
		pyvars.o \
		pyrtmp_pkt.o \
		pynaiad_goldshow.o \
		pynaiad_room.o \
		pynaiad_user.o \
		pymayhem.o

ALL_BIN := $(DUMP_BIN) $(AMFPARSE_BIN)
ALL_LIB := $(PYPM_LIB)
ALL_OBJ := $(DUMP_OBJ) $(AMFPARSE_OBJ) $(PYPM_OBJ)
ALL_DEP := $(patsubst %.o, .%.d, $(ALL_OBJ))
ALL_TARGETS := $(ALL_BIN) $(ALL_LIB)

ALL_IDL := pyvars.idl \
	pyrtmp_pkt.idl \
	pynaiad_goldshow.idl \
	pynaiad_room.idl \
	pynaiad_user.idl

ALL_GEN_HEADERS := $(patsubst %.idl, %.h, $(ALL_IDL))
ALL_GEN := $(ALL_GEN_HEADERS) $(patsubst %.idl, %.c, $(ALL_IDL))

.PHONY: all clean

TARGET: all

all: $(ALL_TARGETS)

# pypm.c includes generated headers so make sure they're made first
pypm.o: pypm.c $(ALL_GEN_HEADERS)

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

# generate C files from IDL's
%.c %.h: %.idl
	@python $<

$(DUMP_BIN): $(DUMP_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(DUMP_OBJ) $(DUMP_LIBS)

$(AMFPARSE_BIN): $(AMFPARSE_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(AMFPARSE_OBJ) $(AMFPARSE_LIBS)

$(PYPM_LIB): $(PYPM_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -shared -o $@ $(PYPM_OBJ) $(PYPM_LIBS)

clean:
	$(RM) -f $(ALL_TARGETS) $(ALL_OBJ) $(ALL_DEP) $(ALL_GEN)
	$(RM) -f *.pyc pymayhem/*.pyc

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
