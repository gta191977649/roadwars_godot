# Global render-state and sampler audit

Program RoadWars, SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Date 2026-10-22. The original executable was not executed or changed.
Retained Ghidra output: render_global_states_ghidra.json. Enum meanings were checked
against the locally installed MinGW Direct3D legacy d3dtypes.h; no SDK types enter
the portable core.

## Confirmed global state

`004dc015` creates the rendering context and calls `004dbbbe` at `004dc797`.
`004dd1d0` caches render states then calls device vtable offset 0x50;
`004dd230` calls texture stage state at vtable offset 0x94.

| State | Initial value | Meaning |
| --- | ---: | --- |
| RS9 SHADEMODE | 2 | Gouraud interpolation |
| RS15 ALPHATESTENABLE | 1 | Alpha test globally enabled |
| RS21 TEXTUREMAPBLEND | 4 | Legacy modulate-alpha |
| RS23 ZFUNC | 4 | Less-or-equal |
| RS25 ALPHAFUNC | 6 | Initial not-equal, subsequently overridden per object |
| RS27 ALPHABLENDENABLE | 1 | Alpha blending globally enabled |
| RS41 COLORKEYENABLE | 1 | Source color key globally enabled |
| RS19 SRCBLEND | 5 | Source alpha |
| RS20 DESTBLEND | 6 | Inverse source alpha |
| RS137 LIGHTING | 1 | Lighting initially enabled |
| RS139 AMBIENT | 0xffffffff | Initial white ambient |

Stage zero COLOROP=4/MODULATE, COLORARG1=2/TEXTURE, COLORARG2=0/DIFFUSE.
Stage zero ALPHAOP initially MODULATE with texture and diffuse arguments; a
ValidateDevice fallback can select texture alpha alone. Stage one is disabled.
Both stage zero and one explicitly receive MAGFILTER=2 and MINFILTER=2: linear
minification and magnification, not point or anisotropic sampling.

No MIPFILTER assignment occurs in this initializer. More directly, the texture
loaders `004e8dea`, `004e9389`, `004e9939` and `004e9ef2` create surfaces with
DDSCAPS_TEXTURE=0x1000 and no DDSCAPS_MIPMAP or DDSCAPS_COMPLEX chain. Their other
conditional descriptor flag 0x100000 is DDSD_TEXTURESTAGE, not a mip count.
Generating a modern mip chain or enabling trilinear filtering is not justified by
this observed texture path. Use linear filtering on the single uploaded level.

`004d710c` temporarily clamps stage-zero ADDRESSV to 3 while drawing sky, then
restores 1/wrap. U remains under the surrounding texture state. This is a specific
sky override, not a reason to clamp ordinary world textures.

## Object overrides and alpha

`004d2fcd` sets ALPHAREF=255 and ALPHAFUNC=3/equal when object flag 0x80 is set.
Otherwise it sets ALPHAREF=0 and ALPHAFUNC=5/greater. It sets source-alpha / one
for additive flag 2; otherwise source-alpha / inverse-source-alpha. Therefore
fractional alpha and zero-alpha discard must remain distinct; a universal 0.5
cutoff or universal opacity treatment changes the original pipeline.

## Culling and search boundary

No CULLMODE (RS22) assignment appears in `004dbbbe`, `004d2fcd`, or the examined
D3DX creation routine `0050bafe`. The latter creates a device and sets its initial
projection; `0050ba62` sets the viewport. A read-only, hash-bound Capstone scan of
the .text section found no immediate RS22 argument at direct `004dd1d0` calls.
This negative scan does not exclude indirect/state-block/driver defaults.

The original device default and the handedness-to-winding conversion still require
an explicit basis/projection audit before claiming an exact modern cull mode. A
blanket cull-disabled setting is new diagnostic behavior, not a recovered flag.
Do not confuse an alpha/depth flag with a double-sided flag without evidence.
