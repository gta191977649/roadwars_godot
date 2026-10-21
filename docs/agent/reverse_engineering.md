# Reverse-engineering workflow

## Reference identity

Initial Ghidra program: `RoadWars`, project `hp2_ps2`, x86 little-endian 32-bit Windows PE, image base `00400000`.

Authoritative input SHA256:

```text
93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8
```

This is the Kopio executable. The other installed RoadWars.exe has a different hash; do not reuse addresses across them. Machine installation paths belong in ignored local configuration. Do not execute or modify original binaries for an inventory refresh.

## Investigation sequence

1. Read existing inventory and evidence. Select the exact binary identity and original address.
2. Follow relevant strings, callers/callees, memory reads and file formats. Use small targeted Ghidra queries. Preserve original program state unless a separate task authorizes annotation.
3. Record observations separately from inferred object ownership, calling conventions and hypotheses. Cite original addresses and retained evidence paths, not only agent conversation text.
4. Recover behavior and contracts in portable C++. Follow original source organization when evidenced; otherwise keep source in unresolved with explicit IDs. Record original source-path evidence before promoting a folder/class name.
5. Add tests with appropriate original/reference evidence. Update CSV status explicitly and regenerate progress.

Confidence is independent of status: `unknown`, `hypothesis`, `high`, `confirmed`. Merely compiling a reconstruction is not `verified`. Implemented means an actual mapped implementation exists; verified means an explicit reference-behavior check supports the claim.

## Current limits

The inventory covers currently recognized functions and RTTI types. Unrecognized functions and original source-only classes may remain. Most function ownership is unresolved; library classes are not proof of gameplay classes. The observed outer-loop variable-delta clamp and dispatch are retained in entry_loop_ghidra.json. Internal simulation timestep, vehicle dynamics, collision, AI, rendering state interpretation and source project layout remain unestablished. The triangle is new infrastructure only.

The project is independent. Do not import code or findings from neighboring roadwars-re or roadwars_tools repositories. Original assets stay outside version control.
