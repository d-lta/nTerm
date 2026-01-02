# A sane Makefile for building nterm.
# This version correctly links the MicroPython static library.
#
# It respects the Ndless toolchain's "auto-bundled" Lua library
# while correctly linking the custom-built MicroPython library.
#

# ===== config =====
DEBUG           ?= FALSE
GCC             := nspire-gcc
AS              := nspire-as
GXX             := nspire-g++
LD              := nspire-ld
LUNA            := "$(shell nspire-tools path)/tools/luna/luna"
GENZEHN         := genzehn
AR              := nspire-ar

# Flags
GCCFLAGS        := -Wall -W -marm -MMD -MP -Os -DNDEBUG -DNSPIRE_NEWLIB -DMINIZIP_NO_64 -DMINIZ_NO_TIME
LDFLAGS         := -Wl,--nmagic -Wl,--gc-sections
ZEHNFLAGS       := --name "nterm" --240x320-support true --compress

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -Os -DNDEBUG
else
	GCCFLAGS += -O0 -g -DDEBUG
endif

# Add Nucleus OS specific flags
GCCFLAGS += -DNSPIRE_NEWLIB

# --- Library Definitions ---
MICROPYTHON_LIB_DIR = libs

# Add MicroPython header include paths
GCCFLAGS += -Icommands -I$(MICROPYTHON_LIB_DIR)

LIBS = -lmicropython -lndls -lz

# Add the library directory and libraries to the linker flags
LDFLAGS += -L$(MICROPYTHON_LIB_DIR) $(LIBS)

# ===== sources / objects =====
# 1. Main source file is in 'src'
SRC_C_MAIN := src/nterm.c

# 2. Command sources are in 'commands'
SRC_C_COMMANDS := $(wildcard commands/*.c)

# 3. Combine all source files.
SRC_C := $(SRC_C_MAIN) $(SRC_C_COMMANDS)

# 4. Define the BUILD directory for all intermediate files
BUILD_DIR := build

# Ensure the object files are output to the BUILD_DIR
OBJ     := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC_C_MAIN))
OBJ     += $(patsubst commands/%.c,$(BUILD_DIR)/%.o,$(SRC_C_COMMANDS))

# 5. Define where the dependency files will live.
DEPS    := $(OBJ:.o=.d)


EXE     := nterm.luax
DISTDIR:= .

vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)

# ===== rules =====
all: $(BUILD_DIR) $(EXE).tns nterm_frontend.tns

# Rule to create the build directory (Crucial Tab here!)
$(BUILD_DIR):
	@mkdir -p $@

nterm_frontend.tns: src/nterm_frontend.lua
	@echo "Compiling Lua frontend..."
	$(LUNA) $< $@

# New, powerful pattern rule to compile source files from ANY directory
# and place the output in the BUILD_DIR. (Crucial Tabs here!)
$(BUILD_DIR)/%.o: %.c
	@echo "Compiling $<..."
	$(GCC) $(GCCFLAGS) -c $< -o $@

# Specific rule for the main file, which has its own directory. (Crucial Tabs here!)
$(BUILD_DIR)/nterm.o: src/nterm.c
	@echo "Compiling $<..."
	$(GCC) $(GCCFLAGS) -c $< -o $@

# And a generic one for all the files in the 'commands' directory. (Crucial Tabs here!)
$(BUILD_DIR)/%.o: commands/%.c
	@echo "Compiling $<..."
	$(GCC) $(GCCFLAGS) -c $< -o $@

# Assembly rule - needs to be updated for BUILD_DIR too (Crucial Tabs here!)
$(BUILD_DIR)/%.o: %.S
	@echo "Assembling $<..."
	$(AS) -c $< -o $@

$(EXE).elf: $(OBJ)
	@echo "Linking $(EXE).elf..."
	# The object files ($^) are now located in the BUILD_DIR
	$(LD) $^ $(LDFLAGS) -o $@

$(EXE).tns: $(EXE).elf
	@echo "Creating $(EXE).tns..."
	$(GENZEHN) --input $(DISTDIR)/$^ --output $(DISTDIR)/$@.zehn $(ZEHNFLAGS)
	make-prg $(DISTDIR)/$@.zehn $(DISTDIR)/$@
	rm $(DISTDIR)/$@.zehn

clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	rm -f $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn nterm_frontend.tns

distclean: clean
	@echo "Deep cleaning..."
	find . -name "*.o" -delete
	find . -name "*.d" -delete

# Development helpers
debug: DEBUG=TRUE
debug: all

install: $(EXE).tns nterm_frontend.tns
	@echo "Copy $(EXE).tns and nterm_frontend.tns to your calculator"

# This uses the dependency files in the BUILD_DIR
-include $(DEPS)

.PHONY: all clean distclean debug install
