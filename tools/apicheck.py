"""Check the downloaded-program API of a built image.

usage: python tools/apicheck.py IMAGE.elf

Downloaded programs call the firmware at fixed addresses in 0x2400AD0..0x2401D92 (the
original firmware's functions there; notes/findings/tamago.md). asm/api.s puts a jump to each
function at its address; this checks, in the linked image, that every entry of asm/api.s is
at its address and jumps to its function.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDUMP = os.path.join(os.environ.get('TOOLS', 'C:/EPSON/GNU33'), 'objdump')


def main():
    elf = sys.argv[1]
    env = dict(os.environ, CYGWIN='nodosfilewarning')
    syms = {}
    for line in subprocess.run([OBJDUMP, '-t', elf], capture_output=True, text=True,
                               env=env).stdout.splitlines():
        p = line.split()
        if len(p) >= 2 and re.match(r'[0-9a-f]{8}$', p[0]):
            syms.setdefault(p[-1], int(p[0], 16))
    entries = re.findall(r'^\s*entry\s+(0x[0-9A-Fa-f]+),\s*(\w+)',
                         open(os.path.join(ROOT, 'asm', 'api.s')).read(), re.M)
    dis = subprocess.run([OBJDUMP, '-d', '--start-address=0x2400AD0',
                          '--stop-address=0x2401DA0', elf], capture_output=True, text=True,
                         env=env).stdout
    # the disassembler shows each `xjp` with its target: "xjp ... (0x02412345)"
    targets = {int(a, 16): int(t, 16) for a, t in
               re.findall(r'^ *([0-9a-f]+):\s+\w+\s+jp\s.*\(0x([0-9A-Fa-f]+)\)', dis, re.M)}
    bad = []
    for addr, func in entries:
        a = int(addr, 16)
        jp = a + 4                     # ext, ext, jp
        if func not in syms:
            bad.append('%s: no such function' % func)
        elif targets.get(jp) != syms[func]:
            bad.append('0x%07X %s: the entry jumps to %s, the function is at 0x%07X'
                       % (a, func, hex(targets[jp]) if jp in targets else 'nothing',
                          syms[func]))
    if bad or not entries:
        print('downloaded-program API broken:')
        for b in bad:
            print('  ' + b)
        sys.exit(1)
    print('OK: %d API entries' % len(entries))


if __name__ == '__main__':
    main()
