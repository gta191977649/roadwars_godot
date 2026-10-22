# Frontend localization and glyph order

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Retained read-only Ghidra results: `frontend_localization_ghidra.json`.
This is static analysis and reconstruction testing, not original executable observation.

## Confirmed lookup behavior

`0045f847` replaces a requested `.txt` suffix with `.bin`. Its read loop adds one to each
encoded byte modulo 256. Localization BIN decoding therefore has the opposite direction
to Configuration PSE decoding. Decoded records use TAB-separated fields and LF record
terminators. Double-quote bytes are ignored rather than interpreted as CSV quoting.
The first field is the English lookup key. Matching uses case-sensitive `strcmp` and
accepts the first matching record. Language zero preserves the key; languages one onward
advance through successive fields.

At `0045fc03`, each language iteration resets only the output write index, not the output
buffer. A field containing no non-quote bytes therefore retains the previous output. The
portable lookup preserves this behavior. Missing keys recover the supplied key through
the final fallback at `0045fd38..0045fd84`; the portable API additionally returns
`Result::not_found` so callers can report missing data without inventing translations.
Invalid language indices and malformed/oversized files receive explicit failures.

The legacy function may convert and delete plaintext input files and show Win32 dialogs.
The portable implementation only reads BIN files and does not reproduce those filesystem
side effects. Its representation is a new bounded table owner, not an original class claim.
Load/decode failures preserve the last valid table transactionally. The source mapping is
partial and does not establish complete recovery of the legacy function.

All eight installed localization tables were inspected through the existing archive
reader. They contain seven fields per row, with byte-oriented translations for English,
German, Dutch, French, Spanish, Italian and Portuguese. There is no Unicode conversion
in the recovered lookup. Text is retained in the original atlas character encoding.

## Confirmed bitmap glyph orders

Ordered alphabet bytes were read from the hash-matched executable, mapping PE virtual
addresses through section headers before reading. No sibling project was consulted.

| Font | Alphabet address | Glyph slots | Evidence |
| --- | --- | ---: | --- |
| 16ScreenTitleFont | `0061c770` | 56 | `004a9879` calls `004e08ab` |
| 16Font12Imp | `0061c7e0` | 138 | `004a9879` calls `004e08ab` |
| 16FontArial15 | `0061c8a0` | 138 | `004a9879` calls `004e08ab` |
| 16Font18Imp | `00619088` | 140 | `0047837d` copies 141 bytes including NUL |

The complete byte orders are preserved in `frontend_localization.cpp`, including repeated
`0x9c` slots. The options font has a different ASCII punctuation/digit order from the
other body fonts; those alphabets must not be interchanged. Title alphabet starts with
uppercase A-Z followed by extended glyphs and ends with hyphen and apostrophe. At
`004e14dc`, space width is exactly integer first-glyph-width / 2 + 1, corroborating the
existing `HudBitmapFont` implementation.

## Validation

`frontend_localization_test.cpp` was compiled directly using GNU C++20 with warnings and
passed both synthetic and original-asset checks. Tests cover byte decoding, quotes,
case-sensitive/duplicate matching, language selection, empty-column carry-forward,
fallback/error results, bounded malformed input and transactional replacement. The
configured original installation was supplied only at test invocation. All eight BIN
tables loaded, and SELECT GAME matched German and Portuguese reference records.
These tests validate format and reconstruction behavior, not binary runtime equivalence.
