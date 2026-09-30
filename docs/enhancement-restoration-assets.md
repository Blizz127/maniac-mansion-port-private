# Selected NES restoration: retained artwork investigation

Task 3b6b23789a0f; inspected 2026-09-30 after the owner selected all five
restoration groups. This lane covers the statue, Ted's posters and removed
sign artwork. No restoration, asset import or ROM modification was performed.
Any later implementation is Enhanced-only and OFF by default; Original and
owner-local ROM files remain unchanged.

**Authentic retail material survives, including readable anti-disco artwork.**
That does not yet establish complete original artwork assembly, placement,
palette or occlusion behavior. None of these groups has a verified, safe
restoration mechanism ready for implementation.

## New local resource evidence

The identified owner-local USA ROM retains the pinned PRG SHA-256
`84f5377980d2fd44d71faec42f858b1e83540c2f55aba9236c3279d6dde8592a`.
Its 40 room graphics resources were decoded read-only. Each consumed exactly
its catalogued compressed length and expanded to its declared tile count.
Room backgrounds and object image references were then inspected, independently
corroborating the native object-loader research.

| Selected group | Retained retail resources | What remains unresolved |
| --- | --- | --- |
| Hall statue | Room 13, statue object 368 and plaque 369; dimensions 9×8 and 3×1 tiles. Their x coordinates 70/72 exceed the room's 60-tile width. Both image references point to the first object-code address, rather than a separate bitmap. Tileset 12 retains 29 tiles not referenced by the inspected background/object images. | Which surviving fragments belong to the original statue; authenticated nametable arrangement, original placement and palette. An off-screen object label is insufficient to restore a complete statue. |
| Ted calendar and mummy | Room 25, calendar object 318 (3×5 tiles) and separate poster 319 (6×4). Both x coordinates are 60 in a 28-tile room and both use the no-separate-image reference. Calendar Read code retains its date/deathday clue; poster Read code identifies the mummy subject. Shared tileset 14 is also used by arcade room 18; 23 local tiles remain unreferenced after inspecting both rooms' backgrounds/object images. | Authentic calendar/mummy tile arrangement, placement and palette. The unused tiles cannot all be attributed to posters merely because those object labels survive. Preserve calendar date/deathday semantics. |
| Anti-disco poster and other signs | Green Tentacle's room 20 retains poster 405, 3×2 tiles, y=2, x=40 outside its 28-tile width, without verb handlers. Tileset 16 contains six unused native tiles that can be arranged into readable anti-disco text. Poster 404 is a different, surviving poster; arcade objects 205/441 are games and are not the removed poster. | The original poster's complete nametable placement, palette and mask behavior. Other signs need their own resource-specific evidence; a positive anti-disco result does not authenticate all missing sign art. |

The no-separate-image reference is also normal for objects whose pictures
already appear in the room background. Therefore these references do **not**
prove that all artwork was deleted. They prove that there is no independently
addressed object bitmap to restore by changing only an object visibility flag.

For the anti-disco contact sheet, the native tile indices are 108/109/110 on
one row and 115/116/117 on the next. The resulting two-row text is legible;
all six tiles are unreferenced in room 20's inspected background and object
images. This establishes retained source pixels, not an authenticated original
room-coordinate/palette/mask recipe. It does not authorize a guessed overlay.

Shower-sign restoration must retain the current game's phone-number drawing.
The native initializer selects among five digit arrangements, and later phone
logic consumes those digits. The historical article's example number is not
a universal number for every retail run. Replacing the sign with a fixed
number would change a puzzle dependency.

## Sources and interpretation

[Douglas Crockford's first-person account](https://www.crockford.com/maniac.html)
establishes the removal history of the statue, Ted's two posters and anti-disco
poster, and explains why the shower clue remained necessary. It does not
provide a complete, authenticated NES resource assembly for these items.

The pinned [ScummVM resource adapter](https://github.com/scummvm/scummvm/blob/abd1ca3e317748489acd8143403615029f973ee3/engines/scumm/file_nes.cpp)
was read for resource boundaries. The corresponding
[NES graphics decoder](https://github.com/scummvm/scummvm/blob/abd1ca3e317748489acd8143403615029f973ee3/engines/scumm/gfx.cpp)
and [SCUMM-NES room parser](https://github.com/gmarty/scumm-nes/blob/master/src/lib/parser/parseRooms.js)
were read for format interpretation. No implementation, resource tables,
patches or third-party assets were imported into tracked files. The numerical
findings above come from read-only analysis of the owner's retail PRG.

## Local artifacts and next step

Images remain ignored owner-local game-derived artifacts under the display
worktree's `build/asset-research/`:

- `room13-background-gray.png`, `room13-alltiles-gray.png`, `tileset12-unused-gray.png`
- `room25-background-gray.png`, `room25-alltiles-gray.png`, `tileset14-unused-gray.png`
- `room20-background-gray.png`, `room20-alltiles-gray.png`, `disco-candidate-gray.png`

These are grayscale decoding/contact-sheet evidence, not gameplay screenshots
or reconstructed authenticated beta art. The unused-tile counts concern the
identified tilesets and inspected references, not an exhaustive search of
every possible script-selected resource or unused byte sequence in the ROM.

For a later Enhanced anti-disco implementation, a possible mechanism is to
reuse the owner's six native tile payloads in a resource/render override,
leaving the cartridge file unchanged. First verify original placement,
palette and mask/actor occlusion behavior from authenticated material.
Presentation-only overlays could otherwise paint over foreground actors;
moving object 405 also requires checking native cursor bounds and state.
There is no verified coordinate or overlay patch in this research.

For statue and Ted's posters, obtain authenticated NES assembly/reference
material before reconstructing missing arrangements. No beta ROM is available
in the inspected local project material. No ROM was downloaded, no computer
version art was substituted, and no invented art was generated. Dialogue and
credits feasibility belongs to the separate script lane. Selection is
authorized; incomplete source assembly still limits what can be implemented
honestly.
