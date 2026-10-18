"""Lockstep comparison of a built image with the original firmware in the tamaemu emulator.

usage:
  python tools/emucompare.py [SCENARIO|GROUP ...] [--image IMG] [--syms SYM] [--ref FW]
                             [-j N] [--keep] [--list] [--no-patch] [--log FILE] [-v]

  SCENARIO     names from tools/scenarios.py (default: all); a GROUP name runs its members
  --image IMG  the image to test (default build/image.bin)
  --syms SYM   its symbol file (default build/image.sym, written by make)
  --ref FW     the reference (default FW_4UPlusJP.bin); its results are cached in build/emu/
  -j N         emulator processes in parallel (default: the CPU count)
  --keep       keep the run directories (frames, logs) under build/emu/runs/
  --no-patch   run the images as they are (see "timing" below)
  --log FILE   append a one-line summary per scenario (default build/emu/validation.log)

Both images run headless with the same button script (the reference with tamaemu's "4u"
profile, the image with "4u-clean", see Addresses), from the same
fresh save, or from the save the REFERENCE left at the end of the scenario named in `after`
(moved to each image: its program sectors replaced, see transplant; so the image also has to
load the original firmware's saves), or with own=True each from the save it left itself at
the end of `after` (so the image also has to read back its own saves). A linked scenario
(link=True) runs two cores of the same image whose NFC readers see each other (tamaemu
--link), core B with its own button script and start-save edits. Compared for each scenario:

  frames   a screenshot every `snap` seconds of emulated time (pixel hash)
  lcd      the sequence of LCD controller commands and pixel-write bursts (--lcd-log,
           without cycle counts / pcs): what was drawn, in which order, timing-free
  tones    the sound/timer register writes (--tone-log, values only), in order per timer
  save     the flash bytes the run changed (the .sav against the image it started from)
  end      the final frame, the emulator's exit code and stop status

Timing. The firmware seeds rand() from the free-running timer T16 ch.2 counter (srand(T16_TC2))
and, in the set-up, advances rand() T16_TC2 times; the counter value depends on the exact
cycle a line runs at, so code that is correct but a few cycles faster or slower gives other
random choices. Unless --no-patch, every 16-bit load from T16_TC2 (0x300794) is patched to
load 0 in BOTH images before they run (patch_timer_reads; each image must have the same
number of such loads). The idle loops also call rand() once per turn just to stir it, and
the number of turns in a stretch of time depends on the code's speed: those calls become
nops in both images (patch_rand_churn). Remaining timing differences show as frames that
differ while the lcd/tones/save streams agree; such a scenario is reported "PASS (frames mid-redraw)" when
every pixel of each differing frame equals the same pixel in one of the reference's frames
one snapshot earlier, at the same time or one later, or when it is a single differing
snapshot between equal ones and the LCD streams are equal (a snapshot taken during a redraw).

Addresses. tamaemu's "4u" profile knows two addresses of the original firmware: the code
where its auto-touch puts a store NFC point in the field (the bingo / Gashapon reader,
spot_touch_any_reader) and the RAM byte --stay-awake clears (sys_display_state). This
image has them elsewhere, so it runs with the "4u-clean" profile, which takes them from the
image's symbol file (--syms). Both run with auto-touch and, unless a scenario says
awake=False, --stay-awake. Needs the emulator built from the tama-clean-profile branch
(set TAMAEMU to its tamaemu-headless.exe; default ../tamaemu-clean/build/).

Exit status: 0 all PASS, 1 a FAIL, 2 usage/emulator error.
"""
import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import scenarios as SC  # noqa: E402

EMU = os.environ.get('TAMAEMU') or os.path.normpath(
    os.path.join(ROOT, '..', 'tamaemu-clean', 'build', 'tamaemu-headless.exe'))
HZ = 30000000
CACHE = os.path.join(ROOT, 'build', 'emu')
VERSION = 'v3'

T16_TC2 = 0x300794


def halfwords(data):
    return memoryview(data).cast('H')


def patch_timer_reads(data):
    """Return (patched image, number of loads patched): every `xld.w rY,0x300794` (ext 0x6;
    ext 0x1e; ld.w rY,0x14) followed within 48 halfwords by `ld.uh rX,[rY]` gets that load
    replaced with `ld.w rX,0`."""
    buf = bytearray(data)
    hw = halfwords(buf)
    n = 0
    sites = []
    i = 0
    end = len(hw) - 3
    while i < end:
        if hw[i] == 0xC006 and hw[i + 1] == 0xC01E and (hw[i + 2] & 0xFFF0) == 0x6D40:
            ry = hw[i + 2] & 0xF
            for j in range(i + 3, min(i + 51, len(hw))):
                h = hw[j]
                if (h & 0xFFF0) == (0x2C00 | ry << 4):
                    hw[j] = 0x6C00 | (h & 0xF)
                    n += 1
                    sites.append(0x2000000 + 2 * j)
                    break
            i += 3
        else:
            i += 1
    hw.release()
    return bytes(buf), n, sites


OBJDUMP = os.environ.get('OBJDUMP', 'C:/EPSON/GNU33/objdump')
RAND = 0x240049A          # the original firmware's rand()


def disassemble(data, lo=0x2400000, hi=0x2570000):
    """Raw disassembly of the program sectors: a list of (address, mnemonic, operands,
    target), one per instruction, `ext` prefixes merged into the instruction they extend
    (address = the first ext's)."""
    tmp = os.path.join(CACHE, 'dis_%d.bin' % os.getpid())
    os.makedirs(CACHE, exist_ok=True)
    open(tmp, 'wb').write(data)
    out = subprocess.run([OBJDUMP, '-D', '-b', 'binary', '-m', 'c33',
                          '--adjust-vma=0x2000000', '--start-address=0x%x' % lo,
                          '--stop-address=0x%x' % hi, tmp], capture_output=True, text=True,
                         env=dict(os.environ, CYGWIN='nodosfilewarning')).stdout
    os.remove(tmp)
    insns = []
    start = None
    for line in out.splitlines():
        m = re.match(r'^ *([0-9a-f]+):\s+[0-9a-f]{4}\s+(\S+)\s*(\S*)', line)
        if not m:
            continue
        a = int(m.group(1), 16)
        if m.group(2) == 'ext':
            start = a if start is None else start
            continue
        t = re.search(r'\(0x([0-9A-Fa-f]+)\)', line)
        insns.append((start if start is not None else a, a, m.group(2), m.group(3),
                      int(t.group(1), 16) if t else None))
        start = None
    return insns


def find_rand(data):
    """rand() of an image: the function computing seed * 0x41C64E6D (ext 0x838, ext 0x1939)."""
    i = data.find(bytes.fromhex('38c839d9'), 0x400000, 0x570000)
    return 0x2000000 + i - 8 if i >= 0 else None


def patch_rand_churn(data, rand):
    """Return (image, count): every `call rand` whose result is dropped because the very
    next instruction is a call or a jump (the idle loops' `rand(); sys_idle_tick();`, the
    set-up's birthday loop) becomes nops. These calls only stir the generator once per loop
    turn, and how many turns fit in a stretch of time depends on the code's speed."""
    buf = bytearray(data)
    insns = disassemble(data)
    at = {s: k for k, (s, a, m, op, t) in enumerate(insns)}

    def overwrites_r4(addr):
        """Do the instructions at addr load r4 before anything reads it?"""
        k = at.get(addr)
        for s, a, m, op, t in insns[k:k + 4] if k is not None else []:
            if '%r4' not in op:
                if m.startswith(('call', 'jp', 'j', 'ret')):
                    return False
                continue
            return m.startswith('ld') and op.startswith('%r4,') and '%r4' not in op[4:]
        return False
    n = 0
    for (s1, a1, m1, op1, t1), (s2, a2, m2, op2, t2) in zip(insns, insns[1:]):
        if m1 == 'call' and t1 == rand and not op2.startswith('%') and (
                m2 == 'call' or (m2 == 'jp' and overwrites_r4(t2))):
            for a in range(s1, a1 + 2, 2):
                buf[a - 0x2000000:a - 0x2000000 + 2] = b'\0\0'
            n += 1
    return bytes(buf), n


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def emu_id():
    st = os.stat(EMU)
    return '%d-%d' % (st.st_size, int(st.st_mtime))


def frame_hash(path):
    d = open(path, 'rb').read()
    off = int.from_bytes(d[10:14], 'little')
    return hashlib.sha1(d[off:]).hexdigest()[:12]


LCD_RE = re.compile(r'^(B?)\[lcd\] (cmd .*|RAMWR burst .*)$')
TONE_RE = re.compile(r'^(B?)\[tone\]\s+[\d.]+ms (\S+)\s+\((\w+)\) = (\w+)')


def parse_log(text):
    """The LCD streams of core A and (linked runs, lines tagged "B") core B, the timer
    writes (core B's timers named B:Tn), the end line and the stop reason."""
    lcd, lcd_b, tones = [], [], []
    for line in text.splitlines():
        if line.startswith(('[lcd] ', 'B[lcd] ')):
            m = LCD_RE.match(line)
            if m:
                s = re.sub(r' (cyc|pc)=\S+', '', m.group(2))
                (lcd_b if m.group(1) else lcd).append(s)
        elif line.startswith(('[tone]', 'B[tone]')):
            m = TONE_RE.match(line)
            if m:
                tones.append('%s%s=%s' % ('B:' if m.group(1) else '', m.group(2), m.group(4)))
    end = re.search(r'^\[end\] cycles=(\d+) pc=(\S+) stopped=(\d+)', text, re.M)
    reason = re.search(r'^\[end\] stop reason: (.*)$', text, re.M)
    return lcd, lcd_b, tones, end, reason


def save_delta(sav, image):
    """The bytes the run changed in flash, as (offset, bytes) runs."""
    out = []
    n = min(len(sav), len(image))
    i = 0
    # compare in 4 KB blocks first (fast path)
    blk = 4096
    while i < n:
        if sav[i:i + blk] == image[i:i + blk]:
            i += blk
            continue
        j = i
        e = min(i + blk, n)
        while j < e:
            if sav[j] != image[j]:
                k = j
                while k < e and sav[k] != image[k]:
                    k += 1
                out.append((j, bytes(sav[j:k])))
                j = k
            else:
                j += 1
        i = e
    return out


# The flash the firmware never writes: the program sectors (0x2400000..0x256FFFF; the diary
# pages start at 0x2570000). A saved game is the rest of the
# flash, so a save is moved to another image by taking this range from that image.
PROGRAM = (0x400000, 0x570000)


def transplant(sav, image):
    """The flash image `sav` (a save the reference left) with `image`'s program sectors."""
    a, b = PROGRAM
    return sav[:a] + image[a:b] + sav[b:]


def start_flash(image, sav_in, sc):
    """The flash a run starts from: the image itself (a fresh save) or the given save moved
    to this image, with the scenario's byte edits."""
    data = bytearray(transplant(sav_in, image) if sav_in else image)
    for off, hexbytes in sc.get('edits', []):
        b = bytes.fromhex(hexbytes)
        data[off:off + len(b)] = b
    return bytes(data)


def run(image_bytes, outdir, sc, sav_in, keep_frames, device, extra=()):
    """Run one image for scenario sc into outdir with tamaemu's device profile `device`
    (plus `extra` arguments), from the reference save sav_in (a path, or None for a fresh
    save); return (result dict, path of the save it left)."""
    if os.path.isdir(outdir):
        shutil.rmtree(outdir)
    os.makedirs(outdir)
    img = os.path.join(outdir, 'image.bin')
    open(img, 'wb').write(image_bytes)
    sav = os.path.join(outdir, 'run.sav')
    start = start_flash(image_bytes, open(sav_in, 'rb').read() if sav_in else None, sc)
    if sav_in or sc.get('edits'):
        open(sav, 'wb').write(start)
    link = sc.get('link')
    argv = [EMU, img, '--device', device, '--headless', '--no-auto-link', '--no-state',
            '--sav', sav, '--max-cycles', str(int(sc['seconds'] * HZ)),
            '--snap', repr(sc['snap']), '--lcd-log', '--tone-log']
    if link:
        # a second core B running the same image, its flash from the same start with the
        # scenario's edits_b, the two cores' NFC readers in each other's field
        sav_b = os.path.join(outdir, 'run_b.sav')
        start_b = bytearray(start)
        for off, hexbytes in sc.get('edits_b', []):
            b = bytes.fromhex(hexbytes)
            start_b[off:off + len(b)] = b
        start_b = bytes(start_b)
        open(sav_b, 'wb').write(start_b)
        if not os.path.exists(sav):
            open(sav, 'wb').write(start)
        argv += ['--link', img, '--sav-b', sav_b, '--persist-ram']
        if sc.get('press_b'):
            argv += ['--press-b', SC.press_arg(sc['press_b'])]
    else:
        argv += ['--dump-bmp', 'end.bmp']
    if sc.get('awake', True):
        argv.append('--stay-awake')
    if sc.get('press'):
        argv += ['--press', SC.press_arg(sc['press'])]
    argv += sc.get('args', []) + list(extra)
    t0 = time.time()
    with open(os.path.join(outdir, 'log.txt'), 'wb') as log:
        p = subprocess.run(argv, cwd=outdir, stdout=log, stderr=subprocess.STDOUT)
    wall = time.time() - t0
    text = open(os.path.join(outdir, 'log.txt'), 'r', errors='replace').read()
    lcd, lcd_b, tones, end, reason = parse_log(text)
    if link:
        lcd = lcd + ['--- core B ---'] + lcd_b
        end = re.search(r'^\[link\] done: A cyc=(\d+) B cyc=(\d+)', text, re.M)
    snapdir = os.path.join(outdir, 'snaps')
    frames = {}
    nframes = int(sc['seconds'] / sc['snap']) + 1
    if os.path.isdir(snapdir):
        for f in os.listdir(snapdir):
            m = re.match(r'(snap|link_a|link_b)_(\d+)\.bmp$', f)
            if m:
                k = int(m.group(2)) + (nframes if m.group(1) == 'link_b' else 0)
                if int(m.group(2)) < nframes:
                    frames[k] = frame_hash(os.path.join(snapdir, f))
                else:
                    # the snapshots follow the emulated clock, which jumps ahead while the
                    # CPU sleeps: only those up to the scenario's length are compared
                    os.remove(os.path.join(snapdir, f))
    flist = [frames.get(k) for k in range(nframes * (2 if link else 1))]
    ends = [os.path.join(outdir, f) for f in (('link_a.bmp', 'link_b.bmp') if link else ('end.bmp',))]
    savb = open(sav, 'rb').read() if os.path.exists(sav) else start
    delta = save_delta(savb, start)
    if link:
        sb = open(sav_b, 'rb').read()
        delta += [('B',)] + save_delta(sb, start_b)
    res = {
        'exit': p.returncode,
        'cycles': int(end.group(1)) if end else None,
        'stopped': int(end.group(3)) if end and not link else None,
        'reason': reason.group(1).strip() if reason else '',
        'frames': flist,
        'end': '/'.join(frame_hash(f) if os.path.exists(f) else '-' for f in ends),
        'lcd': lcd,
        'tones': tones,
        'save': sha1(repr(delta).encode()),
        'save_runs': [(o[0], len(o[1])) if len(o) > 1 else o for o in delta][:200],
        'wall': round(wall, 1),
        'hooks': sorted(set(int(x, 16) for x in
                            re.findall(r'^\[pcA\] \S+\s+([0-9a-f]{8}) ', text, re.M))),
    }
    json.dump(res, open(os.path.join(outdir, 'result.json'), 'w'))
    if not keep_frames:
        shutil.rmtree(snapdir, ignore_errors=True)
        os.remove(img)
    return res, sav


def first_diff(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i
    if len(a) != len(b):
        return min(len(a), len(b))
    return None


def pixels(path):
    if not os.path.exists(path):
        return None
    d = open(path, 'rb').read()
    off = int.from_bytes(d[10:14], 'little')
    return memoryview(d[off:]).cast('I')


def in_transition(refdir, canddir, i, nframes=None):
    """Is candidate frame i a mix of the reference's frames i-1, i, i+1 (every pixel equal to
    one of them)? That is a snapshot taken while the screen was being redrawn, a few cycles
    earlier or later than in the reference. (Linked runs: frames nframes.. are core B's.)"""
    def snap(d, k):
        if nframes:
            if k < 0 or k >= 2 * nframes or (k < nframes) != (i < nframes):
                return None
            core, k = ('b', k - nframes) if k >= nframes else ('a', k)
            return pixels(os.path.join(d, 'snaps', 'link_%s_%04d.bmp' % (core, k)))
        return pixels(os.path.join(d, 'snaps', 'snap_%04d.bmp' % k))
    c = snap(canddir, i)
    refs = [r for r in (snap(refdir, i - 1), snap(refdir, i), snap(refdir, i + 1))
            if r is not None]
    if c is None or not refs:
        return False
    for p in range(len(c)):
        v = c[p]
        if not any(len(r) > p and r[p] == v for r in refs):
            return False
    return True


def compare(ref, cand, sc, refdir, canddir):
    issues = []
    fr, fc = ref['frames'], cand['frames']
    diff = [i for i, (x, y) in enumerate(zip(fr, fc)) if x != y]
    same_lcd = ref['lcd'] == cand['lcd']

    def isolated(i):
        """A single differing snapshot between equal ones, with the same LCD command
        stream: the same drawing, caught at another point of a redraw."""
        return (same_lcd and 0 < i < len(fr) - 1 and fr[i - 1] == fc[i - 1]
                and fr[i + 1] == fc[i + 1])
    nf = len(fr) // 2 if sc.get('link') else None
    shifted = [i for i in diff if isolated(i) or in_transition(refdir, canddir, i, nf)]
    hard = [i for i in diff if i not in shifted]
    for k in ('exit', 'stopped', 'reason', 'end'):
        if ref[k] != cand[k]:
            issues.append('%s: %r, reference %r' % (k, cand[k], ref[k]))
    i = first_diff(ref['lcd'], cand['lcd'])
    if i is not None:
        issues.append('lcd stream differs at entry %d of %d/%d: %r vs reference %r'
                      % (i, len(cand['lcd']), len(ref['lcd']),
                         cand['lcd'][i] if i < len(cand['lcd']) else None,
                         ref['lcd'][i] if i < len(ref['lcd']) else None))
    # per timer: how writes to different timers interleave depends on the code's speed
    # (the sound sequencer runs in an interrupt)
    def per_timer(tones):
        out = {}
        for t in tones:
            out.setdefault(t.split('.')[0], []).append(t)
        return out
    rt, ct = per_timer(ref['tones']), per_timer(cand['tones'])
    for timer in sorted(set(rt) | set(ct)):
        r, c = rt.get(timer, []), ct.get(timer, [])
        i = first_diff(r, c)
        if i is not None:
            issues.append('%s writes differ at entry %d of %d/%d: %r vs reference %r'
                          % (timer, i, len(c), len(r), c[i] if i < len(c) else None,
                             r[i] if i < len(r) else None))
    if ref['save'] != cand['save']:
        issues.append('save differs: runs %s vs reference %s'
                      % (cand['save_runs'][:6], ref['save_runs'][:6]))
    if hard:
        issues.append('%d of %d frames differ, first at %gs'
                      % (len(hard), len(fr), hard[0] * sc['snap']))
    if issues:
        verdict = 'FAIL'
    elif shifted:
        verdict = 'PASS (frames mid-redraw: %d at %s)' % (
            len(shifted), ','.join('%gs' % (i * sc['snap']) for i in shifted[:6]))
    else:
        verdict = 'PASS'
    return verdict, issues


def ref_key(ref_sha, sc, sav_key, patched):
    spec = json.dumps({k: sc.get(k) for k in ('seconds', 'snap', 'press', 'args', 'awake',
                                              'edits', 'link', 'press_b', 'edits_b')},
                      sort_keys=True)
    return sha1(('%s|%s|%s|%s|%s|%s' % (VERSION, ref_sha, emu_id(), spec, sav_key, patched))
                .encode())[:16]


def main():
    ap = argparse.ArgumentParser(usage=__doc__.split('\n\n')[1])
    ap.add_argument('names', nargs='*')
    ap.add_argument('--image', default=os.path.join(ROOT, 'build', 'image.bin'))
    ap.add_argument('--syms', default=None)
    ap.add_argument('--ref', default=os.path.join(ROOT, 'FW_4UPlusJP.bin'))
    ap.add_argument('-j', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--keep', action='store_true')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--no-patch', action='store_true')
    ap.add_argument('--log', default=os.path.join(CACHE, 'validation.log'))
    ap.add_argument('-v', action='store_true')
    a = ap.parse_args()

    if a.list:
        for name, sc in SC.SCENARIOS.items():
            print('%-22s %5gs %s%s' % (name, sc['seconds'],
                                     ('after %s: ' % sc['after']) if sc.get('after') else '',
                                     sc.get('doc', '')))
        for g, members in SC.GROUPS.items():
            print('group %-16s %s' % (g, ' '.join(members)))
        return 0
    if a.syms is None:
        a.syms = os.path.splitext(a.image)[0] + '.sym'
    a.syms = os.path.abspath(a.syms)
    for p in (EMU, a.image, a.syms, a.ref):
        if not os.path.exists(p):
            print('missing: %s' % p)
            return 2
    names = []
    for n in a.names or list(SC.SCENARIOS):
        for m in SC.GROUPS.get(n, [n]):
            if m not in SC.SCENARIOS:
                print('unknown scenario %s' % m)
                return 2
            if m not in names:
                names.append(m)
    # dependencies first
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

    raw_ref = open(a.ref, 'rb').read()
    raw_cand = open(a.image, 'rb').read()
    ref_p, nr, _ = patch_timer_reads(raw_ref)
    cand_p, nc, _ = patch_timer_reads(raw_cand)
    if nr != nc:
        print('timer-read patch: %d loads in the reference, %d in the image' % (nr, nc))
        return 2
    ref_p, nr = patch_rand_churn(ref_p, find_rand(raw_ref))
    cand_p, nc = patch_rand_churn(cand_p, find_rand(raw_cand))
    if nr != nc:
        print('rand churn patch: %d calls in the reference, %d in the image' % (nr, nc))
        return 2
    # (reference, image) to run for a scenario: patched unless --no-patch or patch=False
    images = {True: (ref_p, cand_p), False: (raw_ref, raw_cand)}

    def patched_for(sc):
        return not a.no_patch and sc.get('patch', True)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    rundir = os.path.join(CACHE, 'runs', stamp)
    os.makedirs(rundir, exist_ok=True)

    results = {}       # name -> (ref, cand, ref_sav, cand_sav)
    pending = list(order)
    futures = {}
    ex = cf.ThreadPoolExecutor(max_workers=max(1, a.j))

    def ref_job(n, sav_in, sav_key):
        sc = SC.SCENARIOS[n]
        p = patched_for(sc)
        ref_img = images[p][0]
        key = ref_key(sha1(ref_img), sc, sav_key, p)
        d = os.path.join(CACHE, 'ref', '%s_%s' % (n, key))
        if os.path.exists(os.path.join(d, 'done')):
            return (json.load(open(os.path.join(d, 'result.json'))), os.path.join(d, 'run.sav'),
                    key, d)
        # one directory per thread: the same reference can be wanted by two jobs at once
        tmp = d + '.tmp%d_%d' % (os.getpid(), threading.get_ident())
        run(ref_img, tmp, sc, sav_in, True, '4u')
        for attempt in range(20):
            if os.path.exists(os.path.join(d, 'done')):     # the other job finished it first
                shutil.rmtree(tmp, ignore_errors=True)
                break
            shutil.rmtree(d, ignore_errors=True)
            try:
                os.rename(tmp, d)
                open(os.path.join(d, 'done'), 'w').close()
                break
            except OSError:     # Windows: a file in it still open (emulator exit, scanner)
                if attempt == 19:
                    raise
                time.sleep(0.5)
        return (json.load(open(os.path.join(d, 'result.json'))), os.path.join(d, 'run.sav'),
                key, d)

    def cand_job(n, sav_in):
        sc = SC.SCENARIOS[n]
        d = os.path.join(rundir, n)
        res, sav = run(images[patched_for(sc)][1], d, sc, sav_in, True, '4u-clean',
                       ['--syms', a.syms])
        return res, sav, d

    ref_keys = {}
    failed = False
    done_ref, done_cand = {}, {}
    while pending or futures:
        for n in list(pending):
            dep = SC.SCENARIOS[n].get('after')
            own = SC.SCENARIOS[n].get('own')
            if dep and (dep not in done_ref or (own and dep not in done_cand)):
                continue
            pending.remove(n)
            # both start from the save the reference left (see transplant), or with own=True
            # each from the save it left itself (the image reads back what it wrote)
            rs = cs = done_ref[dep][1] if dep else None
            if dep and own:
                cs = done_cand[dep][1]
            futures[ex.submit(ref_job, n, rs, ref_keys.get(dep, ''))] = ('ref', n)
            futures[ex.submit(cand_job, n, cs)] = ('cand', n)
        if not futures:
            break
        donef, _ = cf.wait(list(futures), return_when=cf.FIRST_COMPLETED)
        for f in donef:
            kind, n = futures.pop(f)
            try:
                r = f.result()
            except Exception as e:  # noqa: BLE001
                print('%s %s: error %s' % (kind, n, e))
                return 2
            if kind == 'ref':
                done_ref[n] = (r[0], r[1], r[3])
                ref_keys[n] = r[2]
            else:
                done_cand[n] = r
            if n in done_ref and n in done_cand and n not in results:
                sc = SC.SCENARIOS[n]
                verdict, issues = compare(done_ref[n][0], done_cand[n][0], sc,
                                          done_ref[n][2], done_cand[n][2])
                results[n] = verdict
                cw = done_cand[n][0]['wall']
                print('%-22s %s  (%gs emulated, %gs wall)' % (n, verdict, sc['seconds'], cw))
                for s in issues:
                    print('    ' + s)
                if verdict.startswith('FAIL'):
                    failed = True
                    print('    run dir: %s' % os.path.join(rundir, n))
                elif not a.keep:
                    shutil.rmtree(os.path.join(rundir, n, 'snaps'), ignore_errors=True)
                with open(a.log, 'a', encoding='utf-8') as lf:
                    lf.write('%s %s img=%s %s: %s%s\n' % (
                        stamp, n, sha1(raw_cand)[:10],
                        'patched' if patched_for(sc) else 'unpatched', verdict,
                        (' | ' + ' | '.join(issues)) if issues else ''))
    ex.shutdown()
    npass = sum(1 for v in results.values() if v.startswith('PASS'))
    print('%d of %d scenarios pass' % (npass, len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
