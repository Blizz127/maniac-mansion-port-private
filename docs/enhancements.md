# Optional NES enhancements

Task 3b6b23789a0f, isolated from the published mm-r5 baseline (`edf82498`).
The native 6502 runtime, retail ROM and battery-save format remain the reference.
Every new enhancement defaults OFF. No release or remote publication is authorized.

## Input design

A tap remains the native action. Proposed A hold threshold is 250 ms; subsequent
walk updates are restricted to native Walk-to, a changed cursor destination,
and enabled field input. Releasing A stops additional walk requests. It does not
teleport the actor or cancel a walk the native game already accepted. Menus,
cutscenes and actor/room changes require neutral input before resuming. Holding
an interaction verb must not repeatedly activate its script.

Select tap must retain character switching. Proposed held Select opens a quick
verb chooser, with D-pad/arrow navigation, A/X confirmation and B/Z cancellation.
Start+Select dev-menu processing takes priority. On a controller, its existing
300 ms chord grace precedes the 250 ms quick-verb hold recognition; the keyboard
Select equivalent uses the hold threshold directly, with F8 opening the dev menu. Selecting a verb changes the
native sentence-selection path; it must not execute an object script directly.
Use/Give retain the native first- and second-object state machine and inventory.
All available native verbs remain accessible, including ones outside the requested
Push/Pull/Give/Open/Close/Get/Use/Read set.

Object snapping must select a currently visible, native-hit-testable on-screen
point. Native parent-object state controls visibility, including the doormat/key
case. An object ID, owner nibble or resource-cache entry alone is insufficient.
A stable nearest directional target needs a deterministic tie break and a free
cursor escape. There are no hidden-object outlines, puzzle hints or new actions.
Holding B while moving a direction temporarily restores free cursor movement;
the B modifier is consumed so it does not cancel a two-object sentence. Lone B
retains the native behavior. See enhancement-input.md for the native adapter.

## Presentation

Deadline display pacing changes only host waiting between native frame deadlines.
The existing NTSC accumulator schedules `nes_run_frame`; no script timer, music,
NPC, puzzle or actor speed is multiplied. It does not interpolate motion or claim
that the NES gameplay has a higher simulation frame rate. Recording and the
existing explicit fast-forward retain their original timing paths.

Inspection found the legacy chord helper counted host polling loops as frames.
Enhancements change the host waiting cadence, so the enhanced path uses a real
300 ms Back/Start grace window and a minimum 34 ms native tap replay. The legacy
path is retained exactly when every enhancement is OFF. Timer-based chord tests
cover multiple polling cadences; no physical handheld gesture acceptance is claimed.

The graphics lane is a reversible scene-color sample, not a replacement asset
set. It adjusts palette-derived scene pixels at their existing coordinates;
the bottom verb/inventory UI remains untouched. There is no resampling of object
geometry, change to hitboxes, change to occlusion, temporal blending or discovery
of hidden objects. Owner-local screenshots provide before/after proof; game pixels
and assets must never be committed. See enhancement-display.md for limits.

A faster player-walking option is separate future work: it requires isolated
actor-step analysis and chase/pathfinding tests. Global simulation speed, script
frequency or refresh changes cannot implement it safely.

## Settings

New menu choices are under Options > Enhancements. Changes are persisted with a
same-directory atomic config-file replacement that preserves unrelated settings,
ROM paths, input mappings and comments. Existing command-line and environment
configuration precedence still applies at next launch. A menu save failure is
shown to the player, and the current choice remains usable for the current run.

## Research context and additional proposals

These individual reviews provide qualitative context, not a universal consensus:

- [GameFAQs review 160275](https://gamefaqs.gamespot.com/nes/563438-maniac-mansion/reviews/160275)
  describes slow D-pad cursor travel and reselection after a wrong action.
- [GameFAQs review 169592](https://gamefaqs.gamespot.com/nes/563438-maniac-mansion/reviews/169592)
  discusses control difficulties, chase pressure and consequences of mistakes.
- [RPGFan review](https://www.rpgfan.com/review/maniac-mansion/)
  discusses presentation and movement separately from cursor usability.

Independent cursor speed/acceleration, direct mouse support and a larger readable
verb UI are proposals only. Multiple named native saves or an autosave before
risky actions need feasibility and owner review first: preserve battery-save
format, identify a true native save boundary, define rollback/retention, and test
with disposable saves. The existing quick-save feature is not proof that native
save expansion is safe. No autosave, hints, softlock prevention, story rebalance,
character rebalance or uncensoring is introduced by this task.

## Verification status

Each lane must distinguish policy/unit checks from actual cartridge execution.
Final acceptance needs all-OFF retail state identity, real held-A/menu/release
and chase checks, visible-only selection, native verb/two-object equivalence,
keyboard/controller parity, display-rate timer/audio/puzzle stability, and sample
before/after checks. Results and outstanding limits will be recorded here after
integration; helper-only checks do not establish feature completion.

## Integrated evidence (2026-09-30)

The integration branch is `codex/enh-integrate`, based on published `edf82498`.
Input lane `9e95ce8` is integrated as `0aaeed8`; presentation lane commits are
integrated as `e9aca0e`, `eaff03a`, and `19c3f4a`. No source or package was pushed.

- Six CTest checks pass: pads, config persistence, enhancement-menu/overlay,
  presentation, input policy, and timed chord. Policy checks are explicitly
  separate from the actual ROM checks below.
- Boot 1,200 frames and smoke 900 frames match Mesen exactly. The longer native
  newgame trace also matches 8,913 RAM-at-NMI snapshots, 297 PPU snapshots, and
  80,379 APU writes over 8,936 frames. Core source and snapshot/save layout are
  unchanged from the baseline.
- The host script checks 900-frame identity with enhancements OFF, presentation
  ON, and menu toggles used; all five choices persist and reload. Trace playback
  intentionally bypasses live input enhancements. The native fixture tests
  separately establish OFF full-machine-state identity with live input.
- Actual USA ROM routines match the visible-hit-test adapter at 7,168 grid points,
  with no missed visible IDs in those fixtures. All 12 native commands are
  selected by 48 native clicks. Tests cover native Select tap, held chooser,
  New Kid navigation, completed single-object Use, pending Give/Use key, and
  free cursor movement without dropping a pending Give sentence.
- Pull mat, Get key, Use key on door reaches mat `$8F`, key `$B3`, and open door
  `$8F`, with complete native-state equality against normal UI commands after
  each step. Hidden-key parent visibility follows the original hit-test rules.
- Native hold tests cover stationary equivalence, changed-target updates,
  release, menu cancellation, actual input-disabled cutscene state, and neutral
  rearming. A separate input-only continuation naturally enters Hall `$0A`,
  then Kitchen `$07`, triggers Edna's pursuit, and reaches capture in Dungeon
  `$04`. In an active chase, 90 stationary-hold native states equal baseline,
  including pursuit/capture. In a naturally fleeing chase, held A retargets once;
  releasing A causes zero additional updates over the next 35 frames.
- In isolated Xvfb, actual XTest keyboard events and a virtual Xbox controller
  feed the production keymap/pad polling functions. All 100 native-frame states
  match, including chooser/verb and free-cursor input. This is device-pipeline
  evidence, not a physical Steam Deck or Legion session.
- Simulated 60/120/144 Hz host schedules preserve all 8,936 newgame native-frame
  state hashes and audio PCM hashes; final serialized states are byte-identical
  (`bf54f55f12b20a09`, diagnostic FNV-1a). This covers the scripted opening and
  field continuation in that trace, not every puzzle or a full playthrough.
- Current-core color-sample proof changes 7,742 scene pixels, zero UI pixels,
  zero pixels with the option OFF, and no native machine-state bytes. Native
  hitboxes and occlusion are unchanged; screenshots were inspected locally.
- Timed chord tests preserve sequential/held gestures and lone taps under
  1/7/16 ms polling. Original all-OFF chord processing remains unchanged.

Reproduce diagnostic builds with `cmake -S port -B build/port
-DMM_BUILD_ROM_DIAGNOSTICS=ON`. Production builds default to excluding the
ROM-dependent diagnostic executables. Run `test-qol-native ROM FIELD_STATE...
--cutscene CUTSCENE_STATE`, `test-qol-chase ROM ACTIVE_CHASE_STATE
FLEEING_CHASE_STATE`, or `presentation-timing ROM port/traces/newgame.mmin 8936`
with owner-local compatible fixtures. `test-qol-host ROM FIELD_STATE` requires
an isolated `xvfb-run` server, `MM_TEST_XVFB=1`, SDL >= 2.24, and XTest; it must
never be run against the owner's desktop. Normal CTest requires no retail data.

Evidence logs and screenshots remain under the ignored integration
`build/enh-validation/` directory. Natural chase preparation is recorded under
the owner's scratch `mm-enh-validation/` directory. ROMs, states, game pixels,
assets and generated listings are excluded from commits and packages.

## Remaining limits

Physical handheld gestures, actual monitor presentation cadence, and a full
playthrough are unverified. Deadline waiting does not invent interpolated motion;
local waiting benchmarks show improved averages but sometimes worse maximum
lateness. The graphics change is only a reversible color sample. Faster actor
movement is not implemented: original actor pacing, scripts, NPCs and timers
remain intact. Save expansion and all additional proposals above remain proposals.
