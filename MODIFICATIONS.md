# 1oom Modifications For This Project

Date: 2026-05-06

This repository maintains a project-specific 1oom tree as the executable oracle
for the Godot port. It is no longer an unmodified upstream 1oom behavior target.
Intentional changes made here must be treated as canonical for the Godot port
unless a later decision record supersedes them.

## Classic+ AI Is Canonical

The old selectable Classic-vs-Classic+ AI split has been removed for this
project. Classic+ behavior is now the only supported AI behavior.

This is an intentional deviation from upstream/original 1oom, where
`GAME_AI_CLASSIC` and `GAME_AI_CLASSICPLUS` were separate AI choices. The
project no longer preserves old Classic bug-for-bug AI behavior as an option.

### Removed Selection Surfaces

- `-nga` / custom-game AI selection is removed.
- Main-menu custom AI selection is removed.
- Config/PBX numeric controls such as `custom_game_ai_id` are removed.

Internal save/runtime fields such as `ai_id` may still exist as compatibility
plumbing, but they must not expose a user-selectable Classic mode. Any old-save
decode behavior should normalize to the single canonical AI or be documented as
save-format compatibility only.

### Canonical AI Behavior

The following Classic+ fixes are now hard-wired behavior, not optional bugfixes:

- Fleet dispatch preserves unsent orbit ships when range or reserve-fuel checks
  prevent dispatch.
- Colony-ship dispatch refreshes local source counts after sends.
- Colony and transport targeting use Classic+ range, unrest, and source-distance
  gates.
- AI ship design uses Classic+ engine-pick and race-weight behavior.
- Battle AI uses Classic+ valuation/fix behavior for hit chance, absorb divisor,
  damage floor, missile/beam accumulation, base handling, and related AI scoring.
- Diplomacy and audience logic uses Classic+ treaty/peace mood selection, war
  parameter ordering, spy-hide attribution, tribute relation updates, relation
  clamp fixes, and audience threat/tribute handling.

### 1oom Files Affected By First Pass

- `src/game/game_ai_classic.c`
- `src/game/game_ai.c`
- `src/game/game_ai.h`
- `src/game/game_ai_classic.h`
- `src/game/game_diplo.c`
- `src/game/game_audience.c`
- `src/game/game.c`
- `src/game/game_new.c`
- `src/ui/classic/uimainmenu.c`
- `src/ui/cmdline/uimainmenu.c`

### Verification Evidence

First Classic+ canonicalization pass:

- `make -C 1oom/build-ai-gate -j4` passes.
- `1oom_cmdline -nga 1` reports unknown option.
- `1oom_cmdline -dumpnum` has no AI selector entries such as
  `custom_game_ai_id`.

Godot parity evidence against this modified 1oom oracle:

- `tools/parallel_run/out-classicplus-seed42-first-pass`
  - Trace drift: `0`
  - State drift: `8` rows at T100, localized to Mrrshan planet 8 economy and
    derived empire totals.
- `tools/parallel_run/out-classicplus-seed{1,7,42,99,12345}-matrix`
  - All five strict T100 matrix runs currently fail.
  - Earliest actionable matrix frontier: seed `1`, T27 `post_turn_p1`
    (`2805369837` vs `1830279879`).
  - Seed `42` remains trace-clean and fails only on the known T100 state-only
    Mrrshan planet 8 economy rows.

## Project Rule

Do not reintroduce old Classic AI behavior behind a hidden option, config key,
or command-line switch. If a future change needs another AI behavior, it must be
documented as a new explicit project decision rather than restoring the old
Classic/Classic+ selector.
