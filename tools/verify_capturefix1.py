"""Run CaptureFix1 software checks in a new evidence directory; no hardware I/O."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import time

from firmware_image import require
from verify_force_servo_firmware import sha

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', default='output/ForceServo1_CaptureFix1/final')
    parser.add_argument('--objcopy-cross-check', action='store_true')
    parser.add_argument('--characterization', action='store_true')
    parser.add_argument('--commissioning', action='store_true')
    args = parser.parse_args()
    out = ROOT/args.output
    require(not out.exists(), 'Use a new output directory; never overwrite evidence')
    out.mkdir(parents=True)
    ps = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File']
    py = [sys.executable]
    commands = [
        ('capture_length_and_observe', ps+['Tests/Host/test_force_servo_capture.ps1',
                                        '-OutputDirectory', str(out/'capture')]),
        ('force_capture_selftest', ps+['tools/capture_force_servo.ps1', '-SelfTest']),
        ('pressure_capture_selftest', ps+['tools/capture_pressure_response.ps1', '-SelfTest']),
        ('auto_capture_selftest', ps+['tools/capture_auto_target_static.ps1', '-SelfTest']),
        ('force_host', py+['tools/run_force_servo_tests.py', '--output', str(out/'host')]+(['--commissioning'] if args.commissioning else [])),
        ('data_schema', py+['tools/force_servo_data.py', '--self-test']),
        ('target250_data', py+['Tests/Host/test_target250_data.py']),
        ('verifier_rejections', py+['Tests/Host/test_force_servo_verifier.py']),
        ('offline_firmware', py+['tools/verify_force_servo_firmware.py']),
        ('historical_auto_firmware', py+['tools/verify_current_auto_target_firmware.py']),
    ]
    if args.commissioning:
        commands.append(('continuous2_range_fixture', py+['tools/run_force_servo_tests.py','--commissioning','--range-fixture','--output',str(out/'range_fixture')]))
        commands.append(('commissioning_gate', py+['Tests/Host/test_commissioning_gate.py', '--output', str(out/'gate')]))
    if args.characterization:
        require(args.commissioning, 'Characterization verification includes commissioning regressions')
        commands += [
            ('characterization_host', py+['tools/run_force_servo_tests.py','--commissioning','--characterization','--output',str(out/'characterization_host')]),
            ('characterization_capture', ps+['Tests/Host/test_force_characterization_capture.ps1','-OutputDirectory',str(out/'characterization_capture')]),
            ('characterization_data', py+['Tests/Host/test_force_characterization_data.py']),
            ('characterization_arm_release', ps+['tools/build_gcc.ps1','-ForceServo','-StaticForceRuntimeCharacterization2','-MotorMode','RealBench','-Configuration','Release','-RealBenchAck','I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION','-BuildDir',str((out/'arm-build').relative_to(ROOT))]),
            ('characterization_rebuild_identity', py+['-c',
                "import sys;sys.path.insert(0,'tools');from pathlib import Path;from verify_force_servo_firmware import sha,PINNED_HASHES;from firmware_image import require;"+
                "b=Path("+repr(str(out/'arm-build/RealBench_ForceServo/Release'))+");"+
                "[require(sha(b/('press_control_f411.'+e))==h,'Rebuilt '+e+' differs from field pair') for e,h in PINNED_HASHES.items()];print('REBUILD_EXACT_HEX_ELF=PASS')"])]
    if args.objcopy_cross_check:
        commands.append(('developer_objcopy', py+['tools/verify_force_servo_firmware.py', '--objcopy-cross-check']))
    records = []
    for name, command in commands:
        started = datetime.now(timezone.utc).isoformat()
        tick = time.monotonic()
        result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log = out/(name+'.log')
        log.write_bytes(result.stdout)
        records.append(dict(name=name, command=command, started_utc=started,
                            elapsed_seconds=round(time.monotonic()-tick, 3),
                            exit_code=result.returncode, result='PASS' if result.returncode == 0 else 'FAIL',
                            log=log.relative_to(ROOT).as_posix(), sha256=sha(log)))
        (out/'test_execution.json').write_text(json.dumps(records, indent=2)+'\n', encoding='utf-8')
        print(f'{name}: {records[-1]["result"]} ({records[-1]["elapsed_seconds"]} s)', flush=True)
        require(result.returncode == 0, f'{name} failed; inspect {log}')
    print('CAPTUREFIX1_SOFTWARE_CHECKS=PASS; physical test NOT RUN')


if __name__ == '__main__':
    main()
