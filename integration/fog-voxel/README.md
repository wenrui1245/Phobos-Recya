# Voxel building fog snapshots (v3)

The native frozen-building record saves SHP frames but no VXL turret/barrel
image. Before a visible freeze, this module copies the native building-overlay
render into owned single-frame SHP data using an empty destination clip. Both
cached and uncached/recoiling voxel paths are captured. Fog drawing replays the
saved pixels, palette choice, tint, light and offsets, without sampling the live
hidden building.

## Ownership and all native callers

The native engine calls FreezeInFog from both CellClass::FogCell (486B84) and
Building AI (440C03). AI passes Visible=false for bookkeeping snapshots. The
common entry hook at 457AA0 prepares images only when Visible=true. The binding
hook at 4D10D3 reads the source building from EBX and ignores invisible/non-VXL
snapshots. It must not read ESP+54 at 4D1310: the animation loop overwrites that
argument with AnimTypeClass* at 4D119A.

Each captured image belongs to a specific native FoggedObjectClass instance.
Different or invisible snapshots at identical coordinates cannot replace or
erase it. Destruction removes exactly that owner's image; building destruction
clears transient capture data and scenario clearing releases everything.

The FVX2 global save block stores the native owner pointer along with the image.
Native AbstractClass loading (410380 -> 6CF2C0) registers its old/new addresses.
After validating the entire image block and moving the final vector into place,
the module registers stable owner-field addresses with the normal Phobos/Ares
swizzle manager. No renderer/cache/converter pointers are persisted. FVX1
coordinate-only blocks remain readable through one-time binding by position,
native first-record building type and visibility. Empty pre-feature blocks are
also accepted. Already-fogged old snapshots with no saved pixels need a new
reveal/fog cycle. New FVX2 saves require a build supporting this format.

## Host verification

Run `integration\fog-voxel\run.bat <python.exe>` from the root with Visual Studio
C++ tools installed. It compiles the production module and real constructor and
common-entry hook bodies with simulated game drawing and a deferred swizzle
queue, plus the production byte streams. Tests cover cached/raw image ownership,
cache address reuse, draw order, shadow exclusion, viewport/camera offsets,
hidden-state isolation, both visible/invisible freeze entry calls, destruction
of another same-coordinate snapshot, relocated native owners after loading,
FVX1 compatibility, empty legacy blocks and all truncated populated-block
prefixes. The same-coordinate invisible-snapshot regression fails with v2 and
passes with v3. The actual renderer/engine swizzle instructions are not executed
by the host fixtures; in-game validation remains necessary.

`python integration/fog-voxel/check-native-layout.py <gamemd.exe>` checks the
native bytes for the two freeze paths and constructor argument reuse.
`python integration/fog-voxel/check-built-hooks.py <Phobos.dll>` checks all six
final declarations in the actual DLL, their addresses and sizes, absence of the
obsolete 4D1310 hook and the v3 diagnostic marker.

Hook conflict scripts found no address/size conflicts. IDA MCP was unavailable;
native instruction/register/stack and relative-instruction checks used the local
game disassembly as supplemental evidence. SyringeEx relocates the calls at
70750E/707736/707235 and the jump at 4D2203; the 457AA0 and 4D10D3 stolen bytes
contain no relative instructions.

## In-game check and diagnostics

Verify new games, partial-to-full fog transitions, repeated reveal/fog, four
facings, firing/recoil, camera edges, save/load under fog, and multiplayer with
the same DLL. Inspect mixed SHP/VXL overlap separately. [FogVoxel v3] logs at most
16 freeze outcomes and 16 first-draw outcomes per scenario. Only visible VXL
snapshots produce missing-image messages, with building ID and capture-stage
counters. Oil derricks and hidden bookkeeping snapshots are not errors. These
are bounded Release diagnostics, not a Debug build or per-frame file writes.
