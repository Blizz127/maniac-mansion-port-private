# Optional input enhancements: adapter and verification

All options default false. With all options false, `qol_input_step` returns every
input byte unchanged and does not inspect or mutate the NES. ROM identity gating,
menu/chord adjudication, trace playback exclusion and one call per emulated frame
belong to the host integrator. The adapter targets the pinned USA ROM only.

## Proposed mapping

Tap A keeps native activation. Holding A for 250 ms permits continued Walk-to
updates only when the current verb is native Go To, the cursor is over an empty native scene hit-test point,
input is enabled, and no two-object sentence is pending. A destination change
produces a walk intent; the callback translates this into a native A latch at most
once per 100 ms without repeatedly selecting another verb; stationary held A produces none. The native consumer clears the action latch between generated updates. Release cancels
future updates; an already issued native walk remains governed by the game.
B, Select, Start, menus, cutscenes, actor/room/load/focus changes cancel the hold
and require neutral input before resuming. This does not alter actor speed,
script scheduling, NPCs, music, chase rules or timed puzzles.

Current New kid (verb 7) preserves native scene actor selection and bypasses
object snapping and the B free-cursor modifier.

With quick verbs enabled, a Select press is withheld for 250 ms. A short release
produces one native Select pulse (native kid switching); a longer hold opens a
host chooser. D-pad/arrow edges move selection, A confirms, B cancels. Select
release alone does not confirm. Start combinations cancel the pending tap;
existing developer-menu chord arbitration must run first. While a native
sentence is pending, Select is passed through and quick selection is unavailable.
Keep the native verb panel usable: chooser is an additional route, not a removal
of verbs. Include Walk to, Push, Pull, Give, Open, Close, Pick up, Use, Read,
Turn on, Turn off and New kid. Controller and keyboard use the same combined input path.

Object snapping uses directional half-planes, weighted squared distance (four
times lateral distance squared) and lower object ID to break exact ties. Selection
should survive provider list reordering. Native free cursor remains available
when snapping is disabled and while B plus a direction is held. This combination consumes B as a modifier;
a lone B retains its native action. Highlight only the
currently selected visible hit-test point and clear it on room/actor changes.

## Native ABI evidence and limits

Original local disassembly shows bank-0 `$913F` computes cursor motion, pointer
coordinates, UI hover and then consumes edge-latched A. `$948F` owns cursor
acceleration/fractions; `$7116/$7118` are pointer pixels. `$944B` uses the live
UI rectangles and IDs. `qol_native_verb_cursor` selects a current verb-panel
rectangle; host emits a native A edge rather than assigning a verb or bypassing
sentence scripts. Native IDs were independently observed in a real saved field
state: Go To 13, Push 9, Pull 10, Give 3, Open 1, Close 2, Get 14, Use 11,
Read 12, Turn On 4, Turn Off 5. These IDs are route metadata, not copied art/scripts.

Fixed-bank `$E1E6` implements native scene hit-testing. The adapter mirrors its
hidden/noninteractive flag, all parent state dependencies, byte-width bounds
and first-object overlap priority. It searches only screen points reachable by
the native cursor in the scene. Hidden keys dependent on a mat state are not
inferred from ownership alone. No inventory or offscreen object is listed.
Malformed counts, parent cycles and invalid points fail closed. This is native
interaction eligibility; it does not infer rendered sprite occlusion beyond the
native hit-test. Before enabling, differential tests must compare points and
hidden-object transitions against cartridge execution.

`qol_native_cursor` changes only verified cursor position, fractions and momentum.
It does not change actor state, sentence state, scripts or object flags. The production callback runs
at the native bank-0 `$913F` entry before cursor calculations. **PPU frame-ready stop is not a mainline/idle boundary**;
the production path therefore avoids plain frame-end injection. The caller must
verify ROM identity before invoking any `qol_native_*` function. General gates
include native input flags, cutscene slots, bank/NMI guards, cursor-disable bytes
and cursor service permission. These gates are conservative, not a complete proof
of chase/menu safety.

## Integration and acceptance

Compile `port/src/host/qol_input.c` into host and policy-test targets. Persist
three false-default options independently. Never call the adapter when all are
off or while replaying input traces. Apply existing pad-selection and developer
chord logic first, then combine keyboard/controller input once. The bound callback builds context
from guarded native state. Native input callbacks must have an explicit release
frame for A; require native `$710E` consumption before pointer restoration after
a verb click. Do not hold generated A or generate verb activation on every held
frame. Restore the scene pointer after a quick verb selection without changing
the native Use/Give first/second object flow. The UI must truthfully distinguish
pending/experimental wiring until actual ROM tests pass.

`test_qol_input.c` covers all 256 OFF input values, A threshold/release/stationary
behavior, menu neutral drain, other-verb suppression, Select tap/hold/Start
cancellation, sentence suppression and stable visible-only spatial filtering.
The test uses synthetic contexts and cannot establish gameplay equivalence.
Run `test_qol_native.c` with owner-local ROM and fixtures for native adapter
execution evidence; keep all artifacts local. Full acceptance still needs
held-A release during live walk/chase, menu/cutscene transitions, native verb
and Use/Give outcomes, object visibility transitions, controller/keyboard parity
and retail OFF state identity. No mock test alone makes these feature complete.

Implemented runtime API: initialize `qol_native_t` once; call
`qol_native_update(nes,state,options,input,permitted)` after host chord/menu
adjudication. `permitted` must include known USA identity and exclude traces,
menus, focus drain and load transitions. The callback binds only when enabled.
Read `state.policy.chooser`, `verb_index`, `state.highlight` and `selected` for
the host overlay. The native callback handles pointer restoration after a verb
click and never writes the current verb or object sentence.

The normal native NMI sets `$72D8=$FF` before the bank-0 service. The readiness
gate permits this only at the verified bank-0 `$913F` entry; general external
frame calls still require that guard clear. This is a specific normal NMI phase
allowance, not a general mid-NMI safety claim. Use/Give pending detection requires the matching current verb, a first object,
an active native preposition `$611D`, and no second object. This was checked
against completed one-object Use (flashlight), pending Give, and pending two-object
Use (key); the native sentence renderer uses this preposition between operands.

Actual owner-local verification by the validation lane: four native field
fixtures, 7,168 native hit-test differential points with zero mismatches, all
visible native IDs represented by snap targets, and 48 native verb-click script
results passed. A live Select chooser selected Push and restored the scene
pointer; a live held-A plus direction probe issued native walk updates as the
cursor moved. A natural Pull/Get/Use key route reached the same mat, key owner
and open-door outcomes as plain native UI, with serialized native machine state
equal after each step. These are cartridge/core execution checks, not physical
controller or chase acceptance. Full final runtime suite results are reported
by the integrator; no fixture, ROM, screenshot or derived listing is committed.
