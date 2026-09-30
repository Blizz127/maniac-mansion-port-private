# Partial native text restoration

The owner selected the five content-restoration groups for Enhanced mode. This
module implements only a narrow part of that request: Ed's single changed word
and the two SCUMM references in the credits. These are separate OFF-by-default
options, not a complete dialogue, decoration or beta-content restoration.

Douglas Crockford's [first-person conversion account](https://www.crockford.com/maniac.html)
identifies the changed Ed wording and removal of both SCUMM references. The
module restores those source-supported words while retaining the retail credit
layout, attribution and spacing. No authenticated prototype ROM was available;
this is not a claim of byte-identical beta credit layout. His additional
[contemporary content list](https://www.crockford.com/maniacthings.html) supplies
previous Edna dialogue, but that material is not implemented by this module.

## Immutable source and exact profile

`restoration_apply` is boot-only and accepts a disposable host-owned PRG copy.
The caller must never pass the cartridge's original storage. Original mode
ignores every option. Enhanced with both options OFF also performs no write.
For an enabled option the module requires the exact pinned 256 KiB retail PRG
SHA-256, entire affected resource hashes and exact short encoded targets. It
validates all selected patches before applying any. A different dump, modified
input or unsupported size is rejected without writing anything. No ROM file is
written, downloaded or bundled.

| Resource | Location in headerless PRG | Encoded bytes replaced |
| --- | --- | --- |
| Global script 140, Ed | `0x26ef7` | 5 |
| Global script 63, first credit reference | `0x233f0` | 5 |
| Global script 63, NES credit reference | `0x23487` | 7 |

The native encoding uses the high bit of a word's last character to insert a
space. Both encoded byte lengths and expanded character counts are retained.
The second credit reference uses existing-width padding rather than moving the
retail byline. Opcode bytes, string terminators, control characters, script
headers, relative jumps, print boundaries and delay instructions stay intact.
There is no native hook, script relocation or simulation-speed change.

The host selects the options at boot, keeps the original cartridge unchanged,
and frees the memory copy on exit. `restoration_identity` derives an eight-byte
identity from the actual PRG profile so an Enhanced snapshot cannot silently
resume under different text settings. Original save identity stays unchanged.

## Remaining selected content

Fred's longer original phrase, Edna's capture and phone speeches, the shower
clue, anatomy caption and decorative artwork are not enabled here. Their
previous wording has varying primary-source support, but a label or quotation
is not an intact NES resource. Longer encoded strings cannot be dropped into
shorter retail storage: moving script text requires audited relative jumps,
resource pointers, print/control boundaries and save compatibility. A display
replacement must also prove native message wait/delay and phone-event behavior;
merely overlaying a line would not establish that result. Phone clue digits and
all puzzle scripts must remain intact. No invented dialogue, alternate puzzle
chain, custom room, ability rebalance or unrelated modern hack is imported.

## Verification

`test_restoration` checks Original/all-OFF immutability, atomic rejection of an
unknown identity and wrong size, and stable/distinct profile identities. The
owner-local native test is a separate execution check: it must verify actual
native print source bytes, script events and timing for the changed resources.
Controlled script execution is not a natural full-ending playthrough. No game
scripts, extracted assets or long copyrighted dialogue are committed.

## Longer-message execution boundaries investigated

The native print opcode enters at `$CDB3`; it saves the operand pointer in
`$B9/$BA`, expands encoded text into `$6F0B…` through `$F522…$F5A7`, and
restores the next bytecode fetch pointer at `$CDA8…$CDAF`. A hypothetical
expanded-buffer replacement at `$F5B2` would leave the bytecode offsets intact,
but would not establish identical native message duration. Native
`waitForMessage` at `$C807` polls script variable 3 (`$6103`); display completion
is part of script execution rather than an independent host subtitle timer.
No such replacement hook is implemented.

Retail capture script 37 selects three speeches using the native random-number
operation, then waits for the message before clearing the captured actor's
freeze bit. The primary description instead distinguishes earlier speeches by
kid gender. Blind substitution into the random branches would misrepresent that
behavior. A future restoration must verify both selection and shared capture
state, including RNG progression, placement, freeze clearing and follow-on
scripts.

Retail phone script 35 has separate first/subsequent-call branches, a native
message wait, actor movement and a later 1200-tick delay before restoring its
phone state and choosing follow-on scripts. Replacing its message with a shorter
line could alter the available diversion window. Future validation must compare
those native events and puzzle outcomes, not merely confirm that a replacement
caption appears.
