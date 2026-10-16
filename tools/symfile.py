"""Write the symbol file of a built image: one "<hex address> <name>" line per global symbol.

usage: python tools/symfile.py IMAGE.elf IMAGE.sym

The emulator's 4u-clean profile (tamaemu, branch tama-clean-profile) reads the firmware
addresses it needs from it (sys_display_state, spot_touch_any_reader); its --syms option
names functions in traces.
"""
import os
import re
import subprocess
import sys

OBJDUMP = os.path.join(os.environ.get('TOOLS', 'C:/EPSON/GNU33'), 'objdump')


def main():
    elf, out = sys.argv[1:3]
    text = subprocess.run([OBJDUMP, '-t', elf], capture_output=True, text=True, check=True,
                          env=dict(os.environ, CYGWIN='nodosfilewarning')).stdout
    syms = set()
    for line in text.splitlines():
        m = re.match(r'^([0-9a-f]{8}) g\s.*\s(\S+)$', line)
        if m and not m.group(2).startswith('.'):
            syms.add((int(m.group(1), 16), m.group(2)))
    with open(out, 'w', newline='\n') as f:
        for a, n in sorted(syms):
            f.write('%08x %s\n' % (a, n))


if __name__ == '__main__':
    main()
