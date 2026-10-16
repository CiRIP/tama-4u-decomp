"""Dump and check TAMAGO item files (the downloadable items, games and outings).

usage: python tools/tamago.py [options] PATH...

PATH = files or directories (searched recursively). A file is either a raw TAMAGO file or
the distributed ".jpg" form (a JPEG preview followed by the TAMAGO data); the data starts
at the first "TAMAGO\\0" in the file.

  (default)        one line per file: type/category/sub-kind, kind byte 0x50, item id,
                   size, the checks, entry offset (programs), game-charset name
  -v               every header field (see notes/findings/tamago.md for the layout)
  --table          per type/category/sub-kind: file count and the header bytes 0x5E..0xFF
                   that are not always zero, with their value counts
  --extract DIR    write each TAMAGO payload (trimmed to its size field) to DIR
  --fw IMAGE       firmware image for the signature tables D_25623D8 / D_2562458
                   (default: FW_4UPlusJP.bin in the repository root, if present)

Programs (type 0x94 / category 2 / sub-kind 1 games, 2 outings) run from flash 0x2000000:
disassemble an extracted payload with
  objdump -b binary -m c33 --adjust-vma=0x2000000 --start-address=0x2000204 -D FILE
(the entry point is 0x2000000 + the big-endian u32 at 0xAE).
"""
import collections
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    from charset import charTable
except ImportError:
    charTable = None

be = lambda b: int.from_bytes(b, 'big')

KINDS = {
    (0x81, 1, 1): 'meal', (0x81, 1, 2): 'snack', (0x81, 2, 0): 'accessory',
    (0x81, 3, 0): 'clothes', (0x81, 4, 1): 'goods', (0x81, 4, 2): 'goods-2',
    (0x81, 6, 1): 'letter?', (0x81, 6, 3): 'card-item', (0x81, 7, 0): 'room',
    (0x81, 8, 0): 'bingo/gasha', (0x94, 2, 1): 'GAME', (0x94, 2, 2): 'OUTING',
    (0x94, 2, 3): 'character',
}


def sig_tables(path):
    if not path or not os.path.exists(path):
        return None
    fw = open(path, 'rb').read()
    val = fw[0x5623D8:0x5623D8 + 128]
    perm = fw[0x562458:0x562458 + 128]
    if any(val[i * 8 + 3] != i << 4 or perm[i * 8 + 3] != i for i in range(16)):
        return None
    return val, perm


def sig_ok(t, tabs):
    val, perm = tabs
    k = t[0x55]
    return all(t[0x52 + perm[(k & 15) * 8 + i]] == val[(k >> 4) * 8 + i]
               for i in range(8) if i != 3)


def game_text(b):
    out = ''
    for i in range(0, len(b) - 1, 2):
        hi, lo = b[i], b[i + 1]
        if hi == 0 and lo == 0:
            break
        if hi == 4 and charTable and lo < len(charTable):
            out += charTable[lo] or '_'
        else:
            out += '{%02X%02X}' % (hi, lo)
    return out


def load(path):
    d = open(path, 'rb').read()
    k = d.find(b'TAMAGO\0')
    if k < 0:
        return None, None
    t = d[k:]
    n = be(t[0x48:0x4c])
    if 0x100 <= n <= len(t):
        t = t[:n]
    return t, k


def checks(t, tabs):
    n = be(t[0x48:0x4c])
    r = []
    r.append('size' if n == len(t) else 'SIZE?')
    r.append('sum' if len(t) > 2 and sum(t[:-2]) & 0xffff == be(t[-2:]) else 'SUM?')
    if tabs:
        r.append('sig' if sig_ok(t, tabs) else 'SIG?')
    return ' '.join(r)


def summary(rel, t, tabs):
    key = (t[0x4e], t[0x4f], t[0x51])
    s = '%02X/%02X/%02X %-11s k%02X id%5d len %5X %-12s' % (
        key + (KINDS.get(key, '?'), t[0x50], be(t[0x5a:0x5c]), len(t), checks(t, tabs)))
    if t[0x4e] == 0x94 and t[0x51] in (1, 2):
        s += ' entry %07X' % (0x2000000 + be(t[0xae:0xb2]))
    return '%s %s | %s' % (s, game_text(t[0x5e:0x72]), rel)


def verbose(rel, t, tabs, k):
    name = t[6:0x30].decode('utf-16-be', 'replace').split('\0')[0]
    print('== %s (TAMAGO data at file offset 0x%X, %s)' % (rel, k, checks(t, tabs)))
    f = [
        ('06 file name (UTF-16BE)', name),
        ('30 container size', '0x%X' % be(t[0x30:0x34])),
        ('34 id', t[0x34:0x48].split(b'\0')[0].decode('latin-1')),
        ('48 size (BE)', '0x%X' % be(t[0x48:0x4c])),
        ('4C/4D', '%02X %02X' % (t[0x4c], t[0x4d])),
        ('4E type/4F category/51 sub-kind', '%02X/%02X/%02X' % (t[0x4e], t[0x4f], t[0x51])),
        ('50 kind', '0x%02X' % t[0x50]),
        ('52 signature (key 0x55)', t[0x52:0x5a].hex(' ')),
        ('5A item id (BE)', be(t[0x5a:0x5c])),
        ('5E name', game_text(t[0x5e:0x72])),
        ('74 price (BE u32)', be(t[0x74:0x78])),
        ('78/79 ?/use animation', '%02X %02X' % (t[0x78], t[0x79])),
        ('7A hunger / 7B happiness', '%d %d' % (t[0x7a], t[0x7b])),
        ('94 class preferences (2 bits)', t[0x94:0x9c].hex(' ')),
        ('9C..A3', t[0x9c:0xa4].hex(' ')),
        ('AE entry offset (BE)', '0x%X' % be(t[0xae:0xb2])),
        ('B2 weight', t[0xb2]),
        ('B8 five trait increments', t[0xb8:0xbd].hex(' ')),
        ('C0 character list (BE u16)', t[0xc0:0xe4].hex(' ')),
        ('F4 date window (decorations)', '%02d/%02d..%02d/%02d' % tuple(t[0xf4:0xf8])),
        ('FF flags', '0x%02X' % t[0xff]),
    ]
    for a, b in f:
        print('  %-34s %s' % (a, b))
    q = 0x200
    if t[0x4e] == 0x81 and t[0x4f] == 2:
        q += 2 + be(t[q:q + 2])
    n = be(t[q:q + 2])
    q += 2
    lens = []
    for _ in range(min(n, 64)):
        lens.append(be(t[q:q + 2]))
        q += 2 + lens[-1]
    print('  %-34s %d blob(s) %s, %d byte(s) after them' % (
        '200 body', n, ' '.join('%X' % x for x in lens[:8]), len(t) - 2 - q))


def table(rows):
    groups = collections.defaultdict(list)
    for rel, t, k in rows:
        groups[(t[0x4e], t[0x4f], t[0x51])].append(t)
    for g in sorted(groups):
        L = groups[g]
        print('== %02X/%02X/%02X %s: %d file(s), kind byte 0x50 %s' % (
            g + (KINDS.get(g, '?'), len(L),
                 dict(collections.Counter('%02X' % t[0x50] for t in L)))))
        for o in range(0x5e, 0x100):
            c = collections.Counter(t[o] for t in L)
            if set(c) != {0}:
                v = sorted(c.items(), key=lambda x: -x[1])
                print('   %03X: %s%s' % (o, ' '.join('%02X:%d' % x for x in v[:10]),
                                         ' ...(%d values)' % len(c) if len(c) > 10 else ''))


def main():
    args = sys.argv[1:]
    if not args or '-h' in args or '--help' in args:
        print(__doc__)
        return
    fw = os.path.join(ROOT, 'FW_4UPlusJP.bin')
    out = None
    mode = 'summary'
    paths = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--fw':
            fw = args[i + 1]
            i += 1
        elif a == '--extract':
            out = args[i + 1]
            i += 1
        elif a == '-v':
            mode = 'verbose'
        elif a == '--table':
            mode = 'table'
        else:
            paths.append(a)
        i += 1
    sys.stdout.reconfigure(encoding='utf-8')
    tabs = sig_tables(fw)
    files = []
    for p in paths:
        if os.path.isdir(p):
            for dp, dn, fn in os.walk(p):
                dn.sort()
                files += [(os.path.join(dp, f), os.path.relpath(os.path.join(dp, f), p))
                          for f in sorted(fn)]
        else:
            files.append((p, os.path.basename(p)))
    rows = []
    for path, rel in files:
        t, k = load(path)
        if t is None or len(t) < 0x100:
            if mode != 'table':
                print('no TAMAGO data | %s' % rel)
            continue
        rows.append((rel, t, k))
        if out:
            os.makedirs(out, exist_ok=True)
            name = rel.replace(os.sep, '__').rsplit('.', 1)[0] + '.bin'
            open(os.path.join(out, name), 'wb').write(t)
        if mode == 'summary':
            print(summary(rel, t, tabs))
        elif mode == 'verbose':
            verbose(rel, t, tabs, k)
    if mode == 'table':
        table(rows)


main()
