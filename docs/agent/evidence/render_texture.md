# Original texture orientation and transparency

Program: RoadWars. SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Date: 2026-10-22. Read-only Ghidra output and targeted assembly are retained in
render_texture_ghidra.json. Original class/source-path ownership remains unresolved.

## Confirmed observations

- `004ea597` uppercases the joined texture name and checks `004ecbb6` for basename
  prefix XX before `004ecc3f` for prefix 16. XX requests 4/4/4/4 channels; 16 requests
  5/5/5/1; ordinary names request the default texture-format route. These are filename
  prefixes, not guesses from whether a bitmap happens to contain black pixels.
- `004ea922` selects a supported surface format. Its default path prefers an 8-bit
  palettized texture surface (`004e8dea`) when available, otherwise it searches for
  5/5/5/1 and uses the indexed-to-16-bit converter (`004e9389`). An explicit alpha
  width greater than three uses `004e9ef2`; explicit 16-bit color uses `004e9939`.
- `004e8dea` reads palette and pixel bytes, then copies file row zero to surface row
  zero, incrementing both source row pointer and destination pitch. There is no BMP
  bottom-to-top reversal. `004e9389`, `004e9939` and `004e9ef2` use the same increasing
  source/destination row order. The original mesh UVs therefore expect the on-disk
  bottom-up bitmap row order. Canonical top-down image decoding introduced a vertical
  flip relative to the original surfaces.
- `004e8dea` sets DirectDraw source color key low/high to zero only for X-prefixed
  basenames (`004ecb48`). In an indexed surface this keys **palette index zero**.
  It does not remove every black RGB color or every palette entry near black.
- `004e9389` quantizes palette colors to the selected surface widths and makes
  quantized RGB black transparent only for an X-prefixed name. This is the alternate
  non-palettized hardware path, not the primary indexed-surface rule.
- `004e9939` converts source BGR colors to selected 5/5/5/1 storage. Quantized RGB
  black receives zero alpha, other colors one. Its surface color key is always zero.
- `004e9ef2` replaces the last four filename characters with `Alpha.bmp`, producing
  a companion such as `XXGlassAlpha.bmp`. It reads an indexed color bitmap and an
  indexed alpha bitmap. Alpha comes from the **raw alpha pixel index**, shifted to
  the selected alpha width. The alpha bitmap's palette is read but is not used by
  the alpha expression. ARGB4444 therefore retains the high four bits of color and
  alpha channels. A black RGB color with nonzero alpha is not discarded.

## Actual archive coverage

A read-only inventory through the single config.local.json game_dir found 1,646 BMP
members: 982 indexed 8-bit and 664 24-bit; all heights are positive. Names comprise
351 X-prefixed indexed assets, 85 XX-prefixed indexed assets (including companions),
641 prefix-16 24-bit assets, 546 plain indexed assets, 22 plain 24-bit assets, and one
XX-prefixed 24-bit alpha companion. Dark Forest's X trees/fences use an RGB-black
palette entry zero; there are substantial index-zero transparent areas. This confirms
the intended test-track family but does not establish every archive member as loaded.

## Portable implementation and boundary

`decode_bitmap` remains a bounded generic BMP decoder returning canonical top-down
RGBA. It now retains source orientation and raw indexed pixels. The new
`prepare_original_texture` reproduces original surface row order exactly once and
reports opaque, binary color key, or alpha blend to the renderer. Ordinary indexed
textures use the evidenced palettized color/index-key behavior, preserving their RGB
palette values. Prefix 16 uses 5/5/5/1 quantization; prefix XX uses 4/4/4/4 and requires
its indexed alpha companion. RGBA8 stores normalized 5-bit channels via integer
`channel * 255 / 31` and 4-bit channels via `channel * 17` for the modern upload.

Hardware enumeration, cache/refcount/DirectDraw object construction and the alternate
default-indexed-to-5551 capability route are not reconstructed by this portable helper.
Those full loader functions remain partial RE-STUB mappings. Unsupported top-down
original textures and non-indexed XX alpha companions return explicit failure.
The archive contains one such 24-bit companion; it is not claimed supported merely
because other BMPs decode successfully. No original runtime was executed or compared.

The renderer must resolve the actual texture path before deriving its companion,
call preparation once after generic decoding, leave original mesh UVs unchanged,
and use alpha blending for XX rather than applying binary alpha scissor to it.
The texture helper does not own material blend/depth/cull/filter state.

## Executed checks

Direct GNU C++20 build/run of tests/core/texture_pipeline_test.cpp passed. Tests cover
padded BMP decoding followed by original file-row order; nonblack index-zero keying;
duplicate black palette entries; opaque index zero; XX raw-index alpha despite a
different alpha palette; ARGB4444 and 1555 quantization; missing-alpha transactional
failure. These are reconstructed contract tests, not original-executable equivalence.
