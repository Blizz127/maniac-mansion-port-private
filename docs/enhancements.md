# Optional NES enhancements

Task 3b6b23789a0f, isolated from the published mm-r5 baseline (`edf82498`).
The interpreted retail 6502 runtime, ROM and battery-save format remain the reference.
Every new enhancement defaults OFF. Useful gated owner-only private builds are
authorized by the current owner cadence; public publication remains restricted.
The owner addendum adds a per-boot Original/Enhanced choice; Original ignores
all enhancement settings and uses the unchanged complete retail-ROM execution
path. See [enhancement-modes.md](enhancement-modes.md) for boot/save isolation
and the research contract for safe mid-game teammate replacement.

## Input design

A tap remains the native action. A hold threshold is 250 ms; subsequent
walk updates are restricted to native Walk-to, a changed cursor destination,
and enabled field input. Releasing A stops additional walk requests. It does not
teleport the actor or cancel a walk the native game already accepted. Menus,
cutscenes and actor/room changes require neutral input before resuming. Holding
an interaction verb must not repeatedly activate its script.

The retail NES Select shortcut already cycles Get/Use/Open; the Enhanced
chooser extends that existing shortcut. A short Select tap forwards one native
pulse. Character switching remains available through native New Kid. Held Select opens a quick
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
Plain d-pad/arrow movement now always keeps the retail free cursor with snapping
ON. B + direction snaps to a visible target; lone B forwards its retail action on
release. The separate OFF-default A + direction walking option keeps the cursor
still and requests short walk legs through the existing input interface. It
preserves retail speed/path logic, has a pause between legs, finishes the current
leg after release, and can delay a plain A press by up to 100 ms. New policy and
request code are native C; retail scripts, walking, rendering and audio remain
interpreted. See [enhancement-snapwalk.md](enhancement-snapwalk.md). The September
30 integrated evidence below describes the earlier candidate and its old snap
mapping; it does not establish acceptance of the October 1 controller correction.

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

New menu choices are under Options > Enhancements in Enhanced mode. The boot
choice defaults to Original every launch. Teammate replacement is also OFF by
default; its constraints and native/save-history evidence are documented in
[enhancement-party.md](enhancement-party.md). Changes are persisted with a
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
before/after checks. The integrated results and outstanding limits below distinguish actual ROM
execution from helper-only checks.

## Integrated evidence (2026-09-30)

The integration branch is `codex/enh-integrate`, based on published `edf82498`.
Input lane `9e95ce8` is integrated as `0aaeed8`; the combined walking/snapping
fix `aeda2d8` is integrated as `df57b05`. Presentation lane commits are
integrated as `e9aca0e`, `eaff03a`, and `19c3f4a`. No source or package was pushed.

- Six CTest checks pass: pads, config persistence, enhancement-menu/overlay,
  presentation, input policy, and timed chord. Policy checks are explicitly
  separate from the actual ROM checks below.
- Boot 1,200 frames and smoke 900 frames match Mesen exactly. The longer native
  newgame trace also matches 8,913 RAM-at-NMI snapshots, 297 PPU snapshots, and
  80,379 APU writes over 8,936 frames. Core source and snapshot/save layout are
  unchanged from the baseline.
- The host script checks 900-frame identity with enhancements OFF, presentation
  ON, and menu toggles used; the initial five choices persist and reload; the party option is included in
  the addendum checks. Trace playback
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
  including pursuit/capture. In a naturally fleeing chase with walking and snapping both enabled, held A
  retargets once; releasing A causes zero additional updates over the next 35
  frames. Native fixture checks also verify snapping resumes after release and
  a new direction press.
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


## Owner addendum: integrated mode and team changes

Original is the per-boot default and executes the complete unchanged retail ROM
through the NES core; it never binds enhancement observers or transactions.
Enhanced has separate battery/quicksave paths and a distinct snapshot identity.
All six optional choices, including teammate replacement, default OFF and persist
only as settings for Enhanced. Original saves retain their existing paths and
raw formats; Enhanced quick states carry checksummed native state and history.

Mid-game replacement is intentionally restricted to an idle starting road with
Dave selected, Dave remaining in the first roster slot, and eligible teammates.
Inventory stays actor-owned, starter items are granted once through native
handlers, and captive/dead/transformed/busy actors cannot be swapped. The host
proof replaces Syd with Bernard and rejoins Syd after a restart/Enhanced quick
load without item duplication. Actual native Save/Load invalidation and a
controlled original death-script test passed. Native Continue/load trust,
late character-specific puzzle routes and full endings remain limitations;
see [enhancement-party.md](enhancement-party.md) for exact evidence and commands.

## Beta-content research addendum

The owner selected all five documented restoration groups for Enhanced only.
Available authentic material and implementation status are tracked in [enhancement-beta-content.md](enhancement-beta-content.md).
Missing assets must be reported rather than invented; unknown additions/new
missions require a separate owner selection.

The verified implemented subset is one earlier Ed wording and both SCUMM credit
references, under the Enhanced Content Restoration page. Both default OFF and
apply next launch; text profiles have separate quick-state slots. See
[enhancement-restoration-text.md](enhancement-restoration-text.md) and
[enhancement-restoration-assets.md](enhancement-restoration-assets.md) for the
native-resource evidence and remaining selected content. This does not complete
the five-group restoration request.
