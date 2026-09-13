"""Compile/run the production gate; invalid commissioning combinations must fail."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=False)
    common = {'SD700_FORCE_SERVO_ENABLED':1, 'SD700_MOTOR_MODE_REAL_BENCH':1,
              'SD700_REAL_BENCH_ACKNOWLEDGED':1, 'SD700_REAL_OUTPUT_ARMING_ENABLED':1}
    cases = [('unsupported_press', {'SD700_FORCE_SERVO_COMMISSIONING':1,'FS_PRESS_PROFILE_CEILING':721}, 'outside the selected experimental build'),
             ('unsupported_release', {'SD700_FORCE_SERVO_COMMISSIONING':1,'FS_RELEASE_PROFILE_CEILING':101}, 'outside the selected experimental build'),
             ('locked', {}, ''), ('commissioning', {'SD700_FORCE_SERVO_COMMISSIONING':1}, ''),
             ('bad_flag', {'SD700_FORCE_SERVO_COMMISSIONING':2}, 'commissioning must be 0 or 1'),
             ('no_force', {'SD700_FORCE_SERVO_COMMISSIONING':1,'SD700_FORCE_SERVO_ENABLED':0}, 'requires ForceServo'),
             ('no_ack', {'SD700_FORCE_SERVO_COMMISSIONING':1,'SD700_REAL_BENCH_ACKNOWLEDGED':0}, 'explicit RealBench acknowledgement'),
             ('no_arm', {'SD700_FORCE_SERVO_COMMISSIONING':1,'SD700_REAL_OUTPUT_ARMING_ENABLED':0}, 'explicitly arm real output'),
             ('wrong_backend', {'SD700_FORCE_SERVO_COMMISSIONING':1,'SD700_MOTOR_MODE_REAL_BENCH':None}, 'requires the RealBench backend')]
    results = []
    for name, changes, error in cases:
        definitions = {**common, **changes}
        executable = out/(name+'.exe')
        command = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2', '-I.']
        command += [f'-D{k}={v}' for k,v in definitions.items() if v is not None]
        command += ['Tests/Host/test_commissioning_gate.c','Board/Motor/motor_real_gate.c','-o',str(executable)]
        result = subprocess.run(command, capture_output=True, text=True)
        log = result.stdout+result.stderr
        if error:
            assert result.returncode != 0 and error in log, (name, log)
        else:
            assert result.returncode == 0, log
            run = subprocess.run([str(executable.resolve())], capture_output=True, text=True)
            log += run.stdout+run.stderr
            assert run.returncode == 0, log
        (out/(name+'.log')).write_text(log, encoding='utf-8')
        results.append(dict(name=name, command=command, expected_rejection=bool(error),
                            compile_exit_code=result.returncode, result='PASS'))
        print(name+': PASS')
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    print('COMMISSIONING_GATE_TESTS=9 PASS; NO_HARDWARE')


if __name__ == '__main__':
    main()
