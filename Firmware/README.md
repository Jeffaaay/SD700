# Current firmware: Target250Continuous2

**POWERED_TEST_READY=NO; physical test NOT RUN.**
This is a software configuration/scaling and reduction-path repair. The explicit
RealBench image retains the previous100/100 output ceilings and arming gate;
it is not a locked image. Higher continuous output is not justified or armed.
Default ForceServo builds remain LOCKED. Do not repeat the unchanged100-cap
powered trial as a solved performance issue. See [current README](../README.md).

[HEX](../output/Target250Continuous2/firmware/SD700_ForceServo1_Target250Continuous2_RealBench_Release.hex)

SHA256: `2B99E36C3694BDC8A9E9A0145161CBA537432607AA3A2268176CD62A18D0CA83`

[ELF](../output/Target250Continuous2/firmware/SD700_ForceServo1_Target250Continuous2_RealBench_Release.elf)

SHA256: `CE5CF188C03AA229288B57DF766615BF5819B0FD9E9494BDB9BF3967C62E35AA`

`python tools/verify_force_servo_firmware.py` checks the exact pair against
[ForceServo1.SHA256SUMS.txt](ForceServo1.SHA256SUMS.txt), strict ELF/HEX load bytes,
identityF103/build46530104,24 exact defaults,16-word contract including direction
ceilings100/100, operating caps100/100, readiness0 and immediate-reduction policy1.
Target250, max275/raw325, wait250, age20/gap125/lease130, build30000/session45000
and saturation5000 remain.30312 addressed load bytes match. Python standard library
suffices offline; objcopy is optional developer cross-check only. No checks skipped.

The previous [Target250MVP1 manifest](Target250MVP1.SHA256SUMS.txt),
[CommissioningUnlock1 manifest](CommissioningUnlock1.SHA256SUMS.txt),
[locked manifest](ForceServo1_Locked.SHA256SUMS.txt) and [AutoTarget manifest](SHA256SUMS.txt)
and their firmware remain unchanged. Historical instructions do not authorize a
current powered trial. GitHub main is source of truth; no ZIP. Physical STOP,
HOLD/Target250, motor current, thermal duty and rotating load are NOT_VALIDATED.
