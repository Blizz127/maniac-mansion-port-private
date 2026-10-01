# Snap combo and direct walk (task 0fa8930471a8)

Owner feedback on mm-r6: "the snap only snaps, it doesn't move the cursor
like originally, so maybe need a button combo to do snapping since the
characters need a better way of walking". Both options are Enhanced-only and
OFF by default. Original mode ignores them, the same way it ignores the other
enhancement settings.

## Controls

| Option (config key) | Menu label | Gesture |
| --- | --- | --- |
| `object_snap` | B + D-PAD: SNAP TO OBJECT | The d-pad always moves the native free cursor. Hold B and press a direction to jump to the nearest visible object that way. A lone B tap still does what B does in the original, delivered once on release. |
| `direct_walk` (new) | A + D-PAD: WALK KID | With Walk to selected and the pointer in the scene, hold A and a direction (diagonals work) to walk the current kid that way, one short leg at a time. The cursor stays where it is. Release to stop after the current leg. While this option is ON, a plain A press reaches the game up to 100 ms late (on release, or after 100 ms held), so A + direction never also clicks at the pointer. |

The B combo can't collide with the Select quick verbs or the Back+Start
developer chord. Select or Start pressed during a held B passes through and
cancels the B tap. The `object_snap` key and saved setting are kept, so an
owner who had snapping ON gets the new combo behaviour without changing
settings.

With Hold A walk also enabled, direct walk takes over A + d-pad: the cursor
doesn't move, so Hold A walk's cursor-following has nothing to follow.

## How it works (native vs interpreted)

All new logic is native C host code in `port/src/host/snapwalk.c`:

- `sw_snap_step`, `sw_walk_step`, `sw_walk_target`: pure host policy with no
  guest access. CTest `snapwalk` covers it.
- `sw_native_request`, `sw_native_exec`, `sw_native_actor`: a native C host
  adapter at PC boundaries. It reads guest state (selected kid `$6100`, active
  slot `$6D22`/`$6D1A`, active x/y `$6CBE`/`$6CC2`, walk flags `$6CCE`,
  camera `$6C31`, room `$63C6`). It writes only the native input interface
  bytes listed below.

New code never runs guest code, never sets PC or CPU registers, and has no
trampoline. A walk is an ordinary native click:

1. At the bank-0 `$913F` cursor service entry (the existing input hook point),
   set the A latch `$710E = $80`, as the retail `$FBAA` does for a press.
2. At the return of the bank-0 pointer-to-scene conversion (`$95B4`), replace
   the conversion result `$6CA5` (screen column) and `$6CA4` (scene row + 18)
   with the target. The drawn cursor `$7116`/`$7118` and the UI hover bytes
   are never written. Tests confirm the cursor sprite stays identical to the
   retail run.
3. The retail main loop (`$C056`) copies those bytes into SCUMM variables
   30/31 (`$611E`/`$611F`). The click starts script 4, which handles the
   sentence and calls `walkActorTo` (`$CF7E`); the engine then does walk-box
   clamping, path and walk speed. When the kid is busy, script 4 can read
   variables 30/31 well after the click. The periodic `findObject` hover read
   (`$D740`) isn't the consumer. So the override lasts until `walkActorTo`
   reaches its walk setup call (`$CF94`) for the kid that made the request,
   with the requested coordinates. It is also dropped if the room, selected
   kid or input control (`$6B0A` bit 4, cutscene slots `$6B08`/`$6B09`)
   changes, or after 60 frames.

The snap combo moves the cursor through the existing verified
`qol_native_cursor` (`$7116`/`$7118` plus momentum). B is withheld from `$02`
and the `$2F` edge group while held. A lone tap is delivered as one `$2F`
B edge, the same mechanism the verified Select tap uses. The native consumer
is `$F7B1` (`$6C2E` → script 4).

**Still interpreted:** the retail 6502 engine. That covers the SCUMM
interpreter, script 4, walking/pathing, rendering and audio. Direct walk and
snapping are native policy steering that interpreted engine through its
player-input interface. This is not native movement, and it is not a native
game. The native-engine milestone 50325f922db8 is where those paths become
native.

## Why the walk is made of short legs

Measured on the fixtures, a native walk request made while the kid is already
walking either restarts the retail walk start-up (the kid turns and pauses for
about 12 frames, flags `$81`/`$83`) or gets deferred by script 4. A deferred
request can later walk the kid to wherever the visible pointer is, and a walk
to the kid's own spot doesn't end a running walk. So direct walk:

- holds back the gesture's own A press, so no native click at the pointer
  happens;
- requests one leg of up to 4 columns / 8 rows ahead, clamped to visible scene
  columns 2..29 and rows 0..69, and only while the kid is idle (walk flags
  clear); it requests the next leg when that one ends, and at once on a
  direction change if the kid is idle;
- sends no stop request; after release the kid finishes its current leg.

The result is native-speed walking with a short retail start-up pause between
legs. Smooth continuous walking, or a stop exactly at release, would mean
writing the retail walk state directly (`$6CCE`, actor tables). That is
outside the native rules for this task, so it is reported as a conflict, not
implemented.

Targets that land on a native object hit-test point are shortened, so a floor
walk never becomes "walk to object". If no free point exists, nothing is sent.
Requests are never made over a pending A latch, while a sentence is pending,
during the quick-verb chooser, during a held B combo, or with any verb other
than Walk to.

## Evidence (owner-local ROM and fixtures, nothing committed)

`test-snapwalk-native ROM STATE [--png DIR]` (built with
`-DMM_BUILD_ROM_DIAGNOSTICS=ON`) checks:

- with snap ON, 900 frames of free d-pad and plain A taps give a full
  machine state identical to the unhooked retail run on every frame;
- with snap and direct walk ON, 900 frames of free d-pad stay identical too;
  a plain A tap reaches the same native walk outcome as retail;
- with every option OFF, 900 frames of arbitrary input stay identical and the
  hook is never bound;
- a lone B tap gives one native B event, the same as retail; B + each
  direction lands on a visible native target with zero native B events;
- A + Right/Left/Up/Down walk the kid with zero cursor or cursor-sprite motion
  and no native click at the pointer; every request is consumed by
  `walkActorTo` with no timeouts; the kid stops within one leg after release.
  Walk boxes block some directions at some fixtures, which is retail behaviour;
- with the pointer in the verb panel, A + Right gives a state identical to
  retail and makes no requests; no request is made while the verb isn't
  Walk to;
- a request pending when the host menu opens still reads its own target; a
  stale request is dropped when the kid, room or input control changes.

The existing `test-qol-native` was updated for the new snap semantics. Plain
d-pad with snap ON now has to match retail state exactly; snapping uses B +
direction.

## Limits

- Physical controller, keyboard and handheld acceptance are not claimed.
  Gestures are tested through the same combined host input byte.
- Chases, timed events and every room's walk boxes are not exhaustively
  tested. Fixtures cover the front-yard/road field states.
- After release the kid finishes its leg (up to 4 columns / 8 rows). There is
  a short retail start-up pause between legs.
- With direct walk ON, plain A presses arrive up to 100 ms late.
- Up and down are bounded by walk boxes; on a narrow path the native clamp
  may turn a vertical request into a diagonal one.
