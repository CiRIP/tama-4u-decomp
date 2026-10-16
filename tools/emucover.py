"""Which functions do the emulator scenarios reach? (coverage for tools/emucompare.py)

usage:
  python tools/emucover.py [SCENARIO|GROUP ...] [--elf ELF] [--image IMG] [--funcs FILE] [-j N]

Runs IMG (default build/image.bin; its symbols from ELF, default build/image.elf, and the
emulator's from the .sym next to IMG) through the scenarios with an emulator PC hook on the entry of each function listed in FILE (default
tools/validation/functions.txt: one name per line, `#` comments; the functions whose
equivalence the scenarios are meant to show), 24 hooks per run, and prints per function the
scenarios that reached it, then the functions no scenario reached.
"""
import argparse
import concurrent.futures as cf
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import emucompare as EC  # noqa: E402
import scenarios as SC  # noqa: E402

OBJDUMP = os.environ.get('OBJDUMP', 'C:/EPSON/GNU33/objdump')


def symbols(elf):
    out = subprocess.run([OBJDUMP, '-t', elf], capture_output=True, text=True).stdout
    syms = {}
    for l in out.splitlines():
        p = l.split()
        if len(p) >= 4 and re.match(r'^[0-9a-f]{8}$', p[0]):
            syms.setdefault(p[-1], int(p[0], 16))
    return syms


def main():
    ap = argparse.ArgumentParser(usage=__doc__.split('\n\n')[1])
    ap.add_argument('names', nargs='*')
    ap.add_argument('--image', default=os.path.join(ROOT, 'build', 'image.bin'))
    ap.add_argument('--elf', default=os.path.join(ROOT, 'build', 'image.elf'))
    ap.add_argument('--funcs', default=os.path.join(ROOT, 'tools', 'validation', 'functions.txt'))
    ap.add_argument('-j', type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()

    funcs = [l.split('#')[0].strip() for l in open(a.funcs, encoding='utf-8-sig')]
    funcs = [f for f in funcs if f]
    syms = symbols(a.elf)
    missing = [f for f in funcs if f not in syms]
    if missing:
        print('not in %s: %s' % (a.elf, ' '.join(missing)))
    funcs = [f for f in funcs if f in syms]
    byaddr = {syms[f]: f for f in funcs}
    batches = [funcs[i:i + 24] for i in range(0, len(funcs), 24)]

    names = []
    for n in a.names or list(SC.SCENARIOS):
        for m in SC.GROUPS.get(n, [n]):
            if m not in names:
                names.append(m)
    order = []

    def add(n):
        if n in order:
            return
        dep = SC.SCENARIOS[n].get('after')
        if dep:
            add(dep)
        order.append(n)
    for n in names:
        add(n)

    raw = open(a.image, 'rb').read()
    img, _, _ = EC.patch_timer_reads(raw)
    img, _ = EC.patch_rand_churn(img, EC.find_rand(raw))
    sym = os.path.abspath(os.path.splitext(a.image)[0] + '.sym')
    work = os.path.join(EC.CACHE, 'cover')
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    hit = {f: [] for f in funcs}
    saves = {}
    ex = cf.ThreadPoolExecutor(max_workers=a.j)
    # one wave per dependency depth
    depth = {}
    for n in order:
        dep = SC.SCENARIOS[n].get('after')
        depth[n] = depth[dep] + 1 if dep else 0
    for level in range(max(depth.values()) + 1):
        futs = {}
        for n in order:
            if depth[n] != level:
                continue
            sc = SC.SCENARIOS[n]
            sav_in = saves.get(sc.get('after'))
            for b, batch in enumerate(batches):
                extra = []
                for f in batch:
                    extra += ['--pc-hook', '0x%x' % syms[f]]
                d = os.path.join(work, '%s_%d' % (n, b))
                futs[ex.submit(EC.run, img, d, sc, sav_in, False, '4u-clean',
                               ['--syms', sym] + extra)] = (n, b, d)
        for f in cf.as_completed(futs):
            n, b, d = futs[f]
            res, sav = f.result()
            if b == 0:
                saves[n] = sav
            for addr in res['hooks']:
                if addr in byaddr:
                    hit[byaddr[addr]].append(n)
    ex.shutdown()
    for f in funcs:
        print('%-34s %s' % (f, ' '.join(sorted(set(hit[f]))) or '-'))
    never = [f for f in funcs if not hit[f]]
    print('%d of %d functions reached; not reached: %s'
          % (len(funcs) - len(never), len(funcs), ' '.join(never)))


if __name__ == '__main__':
    main()
