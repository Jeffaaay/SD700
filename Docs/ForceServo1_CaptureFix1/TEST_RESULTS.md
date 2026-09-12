# ForceServo1 CaptureFix1 — software verification

Baseline: `29d0a9091b8333c690bcb41f5474dffc3909ef61`. HEAD matched and the initial
worktree was clean. This is a tool repair. Production C/H, control architecture,
PID, trajectory, executor, TIM5 lease, HOLD, limits, deadlines and the compiled
physical output lock are unchanged. No later commits needed reconciliation.

**physical test NOT RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD:
NOT_VALIDATED. COMMISSIONING_NOT_TUNED.** No serial port was opened, no firmware
was flashed and no physical START or powered test was performed.

## Problem and change

The original ForceServo capture read active parameters with FC03, but the shared
production response-length parser supported only FC04/05/06 and exceptions.
The original FC03 failure was reproduced locally in Windows PowerShell before
editing: `Unsupported Modbus response function 0x03.` The log and reproduction
script are retained under `output/ForceServo1_CaptureFix1/` in the delivery.
Historical self-test PASS is not evidence that this path previously worked.

The parser now calculates FC03 length from byte count exactly as for FC04.
Station, function, CRC, declared length, requested word count and bounded timeout
checks remain. No damaged, oversized or partial response is accepted to obtain
a PASS. FC05/06 and exception framing retain their existing semantics.

The existing capture orchestration was extracted into `Invoke-ForceCapture` so
the real entry point and no-hardware tests use the same capability/configuration,
snapshot, observation, STOP and reporting sequence. The simulation injects byte
availability/read callbacks below the production framing reader. It never opens
a port. Existing decoder smoke fixtures remain, and `-SelfTest` now also runs
the production-framing/full-flow suite. There is no output-lock bypass parameter.

The configuration-timeout flow additionally exposed a report defect: a STOP
snapshot with unavailable configuration exports an empty `feedback_gap_ms`.
The report now preserves only that unavailable budget as unknown (`None`),
instead of failing during numeric parsing. Blank pressure still fails; no RAM,
pressure, current or thermal measurement is invented.

## Actual local results

The commands, UTC start times, elapsed times, exit codes and log SHA256 values
are recorded in [verification.json](verification.json). Compiler invocations
are also recorded in `output/ForceServo1_CaptureFix1/verified/host/results.json`.

| Check | Result and scope |
| --- | --- |
| New PowerShell framing/full-flow suite | PASS: 17 low-level cases + 5 capture flows = 22 |
| ForceServo `-SelfTest` | PASS: existing decoder/schema smoke checks plus the same 22 cases |
| `capture_pressure_response.ps1 -SelfTest` | PASS, existing assertions retained |
| `capture_auto_target_static.ps1 -SelfTest` | PASS, existing assertions retained |
| ForceServo host O0 / O2 / Os | PASS: 15 groups per optimization, 45 group executions |
| Python data/schema self-test | PASS: 11 checks, including unknown budget and invalid blank pressure |
| Firmware verifier rejection tests | PASS: 15 unittest methods, with additional mutation subcases |
| Offline package rejection tests | PASS: 5 unittest methods |
| Strict current locked firmware verifier | PASS; 30,512 addressized load bytes compared |
| Historical AutoTarget firmware verifier | PASS; historical pair and configuration remain unchanged |
| Developer objcopy cross-check | PASS; byte-identical original HEX regeneration |

The framing cases cover whole and one-byte-fragmented FC03, incomplete header,
incomplete payload and empty response timeouts, CRC/station/function errors,
overlong frames, 0x83 and wrong exception function, and FC04/05/06 normal and
exception regressions. Every case passes through production
`Read-LengthAwareResponse` / `Get-ModbusResponseLength` and then the existing
request validator where appropriate.

Complete Observe reads capability first, then all 48 active configuration words
with five FC03 reads (11/11/11/11/4 words), then frozen 112-word diagnostic
snapshots in chunks of at most 11 words. It observes for a bounded synthetic
one-second interval, sends one STOP, reads STOP diagnostics and writes CSV,
metadata and report. Every response, including latch/STOP echoes, uses production
framing. Assertions check all 24 parameter values, ZERO START, STOP/readback,
finite observation and `INSUFFICIENT_DATA` for powered performance. Other flows
exercise configuration timeout, diagnostic timeout, device fault and a locked
SingleStart refusal; all assert ZERO START and exactly one STOP. The tests use
a synthetic clock and device; their STOP readback is not hardware stop validation.

The standalone firmware verifier was also executed with an empty `PATH` using
the absolute Python interpreter. It succeeds without ARM executables or Python
packages. Assertions remain active under `python -O`. Rejection tests mutate
temporary bytes: HEX checksums/lengths/addresses/overlaps/entry/omissions, ELF
class/type/endianness/ARM ABI/table and symbol bounds/load overlaps, identity,
parameters, protected limits, compiled unlock, actual hashes and rewritten
manifest pins. Configuration/lock mutation tests reach those content assertions
before hash checks, so a hash mismatch alone cannot mask missing validation.

The pure Python comparison uses ELF PT_LOAD physical addresses, including the
flash initializer of RAM `.data`; BSS/zero-fill bytes are not invented. ELF type,
entry, bounds, section/load mappings and HEX record checksums are checked before
exact address/value comparison. The original pair hashes and manifest remain
mandatory. Optional objcopy cross-check is a separate developer dependency.

## Preserved artifacts and delivery

Original HEX SHA256:
`C76059E0FAA126D5E52A6D640599A007E71D669080C022FEEE6180D3AB414B41`

Original ELF SHA256:
`B6AEFBCAD93DE82A3514C134518A249A6D6851D3E8C4E8DF3EE7C41DC0984EFF`

Both files retain their original bytes and timestamps. All 316 original evidence
files selected at the start (CSV, reports, logs, HEX/ELF and delivered ZIPs) retain
their SHA256 and modification times. The initial inventory and preservation
audit are included. Prior ForceServo/Convergence verification and delivered ZIPs
are historical and are not overwritten.

New synthetic captures are explicitly named `SYNTHETIC.csv`, with synthetic
firmware identity and field notes. Development failure logs remain alongside
the final passing logs: initial FC03 rejection; test-clock boundary, array-property
and callback-scope fixture issues; the unavailable-budget report failure; and
an absolute-path test-runner issue. These are local software results, not field
captures. Their corrections are covered by the final successful runs.

The new packager checks clean committed main against remote main, baseline
production/evidence preservation, current verification source hashes, historical
source hashes at the reviewed commit, all referenced historical log hashes,
current root source manifest, strict locked firmware and every packaged entry.
It refuses to overwrite an existing delivery. The ZIP contains the source,
unchanged HEX/ELF, actual logs/results and synthetic captures. Its package
manifest and `SOURCE_OF_TRUTH.json` identify the exact delivered contents/commit.
`verify_force_servo_capturefix1_package.py` checks either the ZIP or its extracted
directory offline; a separate `.zip.sha256` gives the archive's actual digest.

ARM rebuild: **NOT RUN**, because production source and binaries are unchanged.
Physical serial timing, MCU binary readback, hardware stop path, current/voltage,
thermal load, static250 and rotating load: **NOT RUN / NOT_VALIDATED**.
Software PASS cannot qualify continuous-mode hardware stopping or authorize
unlocking. Follow only the [current README](../../README.md): motor power
physically disconnected, MCU/sensor correctly powered, locked firmware, COM5,
Observe at most 60 seconds, ZERO START, retained current limit, no pressure
250/500 trial, one capture only. Return CSV, report, metadata and one field note.
