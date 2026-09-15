# BuildToTarget2_StartAnchorFix1 identity and progress semantics

The wire schema remains F10B, with300 diagnostic words,50 active parameter words,66 operating-profile words,198 disabled-catalog words and66 immutable Build-config words. Profile7, Build config version2/digest1817994819 and every output/safety parameter remain unchanged. **Current build identity is4653010E**, replacing4653010D; current scripts/verifier reject the old identity. The register layouts in the [preceding protocol](../BuildToTarget2/PROTOCOL.md) still apply; its older build identity is historical.

`build_progress_anchor` now identifies a per-START force reference after the accepted START's first valid fresh ordered post-START frame. While START is pending, the previous anchor remains visible; duplicate, old/pre-START or invalid frames cannot initialize a new session. A repeated busy/rejected START, plan commit or STOP does not establish a new anchor.

`build_no_response_ms` is independently cumulative. Rebaselining does not clear it. Qualified owner/request-matched post-pulse fresh feedback with net gain>=2 N from the new reference resets this timer under the unchanged rules. Genuine plateau still faults Detail22 at accumulated5000 ms. Full108 s true-OFF thermal epoch reset remains unchanged. All exposure/approach reservations remain cumulative across START/STOP/plans, with no refund.

Capture workflow is unchanged: exact FC03/FC04/FC05/FC06 framing/CRC, configuration and identity readbacks, atomic Target plan, one START, streamed CSV/metadata/report and manual STOP or real fault. There is no normal overall session/capture timer. Target reach remains true OFF plus continued monitoring, never automatic repress.

BUILD pulse gaps are also completely bridge OFF, unlike the old0.2-0.6 V interpulse preload. Physical equivalence is unproven; no energized preload was added. **PHYSICAL_STATUS=NOT_RUN.**
