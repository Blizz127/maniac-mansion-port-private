# Enhanced roadside team changes

This separate, default-OFF enhancement changes the two teammates during a running
Enhanced game. Dave stays actor 3 in the native first roster slot. Original mode
has no observer or transaction. This is a constrained roadside feature: select
Dave, return Dave and the outgoing teammate to the road, stop moving, finish or
cancel a sentence, and release the game buttons before requesting a change.

The fixed native identities are Syd 1, Razor 2, Dave 3, Michael 4, Bernard 5,
Wendy 6 and Jeff 7. A change updates the native teammate roster through the
original variable setter. It never changes an actor's identity, transfers their
inventory or replaces their character scripts. An outgoing kid is parked
outside the active scene with the original room handler; their inventory remains
owned by that kid. Rejoining places them at the outgoing kid's road position.
Their previous off-road position is not preserved.

A first activation uses the original room, position, inventory pickup and item
state handlers for that kid's original starter item. It requires the untouched
native starter state, no existing inventory copy, and an empty inventory slot.
An immutable issued mask prevents repeating this grant on later rejoins. The
whole introduction script is never replayed. Native New Kid selection continues
to use the original roster, camera, music and sentence scripts.

## Safety and provenance

Team changes require trusted history established by observing the actual native
new-game initialization and starter-item construction, or restored from a trusted
Enhanced snapshot. Merely loading a field state or seeing an actor in room zero
cannot establish that an actor has never been selected. History uses eight
versioned bytes: version, validity, activated mask, retired mask, starter-issued
mask, and the three original roster identities. Invalid or missing metadata
clears trust. The integrator isolates Enhanced save files and binds battery
metadata to the full native battery image. Native in-game Save preserves trusted history. A native in-game Load invalidates
history; a cold Continue without newly trusted metadata cannot change teammates.

The module rejects Dave replacement, invalid or reordered rosters, actors with
changed costumes, dead/frozen actors, actors outside the permitted rooms, current
sentences, movement, cutscenes, Meteor control, and scripts beyond the verified
idle-road whitelist. Captive actors cannot be retired from the roadside. No
resurrection, release from captivity, inventory recovery, puzzle reset or
character ability rebalance is performed. Costume checks deliberately reject
story transformations even when the actor might otherwise be usable.

## Native execution and integration

`party_update` changes host permission/input state only. An Enhanced-only host
instruction dispatcher calls `party_exec` alongside the input adapter, including
when the team-change option is OFF, so new-game provenance can be observed.
Original mode must not bind this observer. `party_request` queues a reviewed
change; `pending || executing` disables saves/loads until stable.

Transactions begin at the verified fixed-bank main-loop boundary. Each native
handler runs through an asynchronous PC/stack call under ordinary
`nes_run_frame`; there is no nested CPU run loop, timer multiplier or fabricated
frame. NMIs, script delays, music and PPU/APU clocks continue normally. Only
verified caller operand scratch and CPU call context are restored; native pad,
mapper, timer and audio state are retained. The actual entry mapper bank is
restored using the original handler. Postconditions are checked before history
is committed. `party_cancel(nes,state)` restores the pretransaction machine if
an unfinished operation must be abandoned; call it before exit/state replacement
and before flushing a stable save, then `party_destroy`.

## Verification boundaries

Synthetic tests cover metadata round trips, malformed provenance, and Original/
OFF immutability. The separate owner-local actual-ROM test covers native
replacement/rejoin cycles, original starter resource/state differentials,
inventory ownership and duplication, native guards and Original machine-state
identity. A separate original-pad-only RESET/boot run observed fresh history
without metadata injection while matching native machine states against an
unmodified instance on every frame. Native New Kid selection after replacement
selected the incoming actor with the original camera and Walk command. These
checks are not an ending or every character-specific puzzle playthrough.
Retail ROM-derived scripts, fixtures and detailed disassembly evidence remain
owner-local and are not included in these source files.

The native test executables require the owner's local ROM; no game data is
bundled. `test-party-native ROM ROAD_DAVE_SYD_WENDY_STATE
ORIGINAL_DAVE_RAZOR_SYD_STATE [CHASE_STATE CAPTURE_STATE]` runs replacement,
rejoin, native selection and unsafe-state checks. `test-party-fresh ROM` boots
from RESET, selects the original kids with ordinary controller input and checks
the read-only provenance observer against a second unmodified instance. Neither
native test substitutes generated fixtures for actual game execution.

`test_party_saveload ROM ROAD_DAVE_SYD_WENDY_STATE` uses the original native
menu to save, reopen and restore. It verifies that Save preserves history, Load
invalidates it, and a rejected post-load team request leaves native state intact.

## Integrated host evidence

The input implementation `130315b` is integrated as `9c3bd82`; its Save/Load
observer correction `ff97d58` is integrated as `f0ad4b7`. Actual Save/Load testing
caught the original misidentification: native script 163 offset `0x63` is Save,
and `0x77` is Load. The corrected original-controller menu test records both
operations, keeps history valid on Save, clears it on Load, and verifies that a
rejected replacement after Load leaves the complete native snapshot unchanged
(17 checks, zero failures).

The production host, in isolated Xvfb, loads an explicitly verified initial
Dave/Syd/Wendy fixture as a trusted Enhanced quick state, navigates the actual
Enhancements/Teammates menu, and replaces Syd with Bernard. Dave/Wendy and all
four item ownership records remain intact and unique. The resulting checksummed
quick state contains activated/issued mask `0x35`, retired mask `0x01`. A second
application launch loads that state and rejoins Syd while retiring Bernard;
all inventories remain unique, and retired mask becomes `0x10` without issuing
another starter. These host tests exercise real menu, dispatcher and persistence
code; the seeded fixture is distinguished from the separate genuine RESET
provenance test.

The native replacement suite passes 263 checks, including 90 Original and 90
Enhanced option-OFF full-state comparisons, three replacement/rejoin cycles,
native New Kid selection, capacity/reordered-Dave guards and cancellation.
The RESET/original-controller provenance test passes 1,502 checks with no
metadata injection and per-frame state equality against an unmodified instance.

A controlled call through the original variable setter and script starter runs
original death script 61: 1,040 native script opcodes over 86 frames produce
Wendy's zero costume, freeze/death flags, grave placement and destroyed starter
ownership. The module denies replacement without changing the snapshot (15
checks). This is actual native death-routine execution, **not** a naturally
triggered gameplay death. Normal death-puzzle routes, late character-specific
puzzles and endings remain unverified.

Enhanced quick states preserve trusted history; older raw Enhanced snapshots
can load with history unavailable. Native in-game Load/Continue currently clears
replacement trust even if battery metadata was present. Normal gameplay remains
available, but further teammate replacement requires a trusted Enhanced quick
state or a new Enhanced game. Do not infer compatibility with every native save
or character-specific puzzle route from these tests.

Optional diagnostics: `test-party-saveload ROM ROAD_DAVE_SYD_WENDY_STATE` and
`test-party-death ROM ROAD_DAVE_SYD_WENDY_STATE`. The latter explicitly labels its
controlled native script call. ROM/state fixtures remain owner-local.
