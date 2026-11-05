# Attachment format repair

Run `run.bat <python.exe>`. The test extracts the actual production serializers
and Load/Save bodies, using controlled scalar payload fields and the real Phobos
stream reader/writer. It verifies framing and record separation, not live pointer
swizzling or full game save/load. Existing attachment-death regression covers
the unchanged lifetime logic separately.

New records have an odd, versioned magic before the legacy payload. Old payloads
start with an aligned saved type pointer, including null, so can be distinguished
without consuming bytes or relying on an enclosing per-record block. New fields
follow the legacy payload only when the header is present. Unknown revisions and
truncated headers/payloads fail instead of silently shifting following records.

Pre-change legacy saves retain their original targetability. They lack original
child ownership history, so a later detachment retains the current owner.
Fresh games retain the added INI options and their configured/default values.

DeepSeek's unversioned intermediate format inserted fields inside the legacy
payload. It cannot be reliably distinguished from the older format. Saves made
with that experimental build are not supported by this migration; retain that
build for accessing them. Newly written saves require this repaired reader.

The unconditional AttachmentTrace.log instrumentation was removed; damage math
is unchanged. Bridge checks now use TryGetCellAt so invalid cells really return
false, rather than querying the game's InvalidCell sentinel.
