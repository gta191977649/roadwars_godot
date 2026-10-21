# Rendering shaders

Original surface and particle draw shaders live here. They are modern host
adaptations of recovered rendering contracts, not claims about original source
filenames. Hash/address evidence is in docs/agent/evidence/billboard.md.

CMake embeds the sources at build time and expands the shared billboard include.
No machine paths or runtime shader-file fallback is required. Surface variants use
RW_* defines supplied by the RenderingServer adapter for original material flags.
Billboarding uses the current render pass's INV_VIEW_MATRIX on the GPU; the CPU
submits static sprite transforms once. Rebuilding after shader edits is automatic.
