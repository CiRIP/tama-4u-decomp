# -*- coding: utf-8 -*-
"""Game text in C sources: _("...") string literals in the game's character set.

The build runs every source file through this before the compiler (src/X.c ->
build/make/pp/X.c): each _("...") becomes the byte initializer the game reads, the rest of
the file is copied unchanged and keeps its line numbers.

    const u8 D_2536DD8[] = _("さつえいする?");
    ->  const u8 D_2536DD8[] = { 0x04,0x0b, 0x04,0x12, ..., 0x04,0x5d, 0x00,0x00 };

Encoding (tools/charset.py): every character is the big-endian halfword 04 xx with
charTable[xx] == the character; the string ends with 00 00. Inside the quotes:
    \\n       line break (04 FF)
    \\0       an extra 00 00 (some files end their strings with two)
    \\c       at the end: no terminator (a string that fills its row of a fixed-width table
             exactly, like a C string literal that fills its char array)
    {XX}     04 XX: a code without a character of its own (unknown glyphs, {00} = empty slot)
    {XXXX}   the raw halfword XXXX (e.g. {FFFE}, end of a keyboard row)
Adjacent literals inside one _() are joined, like C string literals.

    python tools/textpp.py IN.c OUT.c        preprocess one file (the build does this)
"""
import re
import sys

from charset import charTable

NEWLINE = (0x04, 0xFF)

# character -> code, for the characters the table has exactly once ('?' is also the table's
# placeholder for unknown glyphs: the real one is 0x5D)
ENCODE = {}
for _code, _ch in enumerate(charTable):
    if _ch and _ch != '?' and charTable.count(_ch) == 1:
        ENCODE[_ch] = _code
ENCODE['?'] = 0x5D
DECODE = {code: ch for ch, code in ENCODE.items()}


class TextError(Exception):
    pass


def encode(text):
    """The contents of a _("...") literal (escapes still in it) -> bytes, terminator added."""
    out = bytearray()
    terminator = b'\0\0'
    if text.endswith('\\c') and not text.endswith('\\\\c'):
        text = text[:-2]
        terminator = b''
    k = 0
    while k < len(text):
        c = text[k]
        if c == '\\':
            e = text[k + 1:k + 2]
            if e == 'n':
                out += bytes(NEWLINE)
            elif e == '0':
                out += b'\0\0'
            elif e in ('"', '\\'):
                raise TextError('character %r is not in the character set' % e)
            else:
                raise TextError('unknown escape \\%s' % e)
            k += 2
            continue
        if c == '{':
            m = re.match(r'\{([0-9A-Fa-f]{2}|[0-9A-Fa-f]{4})\}', text[k:])
            if not m:
                raise TextError('bad {..} escape at %r' % text[k:k + 8])
            v = int(m.group(1), 16)
            out += bytes((4, v)) if len(m.group(1)) == 2 else bytes((v >> 8, v & 0xFF))
            k += m.end()
            continue
        if c not in ENCODE:
            raise TextError('character %r (U+%04X) is not in the character set' % (c, ord(c)))
        out += bytes((4, ENCODE[c]))
        k += 1
    return bytes(out + terminator)


def decode(data):
    """bytes (a whole string with its final 00 00) -> the text for _("..."), or None when
    the bytes are not one: an odd length or no final terminator."""
    if len(data) < 2 or len(data) % 2 or data[-2:] != b'\0\0':
        return None
    out = []
    for k in range(0, len(data) - 2, 2):
        h, l = data[k], data[k + 1]
        if (h, l) == (0, 0):
            out.append('\\0')
        elif (h, l) == NEWLINE:
            out.append('\\n')
        elif h == 4 and l in DECODE:
            out.append(DECODE[l])
        elif h == 4:
            out.append('{%02X}' % l)
        else:
            out.append('{%02X%02X}' % (h, l))
    return ''.join(out)


def initializer(data):
    return '{ ' + ', '.join('0x%02x,0x%02x' % (data[k], data[k + 1])
                            for k in range(0, len(data), 2)) + ' }'


# C tokens to skip while looking for _(: comments, string and character literals
SKIP = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'', re.S)
CALL = re.compile(r'(?<![\w$])_\s*\(')
LITERAL = re.compile(r'\s*"((?:[^"\\\n]|\\.)*)"')


def preprocess(src, name='<input>'):
    out = []
    pos = 0
    while True:
        m_call = CALL.search(src, pos)
        if not m_call:
            out.append(src[pos:])
            break
        # a match inside a comment or literal doesn't count
        m_skip = SKIP.search(src, pos)
        if m_skip and m_skip.start() < m_call.start():
            out.append(src[pos:m_skip.end()])
            pos = m_skip.end()
            continue
        out.append(src[pos:m_call.start()])
        k = m_call.end()
        parts = []
        while True:
            m = LITERAL.match(src, k)
            if not m:
                break
            parts.append(m.group(1))
            k = m.end()
        m = re.compile(r'\s*\)').match(src, k)
        line = src.count('\n', 0, m_call.start()) + 1
        if not parts or not m:
            raise TextError('%s:%d: _( must hold string literals' % (name, line))
        try:
            data = encode(''.join(parts))
        except TextError as e:
            raise TextError('%s:%d: %s' % (name, line, e))
        # keep the line count: the newlines the call spanned follow the initializer
        out.append(initializer(data) + '\n' * src.count('\n', m_call.start(), m.end()))
        pos = m.end()
    return ''.join(out)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = open(sys.argv[1], encoding='utf-8', errors='surrogateescape').read()
    try:
        text = preprocess(src, sys.argv[1])
    except TextError as e:
        sys.exit('textpp: %s' % e)
    # point the compiler's messages at the real source
    text = '#line 1 "%s"\n' % sys.argv[1].replace('\\', '/') + text
    open(sys.argv[2], 'w', encoding='utf-8', errors='surrogateescape', newline='\n').write(text)


if __name__ == '__main__':
    main()
