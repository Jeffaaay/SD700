# GeneratedArtifactsCleanup1 - ignored output only

PHYSICAL_STATUS=NOT_RUN. Baseline main/origin/main was1e5b839b014570469ecf9607a59cd6475952f63e with a clean worktree. This change adds a maintenance script and its isolated tests, updates cleanup/ignore-policy documentation, and leaves every existing controller source, configuration, verifier/build/capture tool and formal firmware byte unchanged. The ignore patterns are unchanged; only their explanatory comment is updated.

## Repeatable cleanup

```powershell
python tools/clean_generated_artifacts.py
python tools/clean_generated_artifacts.py --apply
```

Default invocation saves an exact dry-run outside the checkout (under the Windows temporary directory) and prints paths grouped by output subtree, file counts, bytes to delete and expected remaining size. `--apply` saves a fresh plan, rechecks it and deletes only its eligible ignored output. To apply a previously reviewed inventory, pass `--plan <absolute-saved-plan.json> --apply`; any intervening content, ignore policy, capture, fixture, tracked-file, HEAD or Git-status change causes refusal before deletion. Inventories/receipts are never overwritten. A computed fixture can be retained with `--keep output/path` when creating the plan.

The scope is only output/. Git-tracked and nonignored files survive, as do literal existing test/tool input paths and explicit --keep paths. Every tracked file across the repository is hash-checked before/after apply. Docs, Reference, Tests, tools and caches outside output are not cleanup targets. The established read-only Windows process check refuses active compiler/build/test/capture tasks; no processes are killed.

CSV/metadata/report families are preserved unless their metadata explicitly identifies synthetic firmware AND synthetic initial-gap/current-limit inputs. A SYNTHETIC filename or NOT_RUN status alone is insufficient; real firmware hashes, serial-port indications, malformed/missing metadata and uncertainty preserve the family directory. Field/captures/physical directories remain protected. The current source dependency audit found only already tracked firmware inputs; the complete regression below checks that no required fixture was removed.

Ignored ARM/host builds, map/object/dependency/executable/cache files, verification/dev/focused/final/repro software output, synthetic capture triplets and the disposable nested clone_verification/.git may be removed. Main Git data is outside the allowed path; other nested Git trees and links/junctions are skipped. Every file deletion resolves inside the checkout's output directory, verifies its hash, and removes that individual file. Only then are empty approved directories removed. There is no git clean, recursive wildcard deletion, history rewrite, source refactor, ZIP or serial I/O.

This policy intentionally supersedes RepositoryCleanup1's earlier retention of ignored final/repro/raw software logs. Existing tracked historical documents and their original recorded hashes remain untouched; their raw output paths describe files that existed at the time of those runs. Such software outputs are now disposable, while tracked evidence and real/uncertain physical captures remain protected.

## Actual initial cleanup

Before deletion, all34 tracked firmware hashes were recorded, including the current pair and the original32 historical files. Actual audit:9340 files and202185232 bytes under output;9306 files were Git ignored, with no nonignored untracked files. All1552 CSV families had explicit synthetic metadata; no physical or uncertain capture family was identified in output. The only nested Git deletion scope was output/ApproachMeasure1/clone_verification/.git. Every literal existing test/tool input was tracked and preserved.

| Initial cleanup measure | Actual value |
|---|---:|
| BEFORE_SIZE (output bytes) |202185232|
| AFTER_SIZE (output bytes) |10556130|
| REMOVED_FILES |9306|
| REMOVED_BYTES |191629102|
| Remaining firmware files |34|
| Active build/test/capture tasks at apply |0|
| Git status immediately after apply |clean|

The dry-run plan and successful receipt are local audit records outside the repository:

- Directory: `C:/Users/jian/AppData/Local/Temp/SD700_GeneratedArtifacts_20260915T022151Z`
- Exact plan: `initial-dry-run.json`; SHA256 `C268DBF093BC582B54F568CD426345E9BA97B1969E6D2865B22F0F09FF8E4BAC`
- Receipt: `initial-dry-run.applied-f7545d3f.json`; SHA256 `162A3CC29C15F3804C7157954C50E312949F60690ABC65C9D06E872759ED9F1D`
- `baseline.json` and `dry-run-review.json` record initial clean HEAD, firmware hashes, synthetic classification and the retained dependency list.

The initial script was tested/staged outside the checkout and invoked with `--root C:/Users/jian/Desktop/SD700-GitHub --plan <audit-directory>/initial-dry-run.json`, first without and then with --apply. Its bytes were copied unchanged to tools only after cleanup had completed and clean Git status was verified. Deleting ignored files created no Git deletion entries. The script/policy commit is separate from firmware/controller work.

Current candidate remains BuildToTarget2_CoolingAnchorFix1, F10C/46530110/profile7. [Current HEX/ELF paths](../Firmware/ForceServo1.SHA256SUMS.txt):

```text
CURRENT_HEX_SHA256=11D2525F2FE0F206453D09CEF4DD483C29CBD1812E7097BA53B6854FB48F25FF
CURRENT_ELF_SHA256=6082D8EB0DE290040961EED0F0E388EE76536D35834BCAF62827D940F41B8507
```

## Actual post-cleanup validation

`python Tests/Host/test_clean_generated_artifacts.py`:8 tests PASS after installation (the identical external staged copy also passed8 before apply). Tests cover default dry-run/no mutation, exact ignored-only removal with clean Git status, tracked historical/current firmware, nonignored output, explicit/computed fixtures, physical/uncertain captures inside generated directories, changed hash/ignore-policy/inventory refusal, new real capture appearing after dry-run, active task refusal, read-only disposable Git packs, main/foreign Git protection, traversal and a real Windows junction. Installed-run raw log SHA256: `BEEBC776F209D1F618043BE7DEACCEA4BF7E0BE113842DED5089B753E8400FEB`.

The complete dependency regression was actually run after the initial cleanup:

```powershell
python tools/verify_build_to_target.py --output output/GeneratedArtifactsCleanup1/verification
```

All23 components PASS:

| Check | Actual result |
|---|---|
| Existing host/policy regression |33 groups +22 cases PASS|
| Locked / commissioning / range-boost ForceServo |15 /42 /55 groups at each O0/O2/Os PASS|
| Historical Runtime2 / current Build |15 /27 groups at each O0/O2/Os PASS|
| Production parser/Observe and Force/pressure/AUTO self-tests |PASS; no hardware or serial connection|
| Runtime2 / Build capture |24+6 /30+6 cases PASS|
| Schema / target / Runtime2 / Build data |11 /15 /5 /7 tests PASS|
| Compile/arming gates |19 cases PASS|
| Strict verifier rejection/equality tests |36 tests PASS|
| Current / historical AUTO verifier |PASS|
| ARM Release / exact rebuild identity / objcopy cross-check |PASS|

The same-environment rebuild produced HEX and ELF SHA256 values exactly equal to the current hashes above. All34 formal firmware paths/hashes remain unchanged. The846 existing tracked files outside the five intentional cleanup-policy/manifest edits were also checked byte-for-byte against1e5b839; this includes every existing production/build/verifier/capture source, configuration, reference and historical firmware. No deleted ignored artifact was needed by the current regression.

The external audit directory contains `verification.json` with all actual commands, timestamps, exit codes and original-log hashes; `verification-logs/` retains these small new software logs, `installed-cleanup-tests.json` records the8 cleanup tests, `unchanged-tracked-inputs.json` records the unchanged-source check, and `rebuild-proof.json` pins exact HEX/ELF equality. No generated binary, synthetic dataset or large raw inventory is added to Git. The root tracked-file SHA256 manifest is updated only for this script/test/policy change.

The size/removal table measures the initial inventory, not cumulative deletion counts across repeated builds. Verification generates new ignored files; use the same cleanup command after verification to restore output to the preserved34 files/10556130 bytes. Default audits stay outside output so they cannot repopulate it. No change to the firmware candidate, runtime behavior, output parameters or safety architecture is part of this commit.

PHYSICAL_STATUS=NOT_RUN. No serial connection, flashing, machine motion, ZIP, physical force/current/temperature measurement or hardware qualification. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain unvalidated.
