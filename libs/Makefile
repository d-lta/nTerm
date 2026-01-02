# ------------------------------------------------------------------------------
# MicroPython for TI-Nspire: Makefile with optional Ndless and "make lib"
# ------------------------------------------------------------------------------

include ../py/mkenv.mk
-include mpconfigport.mk

# ---- toolchain ---------------------------------------------------------------
CC      := nspire-gcc
LD      := arm-none-eabi-ld.gold
AR     ?= arm-none-eabi-gcc-ar        # LTO-aware archiver
RANLIB ?= arm-none-eabi-gcc-ranlib    # LTO-aware ranlib

# Silence "fatal: not a git repository" if upstream tries to use git
GIT ?= true

# ---- Ndless toggle -----------------------------------------------------------
# Enable Ndless build (keeps modnsp.c) with:
#   make HAVE_NDLESS=1 NDLESS_SDK=/path/to/ndless-sdk
HAVE_NDLESS ?= 0
NDLESS_SDK  ?=

# ---- zehn / packaging metadata ----------------------------------------------
ZEHNFLAGS = --name "micropython" \
            --author "micropython team, ported by Fabian Vogt" \
            --notice "Interactive python console" \
            --version 09

# define main target
PROG = micropython

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# OS name, for simple autoconfig
UNAME_S := $(shell uname -s)

# Disable stripping of the main executable, relocation info is needed by genzehn
NOSTRIP := 1
# DEBUG := 1

# include py core make definitions
include ../py/py.mk

# ---- includes ---------------------------------------------------------------
INC  = -I.
INC += -I..
INC += -I../py
INC += -I$(BUILD)

# Ndless headers (if enabled)
ifeq ($(HAVE_NDLESS),1)
  ifneq ($(NDLESS_SDK),)
    INC += -I$(NDLESS_SDK)/include
  endif
endif

# ---- Architecture + Optimisation/LTO ----------------------------------------
# TI-Nspire targets are ARM9E class. Use flags your toolchain actually supports.
ARCHFLAGS := -marm -mcpu=arm926ej-s -mtune=arm926ej-s

# Turn LTO off by default to avoid lto-wrapper issues at link time with other projects.
# Enable with: make USE_LTO=1
USE_LTO ?= 0

ifdef DEBUG
  COPT = -O0 -g -fno-lto
else
  # Os generally produces smaller, safer images than Ofast here
  COPT = -Os -fomit-frame-pointer -ffunction-sections -fdata-sections
  ifeq ($(USE_LTO),1)
    COPT += -flto
  else
    COPT += -fno-lto
  endif
endif

# ---- compiler / linker flags -------------------------------------------------
# NOTE: removed -Werror so warnings don't fail the build
# Also quiet a few noisy pointer/format warnings common with tagged pointers.
CWARN   = -Wall -Wno-error -Wno-error=cpp -Wno-error=format \
          -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-format

CFLAGS  = $(INC) $(CWARN) -ansi -std=gnu99 $(CFLAGS_MOD) $(COPT) $(ARCHFLAGS) $(CFLAGS_EXTRA)

LDFLAGS = $(LDFLAGS_MOD) -lm $(LDFLAGS_EXTRA) -Wl,--nspireio $(ARCHFLAGS)
ifeq ($(USE_LTO),0)
  LDFLAGS += -fno-lto
endif

# Ndless libs (if enabled)
ifeq ($(HAVE_NDLESS),1)
  ifneq ($(NDLESS_SDK),)
    LDFLAGS += -L$(NDLESS_SDK)/lib -lndls -lnspireio
  endif
endif

# ---- sources / objects -------------------------------------------------------
SRC_C = $(shell find . -name \*.c)

# If Ndless is OFF, drop modnsp.c (it includes <libndls.h>)
ifeq ($(HAVE_NDLESS),0)
  SRC_C := $(filter-out ./modnsp.c modnsp.c,$(SRC_C))
endif

OBJ = $(PY_O) $(addprefix $(BUILD)/, $(SRC_C:.c=.o))

# Generic rules from MicroPython
include ../py/mkrules.mk

.PHONY: all lib clean

# ---- default app build -------------------------------------------------------
all: $(PROG).tns

# ---- static library build ----------------------------------------------------
lib: $(BUILD)/libmicropython.a

$(BUILD)/libmicropython.a: $(OBJ)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(OBJ)
	$(RANLIB) $@

# ---- package .tns with genzehn ----------------------------------------------
$(PROG).tns: $(PROG)
	+genzehn --input $^ --output $@.zehn $(ZEHNFLAGS)
	+make-prg $@.zehn $@
	+rm $@.zehn
