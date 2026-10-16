# -*- coding: utf-8 -*-
"""Character set of the game text (provided by the user, verified 2026-10-14 against all
134 KB of text in .rodata: every code decodes to natural Japanese).

Text strings are big-endian 16-bit codes: 04 xx = charTable[xx], 00 00 = end of string.
Control codes: 04 FF = line break (between the lines of a message), FF FE = end of a row in
the name-entry keyboard grids (0x253ADB0, 0x253AF0C), 04 00 = empty slot. Unknown glyphs
(table '?'): C7, D0-DF, FE, used only once each in the keyboard grid at 0x253AF0C.
0x5D is a real '?'. Some tables hold bare indices without the 04 (e.g. the voiced-kana
list used for dakuten in name entry).
"""
charTable = [
''       , u'あ', u'い', u'う', u'え', u'お', u'か', u'き', u'く', u'け', u'こ', u'さ', u'し', u'す', u'せ',
u'そ', u'た', u'ち', u'つ', u'て', u'と', u'な', u'に', u'ぬ', u'ね', u'の', u'は', u'ひ', u'ふ', u'へ',
u'ほ', u'ま', u'み', u'む', u'め', u'も', u'や', u'ゆ', u'よ', u'ら', u'り', u'る', u'れ', u'ろ', u'わ',
u'を', u'ん', u'ぁ', u'ぃ', u'ぅ', u'ぇ', u'ぉ', u'っ', u'ゃ', u'ゅ', u'ょ', u'が', u'ぎ', u'ぐ', u'げ',
u'ご', u'ざ', u'じ', u'ず', u'ぜ', u'ぞ', u'だ', u'ぢ', u'づ', u'で', u'ど', u'ば', u'び', u'ぶ', u'べ',
u'ぼ', u'ぱ', u'ぴ', u'ぷ', u'ぺ', u'ぽ', u'ー', u'～', u'…', u'､', u'｡', '(', ')', u'｢', u'｣', '.',
u'・', '!','?' , '&', u'⬤', u'✝', u'♥', u'☼', u'★', '@', u'♪', u'╬', u'↑', u'↓', u'→', u'←',
'$'     , ' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', u'ア', u'イ', u'ウ', u'エ', u'オ', u'カ', u'キ',
u'ク', u'ケ', u'コ', u'サ', u'シ', u'ス', u'セ', u'ソ', u'タ', u'チ', u'ツ', u'テ', u'ト', u'ナ', u'ニ',
u'ヌ', u'ネ', u'ノ', u'ハ', u'ヒ', u'フ', u'ヘ', u'ホ', u'マ', u'ミ', u'ム', u'メ', u'モ', u'ヤ', u'ユ',
u'ヨ', u'ラ', u'リ', u'ル', u'レ', u'ロ', u'ワ', u'ヲ', u'ン', u'ァ', u'ィ', u'ゥ', u'ェ', u'ォ', u'ッ',
u'ャ', u'ュ', u'ョ', u'ガ', u'ギ', u'グ', u'ゲ', u'ゴ', u'ザ', u'ジ', u'ズ', u'ゼ', u'ゾ', u'ダ', u'ヂ',
u'ヅ', u'デ', u'ド', u'バ', u'ビ', u'ブ', u'ベ', u'ボ', u'パ', u'ピ', u'プ', u'ペ', u'ポ', '?', '?', '?',
'?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?','?', 'A', 'B',
'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
'i', '_', '?', '?', '?', '?'
]
