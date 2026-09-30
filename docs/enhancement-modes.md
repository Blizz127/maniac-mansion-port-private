# Original ROM and Enhanced / Remastered modes

Owner addendum to task 3b6b23789a0f. This document records the implementation
contract; verification results are added only after the corresponding checks.

## Boot and execution

Every interactive launch offers ORIGINAL ROM and ENHANCED / REMASTERED, with
Original selected initially. Choosing Enhanced is explicit for that launch;
optional enhancement settings still default OFF. A diagnostic command-line mode
selection may bypass the chooser. Automated/headless runs default to Original.

Original means execution of the complete owner-local USA retail ROM through the
existing NES hardware core, not a partial reimplementation of the game. The
cartridge loader verifies the complete 256 KiB PRG and maps its bytes through
MMC1. `nes_run_frame` steps the 6502 CPU; original scripts, actors, inventory,
puzzles and audio execute from that cartridge. This does not imply that every
hardware detail or every game route has been independently verified. The
existing Mesen comparisons establish specific trace equivalence.

Original must not enable enhancement callbacks, selection overlays, color
changes, enhanced input/pacing, or teammate replacement even when those options
were enabled previously in Enhanced. Its enhancement menu entry is absent.
The owner-local ROM is read and never patched or rewritten in either mode.

## Save isolation

Original retains the existing battery-save and quick-state locations and formats.
Enhanced uses a separate namespace; it neither imports nor overwrites Original
saves automatically. Enhanced quick states use a distinct snapshot identity so
an Original quick load rejects an enhanced state even if someone copies it into
the Original slot. The core serialized-state layout remains unchanged.

This separation is not an expanded native-save feature. Named save slots,
autosaving and migration of existing Original progress remain separate proposals.

## Mid-game party research contract

Native New Kid already switches control among the initial party. Replacing an
initial teammate is a separate operation. Native name tables and live fixtures
identify fixed actors: Syd 1, Razor 2, Dave 3, Michael 4, Bernard 5, Wendy 6,
Jeff 7. Inventory owners use those identities. Roster variables, actor
initialization and running scripts must be verified independently; merely
overwriting a costume is unsafe.

Before enabling replacement, inspect native selection initialization, party
identity, actor resources, room/position/path state, item ownership, running
actor/script references, character-specific puzzle progress, capture/death and
Dave's role. Define allowed entry states and rejection reasons from that
evidence. A replacement must not resurrect a dead kid, release a captive,
reset puzzles, invent items, transfer story abilities by accident or remove
Dave. Menus, pending commands, dialogue/cutscenes, chase/capture and running
character-specific actions require explicit treatment and actual ROM tests.

The constrained implementation is described in [enhancement-party.md](enhancement-party.md).
It uses original native handlers, with history established by observing new-game
construction or restored from a checksummed enhanced quick state. Unsupported
or unverified states are refused with an understandable reason.

## Verified boot-mode behavior

Boot lane `11e550f` is integrated as `bbb7408`. Seven CTest checks pass,
including default/neutral/edge chooser behavior and actual snapshot-loader
cross-mode rejection. The production host compares Original, Original with stale
all-ON enhanced settings, and Enhanced OFF: all 900 native framebuffer hashes and
captured rendered-window pixels match. Startup logs confirm effective Original
settings are all OFF.

The isolated Xvfb diagnostic feeds actual XTest keyboard events and a virtual
Xbox through the production boot chooser. Both select Enhanced after neutral,
and confirmation is drained before core execution. Existing 100-frame native
keyboard/controller parity remains exact. Physical handheld operation is not
claimed by this device-pipeline test.

A disposable Original battery sentinel remains byte-identical during Enhanced
execution. Enhanced writes its own quick state; copying that state into the
Original slot is rejected, while Enhanced loads its own state successfully.
Enhanced quick states now wrap the unchanged core snapshot with checksummed party
history. Battery sidecars bind that history to all 8,192 saved PRGRAM bytes.
Legacy enhanced states can load, but lack trusted replacement history.
The owner-local ROM SHA-256 remains unchanged.

An independent read-only instruction observer over 180 live native frames
records 1,900,786 original 6502 instructions, including 1,898,146 from cartridge
ROM and 2,640 from native RAM code. The complete 262,144-byte PRG is byte-identical
before/after. This establishes the original retail engine path, alongside the
previous Mesen trace comparisons; it is not a full-playthrough assertion.

Reproduce host mode/isolation checks with `port/tests/test_modes.sh build/port
OWNER_LOCAL_ROM build/local-mode-proof`. `MM_MODE_TEST_SAVE_ONLY=1` runs only the
save checks after the frame/window comparisons have already passed. The script
uses isolated Xvfb and disposable config/data roots. Screenshots and states stay
under ignored build output. Native instruction evidence remains in owner-local
scratch, not packaged game data.


## Integrated addendum status

The pre-restoration host had nine ROM-free CTest checks. All six original optional choices persist
and reload in Enhanced; Original ignores stale ON values for all six. Enhanced
teammate replacement uses the constrained native roadside implementation above,
with real production-menu replacement and cross-launch quick-state rejoin checks.
Native Save/Load and controlled native death regression evidence is documented in
[enhancement-party.md](enhancement-party.md). No push, release or deployment has
been performed. Physical handheld operation, physical monitor cadence and a full
playthrough remain outside the verified evidence.

## Selected restoration profiles

The owner subsequently selected all five content groups for Enhanced mode.
The implemented subset is one earlier Ed wording substitution and both SCUMM
references. Both settings default OFF, persist independently, and apply only
at the next launch. Original ignores both even with stale ON config. It never
allocates or maps a restoration copy. The rest of the selected content remains
incomplete for the reasons in [enhancement-beta-content.md](enhancement-beta-content.md).

A verified disposable PRG copy supplies only those three fixed-length text
splices in Enhanced. The owner's cartridge storage is read-only; the native
engine code, resource sizes and relative jumps retain the retail bytes.
Restoration does not replace the emulator with a partial game port. Enhanced
OFF and Original continue mapping the original verified PRG.

Text profiles 1/2/3 use separate `quicksave/enhanced/restoration-N/slot1.mmst`
slots and identities derived from their actual PRG bytes. Profile 0 retains the
existing Enhanced slot. This preserves old states and rejects a state copied
from a different text profile. Original quick-state paths and identity are
unchanged. Fixed-length text leaves native battery/save layout unchanged;
Enhanced battery state remains in its previously isolated namespace.

Both production and diagnostic builds now pass ten CTest checks. The actual
host verifies next-launch activation, menu/config persistence, separate profile
slots and successful own-profile loads, rejected copied wrong-profile states,
and unchanged owner ROM. With stale ON config for all eight settings, Original
still matches Original OFF and Enhanced OFF for 900 native hashes and captured
window pixels. Original battery isolation checks also pass. These are targeted
checks, not proof of a full restored game or every ending.
