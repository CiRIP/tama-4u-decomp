"""Extract the flash contents outside the program from your firmware into assets/.

usage: python tools/extract_assets.py [FIRMWARE]     (default FW_4UPlusJP.bin)

Run once after cloning: the firmware and the extracted assets are not in the repository.
The linker script (ldscripts/firmware.ld) places each file at its flash address; the rest of
the flash outside the program is 0xFF.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# name, first and end offset in the 8 MB flash image (flash address - 0x2000000)
ASSETS = [
    ('save_0040000', 0x040000, 0x042000),   # the scratch sector's factory content
    ('gfx_0080000', 0x080000, 0x26D890),    # pictures, fonts (the test mode sums both)
    ('gfx_0340000', 0x340000, 0x3D92B0),
    ('save_05D0000', 0x5D0000, 0x5D1000),   # factory data in the record sectors
    ('save_06E0000', 0x6E0000, 0x6E2000),   # the default background
]


def main():
    fw = open(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'FW_4UPlusJP.bin'),
              'rb').read()
    d = os.path.join(ROOT, 'assets')
    os.makedirs(d, exist_ok=True)
    for n, a, b in ASSETS:
        open(os.path.join(d, n + '.bin'), 'wb').write(fw[a:b])
        print('%-14s %06X..%06X %7d bytes' % (n, a, b, b - a))


if __name__ == '__main__':
    main()
