# Inventory and progress tracking

## Sources and identity

`docs/functions.csv` is the canonical human-editable registry. `docs/Progress.md` is deterministic generated output with collapsed tables. `evidence/roadwars_inventory.json` stores 8,603 internal functions and 121 RTTI types collected through all Ghidra MCP pagination cursors. The MCP list omits 692 imported functions included in program metadata; these were supplemented from the SHA256-matched executable's PE32 import table for a total of 9,295 function entries.

Internal addresses are original Ghidra entry points. External addresses are PE IAT **slot** virtual addresses, not function bodies and not Ghidra synthetic EXTERNAL addresses. Namespaces matching an exact known RTTI class are linked; other ownership remains empty. Class identity uses its RTTI descriptor address, including types whose name begins `struct`.

Stable IDs have format `<binary_sha256>:<kind>:<lowercase-address>`. Names may change without changing identity. Do not replace class IDs when assigning recovered names. Future non-RTTI classes need a separately documented stable binary address anchor and evidence before registration; do not fabricate class rows from guesses.

## Refresh

The initial export is retained and can be imported directly:

```text
python scripts/import_inventory.py docs/agent/evidence/roadwars_inventory.json
python scripts/progress.py update
python scripts/progress.py check
```

To refresh from Ghidra, run `scripts/ghidra/ExportRoadWarsInventory.java` in the already-open analyzed program, giving a new JSON output path under `docs/agent/evidence`. It reads internal functions and named RTTI descriptors without modifying the program. Cancellation must not be accepted as a complete export. Confirm class coverage against the previous snapshot; descriptor naming can vary across Ghidra versions.

Then supplement that JSON using `scripts/ghidra/complete_pe_inventory.py <export.json> <matching-executable>`. It verifies the executable hash, adds imports using the same IAT identity convention and confirms named five-byte E9 thunks directly from their bytes. The original binary is only read.

Import using `scripts/import_inventory.py <export.json>`. Counts and duplicate IDs must validate. A changed binary hash is rejected. Missing old entries are reported and cause a failed import unless `--allow-missing` explicitly accepts retaining them. Human statuses, source mappings, notes and confidence are preserved. Never silently drop old rows to improve completion.

## Fields and state

CSV fields: `id,binary_sha256,kind,address,original_name,class_id,recovered_name,category,scope,status,confidence,source_path,source_symbol,evidence,notes`.

- `kind`: class or function. `category`: game, library, thunk, unknown.
- `scope`: required, excluded, undecided. Exclusion requires library/thunk evidence. Unknown and undecided entries remain visible unresolved work.
- `status`: not_started, analyzing, understood, implemented, verified. Scope and confidence do not advance status.
- `confidence`: unknown, hypothesis, high, confirmed.
- `source_path`: generated semicolon-separated repository-relative paths from source markers; do not edit manually. `source_symbol`: manually maintained recovered C++ symbol name(s).
- `evidence`: retained evidence or behavioral-test reference(s). `notes`: human explanation, including original path/module provenance. Separate multiple references with semicolons.

Add one full `// RE: <id>` line per mapped original function in implementation source, including split or merged implementations. Marker scanning is restricted to project C++ under game_src and platform, excluding third_party. It updates paths but never counts code as implemented automatically.

Advance a function explicitly:

```text
python scripts/progress.py set <id> --status implemented
python scripts/progress.py set <id> --status verified --evidence <reference-test-report>
```

Implemented and verified require a live source mapping. The verified command requires explicit validation evidence, not merely the inventory file. Agents remain responsible for the relevance and truth of that evidence. Class status is derived from the least advanced known required member; no required members means not_started. This aggregate does not assert that all original members have been discovered.

## Checks and interpretation

`progress.py check` is read-only and fails on duplicate or unstable identities, invalid enums, mixed binary hashes, broken class references, unknown source markers, stale mappings, unsupported exclusions, missing completion evidence, stale class aggregates or outdated generated Markdown.

The dashboard reports category coverage, undecided scope and unassigned ownership separately from implementation rates. The denominator for completion is only required functions; zero required functions yields 'not yet established', not 100%. Tables include excluded and unknown entries. All `<details>` omit `open` so they start collapsed.

## Additional class symbols and scaffolds

The current registry also includes 11 non-RTTI symbols directly confirmed as Class by Ghidra symbols.get, for 132 types total. Evidence is retained in evidence/class_namespace_symbols.json. Their class symbols have NO ADDRESS, so their documented stable class IDs use the lowest member address known at discovery as an anchor; this is not a claim that an RTTI structure exists there. Keep these IDs stable as new members are found. Non-Class namespace symbols are not promoted into classes.

Source navigation also scans RE-STUB markers. A stub-only mapping is rejected for implemented/verified status; actual RE mappings remain necessary. Read class_generation.md before regenerating or replacing stubs.
