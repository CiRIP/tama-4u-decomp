"""Set the test mode's ROM check reference in a built image.

usage: python tools/romsum.py IMAGE.elf IMAGE.bin

The test mode's ROM check (test_rom_sum_program) adds up the program halfwords from
flash_program_start to flash_program_end and compares the sum, minus the reference
test_rom_sum_ref, with test_rom_sum_ref. The reference is an initialized variable, so its
initial value lies in the summed range (in the .data load image): it has to equal the sum of
all the other halfwords. This writes that value into the image (the source says 0x8F12, the
original firmware's).
"""
import os
import re
import subprocess
import sys

OBJDUMP = os.path.join(os.environ.get('TOOLS', 'C:/EPSON/GNU33'), 'objdump')
FLASH = 0x2000000


def objdump(*args):
    return subprocess.run([OBJDUMP] + list(args), capture_output=True, text=True, check=True,
                          env=dict(os.environ, CYGWIN='nodosfilewarning')).stdout


def main():
    elf, binpath = sys.argv[1:3]
    syms = {}
    for line in objdump('-t', elf).splitlines():
        p = line.split()
        if len(p) >= 2 and re.match(r'[0-9a-f]{8}$', p[0]):
            syms[p[-1]] = int(p[0], 16)
    # the .data section: run address (VMA) and where its initial values are stored (LMA)
    vma = lma = None
    for line in objdump('-h', elf).splitlines():
        p = line.split()
        if len(p) >= 6 and p[1] == '.data':
            vma, lma = int(p[3], 16), int(p[4], 16)
    ref = lma + syms['test_rom_sum_ref'] - vma
    img = bytearray(open(binpath, 'rb').read())

    def half(a):
        return int.from_bytes(img[a - FLASH:a - FLASH + 2], 'little')
    total = 0
    for a in range(syms['flash_program_start'], syms['flash_program_end'], 2):
        if a != ref:
            total += half(a)
    img[ref - FLASH:ref - FLASH + 2] = (total & 0xFFFF).to_bytes(2, 'little')
    open(binpath, 'wb').write(img)
    print('ROM check reference at 0x%07X: 0x%04X' % (ref, total & 0xFFFF))


if __name__ == '__main__':
    main()
