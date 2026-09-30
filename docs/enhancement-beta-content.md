# NES beta-content research, pending owner selection

Task 3b6b23789a0f, owner addendum, researched 2026-09-30. Restoration is
**not authorized until the owner selects items**. The coordinator has already
asked; do not send a duplicate selection request. This inventory supplies that
decision. No restoration code, menu toggle, patch, asset import or puzzle change
has been implemented. Any selected work belongs to Enhanced mode only; Original
boot, owner ROM and Original saves retain their existing behavior.

## Documented pre-release NES edits

[Douglas Crockford's first-person account](https://www.crockford.com/maniac.html)
documents the following. It describes conversion/submission history, not a
byte-level comparison against a particular authenticated beta cartridge.

| Selection ID | Candidate | Account's evidence |
| --- | --- | --- |
| statue | Reclining hall statue | Removed after review. |
| calendar | Ted's swimsuit calendar | Removed. |
| mummy | Ted's posed, bandaged mummy poster | Removed. |
| disco | Green Tentacle's anti-disco poster | Voluntarily removed. |
| shower | Ted's bathroom phone-number joke | Shortened; Edna's number retained. |
| anatomy | Fred's medical-diagram joke | Caption replaced. |
| fred | Fred's brain-extraction dialogue | Wording softened. |
| ed | Ed's concern about Fred | Eating became sleeping. |
| edna-capture | Edna's suggestive capture speeches | Replaced with milder responses. |
| edna-phone | Edna's suggestive phone scene | Rewritten around Ted. |
| profanity | Earlier strong-language variants | Conversion edits preceded submission. |
| arcade | Arcade naming history | Two earlier names preceded Tuna Diver; versions need distinguishing. |
| credits | SCUMM / NES SCUMM references | Removed from ending credits. |

These are candidate **selections**, not a blanket uncensoring plan. Suggested
but rejected Edna lines in the account are unshipped proposals, not recoverable
beta content. Its description of hamster treatment explicitly says it survived
the review; do not treat it as a USA beta-to-release removal.

## What the available local material establishes

Only the owner's identified USA retail dump is available in the inspected project
ROM directories. No beta or PC dump was obtained. Its full-file SHA-256 remains
`e59f95a80497779b861daa26e1b890929fd2f9939e78003898d9bff3ea3f6db2`;
the 256 KiB PRG retains the pinned retail identity. Existing local script
disassemblies number 171. Decoded scripts, room data and images stay owner-local.

Read-only inspection confirms the released dialogue substitutions in script 148
(Fred) and script 140 (Ed), and generic story-system/NES-version credit wording
in script 63. This corroborates specific release-side changes; it does not
recover the exact previous encoded text, artwork, mask or object placement.

A case-insensitive literal ASCII scan of the complete retail PRG finds names
for statue, calendar, mummy, disco, telescope, skeleton and surplus body parts.
The scan also finds the current arcade title and a SCUMM occurrence. Those
occurrences alone establish **retained labels**, not accessible objects, intact
artwork, executable interactions or displayed credits. A zero literal count
does not prove absence: NES text has control codes and graphics are encoded.
In particular, do not promise restoration by merely unhiding a label or assume
the statue's graphics survived because its name did.

Local script 107 retains a recorded mating-call branch and reaction, but that
branch does not invoke a death script before returning camera/control. Turning
it into a killing event would change gameplay. No local beta comparison proves
that outcome existed in a specific NES prototype.

The owner-local evidence ledger is `build/enh-validation/beta-research.json`:
ROM identity, script hashes, eight read-only checks and literal-name counts.
It contains no extracted game assets or decoded script text. It is ignored
build output, not a distributable beta manifest.

## Other claims and exclusions

| Claim or variant | Classification for this project |
| --- | --- |
| Skeleton/dungeon redesign, surplus body parts, Ed's pennant | Further candidates, not authenticated NES beta removals. Some retail labels survive, but label presence cannot establish original NES appearance or behavior. |
| Killing response to the mating-call recording | Requires independent NES beta proof and explicit gameplay selection. Retail branch evidence above does not establish that response. |
| Out-of-order staircase/new room/new puzzle chain | No verified beta restoration here. Do not enable speculative routes or borrow a modern hack's added puzzles. |
| PC/C64 room layouts, extra commands, platform art; Japanese title/port | Platform differences require separate evidence. They are not automatically western NES beta content. |
| Modern mouse support, cosmetic fixes and bug fixes | New enhancements, not restoration provenance. |

The [Decoded project's own README](https://github.com/gzip/nes-6502-maniac-mansion-decoded)
expressly describes its optional puzzle chain and added room as custom work;
it separately lists cosmetic patches and a Japanese title variant. None were
imported. This primary author description rules out calling that chain a beta
puzzle restoration.

An [uncensored-hack report](https://www.timeextension.com/news/2023/05/maniac-mansion-nes-receives-brand-new-uncensored-version)
is a discovery lead, not proof of NES beta provenance. Its linked original
RHDN author page returned HTTP 403 during this research. Consequently its extra
restoration claims are not promoted to verified NES prototype facts. No patch
or ROM was downloaded. Crockford's linked additional-content page also failed
to load; it supplies no additional verified candidates here.

## Selection and subsequent feasibility

Await the coordinator's existing owner selection, using the IDs above. For any
selected item, first identify authentic NES-version material and compare the
affected retail resource, scripts, object state, hitbox and occlusion masks.
If only computer-version art or a modern recreation is available, label that
explicitly and obtain the owner's choice before substituting it. No reconstructed
asset should be presented as an authenticated beta asset.

Separate decoration/text/credits choices from outcome-changing scripts. Preserve
phone-number and puzzle dependencies; test the complete native sentence path,
capture/death behavior and timing for selected dialogue branches. Establish
save/version isolation before any Enhanced resource or script change. Keep the
owner ROM immutable, all restoration settings OFF by default, and run the exact
staged retail-data guard. No runtime implementation is part of this research.

## Native Select correction

The [Jaleco/Lucasfilm NES manual](https://www.classicgaming.cc/pc/maniac-mansion/files/manual-maniac-mansion-nes.pdf)
printed pages 10 and 12 describes Select cycling Get/Use/Open; page 15 describes
New Kid for character selection. Retail script 4 matches the cycle (native verb
IDs 14, 11, 1); script 164 defines New Kid separately (ID 7). Existing Enhanced
short-tap code forwards a native Select pulse; held Select opens the broader
chooser. This **extends** native quick verbs. Earlier design prose wrongly
attributed the short tap to kid switching and has been corrected. No mapping or
Original execution change was needed for this documentation correction.
