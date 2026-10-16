# Builds the firmware image build/image.bin with the Epson GNU33 toolchain.
#
#   make            build/image.bin (8 MB, the whole NOR flash image)
#   make clean
#
# Once after cloning: python tools/extract_assets.py (the graphics and save data of your
# FW_4UPlusJP.bin, which the image includes). Game text written as _("...") in the sources
# becomes bytes in the game's character set (tools/textpp.py) before compiling.

TOOLS ?= C:/EPSON/GNU33
PYTHON ?= python
CC := $(TOOLS)/xgcc
AS := $(TOOLS)/as
LD := $(TOOLS)/ld
OBJCOPY := $(TOOLS)/objcopy
B := build

CFLAGS := -B$(TOOLS)/ -mc33pe -medda32 -mlong-calls -O1 -fno-builtin -mno-sjis-filt -Iinclude
ASFLAGS := -mc33pe -medda32
LIBS := $(TOOLS)/lib/pe/libstdio.a $(TOOLS)/lib/pe/libc.a $(TOOLS)/lib/pe/libgcc.a
ASSETS := $(wildcard assets/*.bin)
MKDIR = @$(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)"

SRCS := $(wildcard src/*/*.c src/*/*/*.c)
OBJS := $(SRCS:src/%.c=$(B)/%.o) $(patsubst asm/%.s,$(B)/asm/%.o,$(wildcard asm/*.s))

.PHONY: all clean
all: $(B)/image.bin

# the flash image, its ROM check reference set (romsum), the API checked (apicheck), and
# the symbol file for the emulator (symfile)
$(B)/image.bin: $(B)/image.elf tools/romsum.py tools/apicheck.py tools/symfile.py
	$(OBJCOPY) -I elf32-little -O binary --gap-fill 0xff --pad-to 0x2800000 $< $@
	$(PYTHON) tools/romsum.py $< $@
	$(PYTHON) tools/apicheck.py $<
	$(PYTHON) tools/symfile.py $< $(B)/image.sym

$(B)/image.elf: $(OBJS) $(ASSETS) ldscripts/firmware.ld
	$(LD) -Map $(B)/image.map -o $@ -T ldscripts/firmware.ld $(OBJS) $(LIBS) \
	    -b binary $(ASSETS) -b elf32-c33

# C: game text to bytes, then compile (-MMD -MP: the header dependencies)
$(B)/%.o: src/%.c tools/textpp.py tools/charset.py
	$(MKDIR) $(@D)
	$(PYTHON) tools/textpp.py $< $(B)/$*.pp.c
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -MF $(B)/$*.d -c -o $@ $(B)/$*.pp.c

$(B)/asm/%.o: asm/%.s
	$(MKDIR) $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -rf $(B)

-include $(SRCS:src/%.c=$(B)/%.d)
