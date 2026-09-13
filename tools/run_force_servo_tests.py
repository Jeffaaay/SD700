"""Production machine/controller/executor/HW/TIM5 chain with host register shims."""
import argparse, json, subprocess
from pathlib import Path

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--output',default='output/ForceServo1/host')
    ap.add_argument('--commissioning',action='store_true',help='CommissioningUnlock1 with the production arming gate')
    args=ap.parse_args(); out=Path(args.output); out.mkdir(parents=True,exist_ok=True)
    sources=['Tests/Host/test_force_servo.c','Application/force_servo.c',
             'Application/force_servo_machine.c','Application/machine.c',
             'Application/runtime.c','Application/pressure_control.c','Application/auto_target_config.c',
             'Board/Motor/motor_executor_real.c','Board/Motor/motor_hw_real.c',
             'Board/Motor/motor_stop_timer_tim5.c','Tests/Host/fake_stm32_hal.c',
             'Transport/Modbus/force_servo_protocol.c','Transport/Modbus/modbus_semantic_map.c',
             'Transport/Modbus/modbus_rtu_server.c','Protocol/Modbus/modbus_crc16.c']
    defines=['SD700_FORCE_SERVO_ENABLED=1','SD700_MOTOR_MODE_REAL_BENCH=1',
             'SD700_REAL_BENCH_ACKNOWLEDGED=1','SD700_REAL_OUTPUT_ARMING_ENABLED=1',
             'SD700_MOTOR_REAL_HOST_TEST=1']
    if args.commissioning:
        defines+=['SD700_FORCE_SERVO_COMMISSIONING=1','SD700_TEST_PRODUCTION_ARMING_GATE=1']
        sources+=['Board/Motor/motor_real_gate.c']
    results=[]
    for opt in ('-O0','-O2','-Os'):
        exe=out/('force_servo_'+opt[1:]+'.exe')
        cmd=['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic',opt,'-I.','-ITests/Host/Shim']
        cmd+=['-D'+x for x in defines]+sources+['-lm','-o',str(exe)]
        subprocess.run(cmd,check=True)
        result=subprocess.run([str(exe.resolve())],capture_output=True,text=True)
        (out/(opt[1:]+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        print(result.stdout,result.stderr,flush=True)
        results.append({'optimization':opt,'command':cmd,'exit_code':result.returncode})
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        result.check_returncode()
    print('FORCE_SERVO_OPTIMIZATIONS=3 PASS; hardware NOT_RUN')

if __name__=='__main__': main()
