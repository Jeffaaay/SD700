"""BuildToTarget2_RiseDiagnostic1 software evidence runner. Never connects a serial port."""
import argparse, json, subprocess, sys, time
from datetime import datetime, timezone
from pathlib import Path
from firmware_image import require
from verify_force_servo_firmware import sha

ROOT=Path(__file__).resolve().parent.parent

def main():
    p=argparse.ArgumentParser(); p.add_argument('--output',required=True)
    p.add_argument('--only',nargs='+',help='Named checks for focused revalidation; omission runs the complete suite')
    a=p.parse_args()
    out=ROOT/a.output; require(not out.exists(),'Use a fresh evidence directory; never overwrite logs')
    out.mkdir(parents=True)
    ps=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File']; py=[sys.executable]
    host=['tools/run_force_servo_tests.py']
    commands=[
        ('host_regression',ps+['tools/run_host_tests.ps1','-OutputDirectory',str((out/'host_regression').relative_to(ROOT))]),
        ('plain_force',py+host+['--output',str(out/'plain')]),
        ('commissioning_force',py+host+['--commissioning','--output',str(out/'commissioning')]),
        ('range_boost_fixture',py+host+['--commissioning','--range-fixture','--output',str(out/'range')]),
        ('runtime2_force',py+host+['--commissioning','--characterization','--output',str(out/'runtime2')]),
        ('build_to_target_force',py+host+['--commissioning','--characterization','--build-to-target','--output',str(out/'build_host')]),
        ('capture_length_observe',ps+['Tests/Host/test_force_servo_capture.ps1','-OutputDirectory',str(out/'capture')]),
        ('force_capture_selftest',ps+['tools/capture_force_servo.ps1','-SelfTest']),
        ('pressure_capture_selftest',ps+['tools/capture_pressure_response.ps1','-SelfTest']),
        ('auto_capture_selftest',ps+['tools/capture_auto_target_static.ps1','-SelfTest']),
        ('runtime2_capture',ps+['Tests/Host/test_force_characterization_capture.ps1','-OutputDirectory',str(out/'runtime2_capture')]),
        ('build_to_target_capture',ps+['Tests/Host/test_build_to_target_capture.ps1','-OutputDirectory',str(out/'build_capture')]),
        ('schema_selftest',py+['tools/force_servo_data.py','--self-test']),
        ('target250_data',py+['Tests/Host/test_target250_data.py']),
        ('runtime2_data',py+['Tests/Host/test_force_characterization_data.py']),
        ('build_to_target_data',py+['Tests/Host/test_build_to_target_data.py']),
        ('compile_gates',py+['Tests/Host/test_commissioning_gate.py','--output',str(out/'gates')]),
        ('strict_verifier_rejections',py+['Tests/Host/test_force_servo_verifier.py']),
        ('offline_firmware',py+['tools/verify_force_servo_firmware.py']),
        ('historical_auto_firmware',py+['tools/verify_current_auto_target_firmware.py']),
        ('arm_release',ps+['tools/build_gcc.ps1','-ForceServo','-BuildToTarget2','-MotorMode','RealBench','-Configuration','Release',
                          '-RealBenchAck','I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION','-BuildDir',str((out/'arm').relative_to(ROOT))]),
        ('rebuild_identity',py+['-c',
            "import sys;sys.path.insert(0,'tools');from pathlib import Path;from verify_force_servo_firmware import sha,PINNED_HASHES;from firmware_image import require;"+
            "b=Path("+repr(str(out/'arm/RealBench_ForceServo/Release'))+");"+
            "[require(sha(b/('press_control_f411.'+e))==h,'Rebuilt '+e+' differs from field pair') for e,h in PINNED_HASHES.items()];print('REBUILD_EXACT_HEX_ELF=PASS')"]),
        ('developer_objcopy',py+['tools/verify_force_servo_firmware.py','--objcopy-cross-check'])]
    if a.only:
        require(set(a.only)<={name for name,_ in commands},'Unknown check name')
        commands=[(name,cmd) for name,cmd in commands if name in a.only]
    records=[]
    for name,command in commands:
        started=datetime.now(timezone.utc).isoformat(); tick=time.monotonic()
        r=subprocess.run(command,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        log=out/(name+'.log'); log.write_bytes(r.stdout)
        records.append(dict(name=name,command=command,started_utc=started,elapsed_seconds=round(time.monotonic()-tick,3),
                            exit_code=r.returncode,result='PASS' if r.returncode==0 else 'FAIL',log=log.relative_to(ROOT).as_posix(),sha256=sha(log)))
        (out/'test_execution.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
        print(f'{name}: {records[-1]["result"]} ({records[-1]["elapsed_seconds"]} s)',flush=True)
        require(r.returncode==0,f'Check failed; actual log retained: {log}')
    print(('BUILD_TO_TARGET_SELECTED_CHECKS' if a.only else 'BUILD_TO_TARGET_SOFTWARE')+'=PASS; PHYSICAL_STATUS=NOT_RUN')

if __name__=='__main__': main()
