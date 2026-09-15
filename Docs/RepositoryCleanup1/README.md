# RepositoryCleanup1 - separate file-only commit B

PHYSICAL_STATUS=NOT_RUN. Commit A is `1f4c64a2bac995a19ce4bcd0e6b8020b7f7fd700`, already pushed and verified against origin/main before cleanup. It contains BuildToTarget2_CoolingAnchorFix1. Commit B does not change firmware source, configuration, identity or image bytes.

Before cleanup, [commit_a_firmware.json](commit_a_firmware.json) recorded all34 tracked HEX/ELF paths and actual SHA256 values: the original32 paths plus A's new pair. The current hashes remain:

- HEX `11D2525F2FE0F206453D09CEF4DD483C29CBD1812E7097BA53B6854FB48F25FF`
- ELF `6082D8EB0DE290040961EED0F0E388EE76536D35834BCAF62827D940F41B8507`

Use the [current firmware manifest](../../Firmware/ForceServo1.SHA256SUMS.txt) and [A handoff](../BuildToTarget2_CoolingAnchorFix1/HANDOFF.md). No candidate rename or rebuild-driven identity change occurred in B.

The default dry-run command actually executed was:

```powershell
python tools/cleanup_generated.py --plan Docs/RepositoryCleanup1/dry-run.json
```

[The exact inventory](dry-run.json) records each canonical relative path, byte length, SHA256 and deletion reason, along with HEAD and the fingerprint of retained paths/content. The script rejects path escapes, symlinks/junctions, Git paths, tracked/source/reference/firmware/evidence files, changed candidate bytes and changed retained bytes. The only tracked-file exception is the root INA240 duplicate after comparison with its retained Reference copy. A dry-run never deletes. Existing inventories/receipts are never overwritten; reuse requires a new plan filename. The development cleanup tool uses Python3.12 and Windows PowerShell/CIM for its process check; it is not a field capture prerequisite.

The apply command actually executed, after the dry-run scope review and all active build/test tasks had finished, was:

```powershell
python tools/cleanup_generated.py --plan Docs/RepositoryCleanup1/dry-run.json --apply --result Docs/RepositoryCleanup1/applied.json
```

The real process inventory returned no active compiler/build/test/capture tasks. The tool preflighted every listed file before the first removal, checked each resolved path/hash again, and deleted individual files only. [The receipt](applied.json) pins the inventory SHA256 and records identical before/after retained fingerprints:9493 files,226042824 bytes, path/content digest `F31008F2209092979DCEA3C228DD7A06057173869E7059B5F87DC14E68166D9E`.

| Removed category | Files |
|---|---:|
| Untracked regenerable objects/dependency files |1309|
| Untracked regenerable host test executables |137|
| Python cache files |5|
| Identical root INA240 PDF |1|
| Total |1452|

The removed files totaled47160552 bytes. This is the actual deletion total, not a claim about net final disk savings after verification regenerates outputs. All HEX/ELF images, including untracked unique historical firmware, remain. CSV/metadata/report, every existing log, final runs and explicit failed/reproduction runs remain. All Reference sources, SDK files, existing regression tools and fixtures retain their original bytes. Cleanup did not delete directory trees or Git data, rewrite history, connect a serial port or create a ZIP. New verification outputs are retained as final evidence.

The root `ina240.pdf` and [Reference/Hardware/ina240.pdf](../../Reference/Hardware/ina240.pdf) were both2247687 bytes with SHA256 `3ACA6049D67D03B39A2D7583157737D5EC0E67884C2D16A8E23A4FE6B815E6B2`. Only the root duplicate was removed. README and the Phase1 provenance inventory now link to the retained copy; historical source names remain described as provenance. [Repository index](../REPOSITORY_INDEX.md) separates current, historical and test-fixture entry points. The historical AUTO index no longer labels its HEX as the current Release.

[unchanged_inputs.json](unchanged_inputs.json) verifies833 existing tracked files against commit A, including all firmware sources/configuration, build/test/verifier tools, prior fixtures, Reference sources and all34 firmware files. Only documentation, the root tracked-file SHA256 manifest and the duplicate PDF differ among existing tracked files. New files implement/document/test the cleanup workflow; no source refactoring or SDK trimming occurred.

Cleanup rejection/preservation tests were actually run before and after apply:

```powershell
python Tests/Host/test_cleanup_generated.py
```

All10 tests passed in both runs, including default dry-run/no deletion, exact apply, changed-file and retained-log refusal before deletion, protected-file injection, traversal/Git/alias rejection, a real Windows junction, active-task refusal, mismatched PDF refusal, failed/final run retention and HEAD/totals rejection. The junction test creates/removes only a link inside its disposable fake repository.

After cleanup, the complete dependency regression and firmware validation command was:

```powershell
python tools/verify_build_to_target.py --output output/RepositoryCleanup1/verification-after
```

All23 checks passed: existing host/policy tests; locked, commissioning, range/boost, historical Runtime2 and current Build suites at O0/O2/Os; production capture/Observe and old capture self-tests; capture/schema/report/policy tests; compile/arming gates;36 strict verifier tests; current and historical firmware verifiers; ARM Release; exact rebuilt HEX/ELF equality to commit A; developer objcopy cross-check. Current Build has27 groups per optimization. The pure Python field verifier remains strict and independent of ARM executables.

[verification.json](verification.json) records the23 actual regression/build commands, results and raw-log hashes; [software_test_output.txt](software_test_output.txt) contains their normalized output. Both cleanup-test runs are recorded separately in [cleanup_tests.json](cleanup_tests.json) and [cleanup_test_output.txt](cleanup_test_output.txt). [firmware_after.json](firmware_after.json) records all34 preserved firmware hashes and the same-environment rebuilt pair equality. The root SHA256SUMS.txt is updated for the final tracked tree and excludes itself.

Not run: serial connection/capture, flashing, powered machine motion or any physical validation. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated. Synthetic/host PASS is not field qualification. PHYSICAL_STATUS=NOT_RUN.
