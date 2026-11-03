# Direct protocol building delivery

`Phobos_DeliverProtocolBuilding` (interop 1.4.0) consumes one building effect without SuperClass grant/launch or sidebar registration.
Limbo delivery reuses CreateLimboBuilding and its native build-limit/undeploy semantics. Normal delivery reuses the native trigger CreateBuildingAt construction/ForceCreate/Place ordering at the house base center. S_OK consumes the credit, including the original limbo build-limit skip; S_FALSE means placement unavailable and retryable. Allocation/identity errors do not create a building. Existing superweapon availability query remains unchanged.

run.bat extracts the actual export body into a private test include. Controlled host mocks cover identities, allocation, IDs, build-limit skip, missing base cell, rejected placement cleanup, and buildup/no-buildup initialization. No SuperClass or grant service is available in the fixture. This does not establish real-game placement, native side effects or network synchronization correctness.
