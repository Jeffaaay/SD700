# PressBoost1 / FieldReady1 - field repository index

This repository contains the current source, tools, documentation and exactly
one field candidate HEX/ELF pair. The Git setup changes tracking and entry
documentation only. MCU sources, capture scripts, parameters and both firmware
files are unchanged; no firmware build or hardware operation was performed.

| Review item | Location |
| --- | --- |
| Current field version, status, clone/update workflow | [README.md](README.md) |
| ONE START, static, full AutoTarget instructions and RAM field map | [Static test](Docs/AUTO_TARGET_STATIC_TEST.md) |
| Current Release HEX | [SD700_AutoTarget_PressBoost1_RealBench_Release.hex](output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.hex) |
| Matching Release ELF | [SD700_AutoTarget_PressBoost1_RealBench_Release.elf](output/AutoTarget/firmware/SD700_AutoTarget_PressBoost1_RealBench_Release.elf) |
| Firmware hashes | [Firmware/SHA256SUMS.txt](Firmware/SHA256SUMS.txt) |
| SHA256 of every Git-tracked file except the manifest itself | [SHA256SUMS.txt](SHA256SUMS.txt) |
| Source and configuration | [Application](Application/), [Config](Config/) |
| Host regression source and runners | [Tests](Tests/) |
| Capture tool and its self-test | [capture_auto_target_static.ps1](tools/capture_auto_target_static.ps1) |
| Git output allowlist and byte-preservation policy | [.gitignore](.gitignore), [.gitattributes](.gitattributes) |
| Historical control reference (not calibrated parameters) | [Legacy evidence](Docs/LEGACY_CONTROL_EVIDENCE.md) |

```text
UNCHANGED_HEX_SHA256=26CB9D8AB146A63DCF21F420CCA4AAB256004FF60AD2B08317BCF4FDA7B5AEB2
UNCHANGED_ELF_SHA256=DAF479B97E4BD0E286962EBF16D21FFA235C3450D4E46CA5B6CF6D3DB9725256
```

Target 250 / AUTO_HOLD real PressBoost1 validation is pending. Target uses
sensor control units, not certified Newtons. Success needs real field evidence:
contact, rise above the previous approximately 30 plateau, approach 250,
245..255 and AUTO_HOLD. Use full AutoTarget, not ApproachOnly.

## Local engineering evidence

Historical firmware, baselines, build/test executables, logs, field captures and
ReviewBundle ZIPs remain on the engineering machine and are excluded from Git.
A fresh clone therefore has no historical field candidate to confuse with the
pair above. New captures belong under ignored `output/field/`.

The prior FieldReady1 run's `output/AutoTarget/TEST_RESULTS.md`,
`field_ready1_test_results.json`, `field_ready1_*.log` and audit JSON files are
local evidence, not files provided by this repository. They record the previous
41 C variants, 11 policy cases, capture tests and physical-output-lock check.
Those tests were not rerun for this Git-only setup. Original PressBoost1 build
logs and earlier baselines are also local history. All software pressure inputs
were SYNTHETIC_INPUT; they do not establish physical response.

The earlier complete ZIP, its manifests and the pre-Git README/index/manifest
remain locally under `output/`. Git setup verification records and the saved
pre-edit files are under `output/github_field_setup/` and are also ignored.
The existing `tools/package_auto_target.py` is the historical engineering
packager: it expects those local baseline/test artifacts and is not required
for the field clone/pull workflow.
