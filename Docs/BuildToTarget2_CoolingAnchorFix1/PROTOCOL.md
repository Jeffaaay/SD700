# CoolingAnchorFix1 protocol

F10C schema,46530110 build ID,profile7, Build config version3/digest2848875769. The38-u32 immutable config and124-u32/32-float diagnostic layout are unchanged from [Interpulse1](../BuildToTarget2_Interpulse1/PROTOCOL.md). Only firmware build identity/hash differ. All38 configuration words are compared byte-for-byte against the preceding ELF by strict verifier tests. Current HEX/ELF paths/hashes are in [HANDOFF.md](HANDOFF.md).

The correction is observable through full_rest_remaining_ms: when a failed preload-to-pulse transition really disables the bridge, its countdown starts at108000, not from the preceding pulse end. Subsequent repeated STOP does not shift that countdown or clear reservations. No added runtime parameter. PHYSICAL_STATUS=NOT_RUN.
