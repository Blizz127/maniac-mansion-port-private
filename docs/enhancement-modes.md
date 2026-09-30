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

No party-state mutation is implemented by this design contract alone. Unsupported
or unverified states must be refused with an understandable reason rather than
presented as a working feature.
