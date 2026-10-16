"""Emulator scenarios for tools/emucompare.py.

A scenario: seconds of emulated time, a button script, the screenshot interval, extra
emulator arguments, and optionally `after`: start from the save the named scenario left
(the saves chain per image). Buttons: A = select / next, B = confirm, C = back.

Press scripts are written as 'T:BTN[:DUR] ...': T in seconds from the start (or +T after the
previous press), BTN one or more of A B C (pressed together), DUR the hold time in seconds
(default 0.15). The emulator takes at most 32 presses per run; longer flows chain scenarios.
"""

HZ = 30000000


def press_arg(spec):
    out = []
    t = 0.0
    for item in spec.split():
        parts = item.split(':')
        t = t + float(parts[0][1:]) if parts[0].startswith('+') else float(parts[0])
        mask = 0
        for ch in parts[1]:
            mask |= {'A': 1, 'B': 2, 'C': 4}[ch]
        dur = float(parts[2]) if len(parts) > 2 else 0.15
        out.append('%d:%d:%d' % (int(round(t * HZ)), mask, int(round(dur * HZ))))
    if len(out) > 32:
        raise ValueError('more than 32 presses: %s' % spec)
    return ','.join(out)


def seq(start, step, buttons):
    """'B B A' at start, start+step, ... -> press items."""
    return ' '.join('%g:%s' % (start + i * step, b) for i, b in enumerate(buttons.split()))


SCENARIOS = {}
GROUPS = {}


def scenario(name, seconds, press='', snap=0.5, after=None, args=(), awake=True, edits=(),
             patch=True, own=False, doc=''):
    """awake: --stay-awake (the emulator keeps the display on); edits: (flash offset, hex
    bytes) written into the starting flash of both images; patch: run the images with the
    timer-counter loads patched (tools/emucompare.py; off where the ROM check must see the
    images as built); own: each image starts from the save it left itself at the end of
    `after` (a reboot from its own save) instead of the reference's."""
    SCENARIOS[name] = dict(seconds=seconds, press=press, snap=snap, after=after,
                           args=list(args), awake=awake, edits=list(edits), patch=patch,
                           own=own, doc=doc)


def probe(t):
    """Emulator arguments: a store NFC point (Touch Spot) in the field from t seconds on."""
    return ['--nfc-probe', str(int(t * HZ))]


# ---- first boot -----------------------------------------------------------------------------

# the clock and birthday set-up (B through the fields), the name entry (B opens the character
# grid, B picks, A to the end, B confirms), then the egg hatches
SETUP = (seq(10, 1, 'B B B B B B B B B') + ' ' + seq(20, 2, 'B B B B') + ' 28:B 29:B ' +
         seq(30, 1, 'A A A A A A A') + ' 38:B')
scenario('boot', 20, '', snap=0.25, doc='power-on: start-up picture, clock screen')
scenario('walk', 60, ' '.join(
    ['%d:B' % s for s in (10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26)] +
    ['%d:A' % s for s in range(27, 41)] + ['42:B', '45:B', '48:C', '51:C:1', '55:B']),
    snap=0.25, doc='set-up and every keyboard row of the name entry (emutest walk)')
scenario('setup', 300, SETUP, doc='first boot to the hatched baby')

# the factory test mode: A C A B C on the start-up picture; colours (A B C A), the
# power-down (A wakes, twice), the RTC count (A), the version (A), battery, then B+C: the ROM
# checksum (A: program, OK -> A), NFC init (A), the tag tests (B each) ... Unpatched, so the
# ROM check sums the images as built. The power-down makes the emulated clock jump ahead.
TESTMODE = ('5.6:A 6.0:C 6.4:A 6.8:B 7.2:C 9:A 11:B 13:C 15:A 18:A 21:A 25:A 29:A 33:A '
            '37:BC 41:A 45:A 49:A 53:B 58:B 63:B 68:A')
scenario('testmode', 90, TESTMODE, snap=30, patch=False, doc='the factory test mode')

# ---- from the saved game ---------------------------------------------------------------------

# power-on with a save: "continue" (B), the clock reminder (B), the pet calls (A), the mood
# question (B: the first face), the "registered" message (B); then the room
RESUME = '10:B 13:B 16:A 20:B 24:B'
scenario('resume', 60, RESUME, after='setup', doc='continue the saved game, mood question')


def fuzz(seed, n=29, start=16.0, after='resume', prefix='10:B 13:B'):
    """A seeded random walk: n presses after the prefix, 0.5..3 s apart, A and B more
    often than C, now and then a long hold."""
    import random
    rnd = random.Random(seed)
    items = prefix.split()
    t = start
    for _ in range(n):
        b = rnd.choices('ABC', weights=(45, 40, 15))[0]
        dur = 1.2 if rnd.random() < 0.05 else 0.15
        items.append('%.2f:%s:%g' % (t, b, dur))
        t += rnd.uniform(0.5, 3.0) + dur
    return ' '.join(items), t + 8


for s in range(1, 41):
    p, secs = fuzz(s)
    scenario('fuzz%02d' % s, round(secs), p, after='resume',
             doc='random walk %d from the resumed game' % s)

# ---- saved-game edits ---------------------------------------------------------------------------

# The save (save_game): the pet state (struct PetState) at flash 0x200E000, then the whole
# struct SaveBlock at 0x200E248, loaded in that order: the pet state is in the save twice
# (file offsets 0xE000 and 0xE248 + 0x300 = 0xE548, its offset in the block); the second wins.
PET_COPIES = (0xE000, 0xE548)


def pet_edits(**fields):
    """Edits of struct PetState fields in both copies: name=(offset, size, value)."""
    out = []
    for off, size, value in fields.values():
        for base in PET_COPIES:
            out.append((base + off, value.to_bytes(size, 'little').hex()))
    return out


# a healthy adult まめっち (id 16), fed and happy, with 9999 gotchi points
ADULT = pet_edits(chara=(0x00, 2, 16), row=(0x02, 2, 12), hunger=(0x05, 1, 4),
                  calls=(0x0A, 1, 0), happiness=(0x0C, 1, 40), mistakes=(0x0F, 1, 0),
                  sick=(0x10, 1, 0), status=(0x11, 1, 0), age=(0x14, 4, 0x2000),
                  points=(0x18, 4, 9999), weight=(0x30, 1, 20), stage=(0x46, 1, 3),
                  adult_id=(0x222, 2, 16), hour=(0x85, 1, 10), minute=(0x86, 1, 0))

# ---- growing up (the game clock sped up: --rtc-mult) -------------------------------------------

scenario('grow_child', 360, '10:B 13:B', after='resume', args=['--rtc-mult', '30'], snap=1,
         doc='a game day at 30x: the baby grows into a child')
scenario('grow_adult', 1500, '10:B 13:B', after='grow_child', args=['--rtc-mult', '120'],
         snap=2, doc='two game days at 120x: the child grows into an adult')

# ---- an adult pet (edited save), random walks from it ---------------------------------------

scenario('adult', 60, RESUME, after='resume', edits=ADULT,
         doc='the resumed game with the pet edited into a fed, happy adult with 9999 points')
for s in range(1, 13):
    p, secs = fuzz(100 + s, after='adult')
    scenario('afuzz%02d' % s, round(secs), p, after='adult',
             doc='random walk %d with the adult pet' % s)

# ---- the Touch Spot / NFC reader (a store NFC point in the field: tamaemu --nfc-probe) ----------

# the communication menu (icon 2: A A A B), its last entry Touch Spot (A A A B), with the
# point already in the field; then a few presses through what it shows
COMM_SPOT = '10:B 13:B 30:A 32:A 34:A 36:B 40:A 42:A 44:A 46:B 56:B 60:B 64:B 68:A 72:B'
scenario('spot_menu', 90, COMM_SPOT, after='resume', args=probe(38), snap=1,
         doc='the Touch Spot entry of the communication menu, a store point in the field')
for s in range(1, 7):
    p, secs = fuzz(200 + s)
    scenario('touch%02d' % s, round(secs), p, after='resume', args=probe(14 + 7 * s),
             doc='random walk %d with a store point coming into the field' % s)
for s in range(1, 4):
    p, secs = fuzz(300 + s, after='adult')
    scenario('atouch%02d' % s, round(secs), p, after='adult', args=probe(20 + 9 * s),
             doc='adult random walk %d with a store point coming into the field' % s)

# ---- the display going to sleep (no --stay-awake) ------------------------------------------

scenario('sleep', 150, '10:B 13:B 100:A 103:A 106:B 110:C', after='resume', awake=False, snap=1,
         doc='left alone: the screen goes off, a key wakes it')

# ---- reboots from each image's own save ------------------------------------------------------

REBOOT = '10:B 13:B 16:A 20:B 24:B 30:A 33:B'
scenario('reboot_setup', 45, REBOOT, after='setup', own=True,
         doc='power-on from the save the image itself wrote in setup')
scenario('reboot_fuzz', 45, REBOOT, after='fuzz05', own=True,
         doc='power-on from the save the image itself wrote in fuzz05')
scenario('reboot_adult', 45, REBOOT, after='afuzz03', own=True,
         doc='power-on from the save the image itself wrote in afuzz03')
scenario('reboot_touch', 45, REBOOT, after='touch02', own=True,
         doc='power-on from the save the image itself wrote in touch02')
scenario('reboot_grown', 45, REBOOT, after='grow_adult', own=True,
         doc='power-on from the save the image itself wrote in grow_adult')

GROUPS['quick'] = ['boot', 'walk', 'setup']
GROUPS['grow'] = ['grow_child', 'grow_adult']
GROUPS['fuzz'] = ['fuzz%02d' % s for s in range(1, 41)]
GROUPS['adult'] = ['adult'] + ['afuzz%02d' % s for s in range(1, 13)]
GROUPS['touch'] = (['spot_menu'] + ['touch%02d' % s for s in range(1, 7)] +
                   ['atouch%02d' % s for s in range(1, 4)])
GROUPS['reboot'] = ['reboot_setup', 'reboot_fuzz', 'reboot_adult', 'reboot_touch', 'reboot_grown']
