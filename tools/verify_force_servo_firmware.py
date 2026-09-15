"""Exact BuildToTarget2_Pulse10msFix1 hashes, ARM ELF data and bounded arming profile."""
import argparse, hashlib, json, struct, subprocess
from firmware_image import ElfImage, compare_images, require
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
STEM='SD700_ForceServo1_BuildToTarget2_Pulse10msFix1_RealBench_Release'
FW_RELATIVE='output/BuildToTarget2_Pulse10msFix1/firmware'
FW=ROOT/FW_RELATIVE
PINNED_HASHES={
    'hex':'86C6205D036C0212F1B7D2DD56BA08CF3BB8D9E20072D9DA9A64100193AFAD96',
    'elf':'8819B57A5BAEC0091ABB171E560386F64D45AA6256E5967FBC1E824D153F0EC2',
}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()

def symbols(path):
    return ElfImage(path.read_bytes()).symbols


def verify_contents(elf_data, hex_data):
    image=ElfImage(elf_data)
    sym=image.symbols
    verify_contract(sym)
    compare_images(image,hex_data)
    return image


def verify_contract(sym):
    require(sym['g_force_servo_contract']==struct.pack('<20I',0xF10C,0x46530111,1,3000,3000,0,8500,100,250,250,125,130,0,100,1,1,8500,0,4,3),'Firmware identity/configuration assertion failed')
    require(sym['g_force_characterization_contract']==struct.pack('<16I',1,1,3000,0,0,240,1,2,4,4,5000,0,30,2,25,2),
            'Runtime characterization configuration assertion failed')
    defaults=(10,0,0,.02,200,1000,1000,0,100,-1000,1000,5,0,5,125,20,130,5,10,0,5000,50,5000,2,500)
    require(sym['g_force_servo_default_config']==struct.pack('<25f',*defaults),'Unexpected default parameter group')
    profile=(7,2,0,0,3000,1,0,0,0,3000,3000,0,10,0,100,0,4,4,30,0,0,0,0,500,2,5000,1,3,1,2,5000,2,25)
    require(sym['g_force_servo_profile']==struct.pack('<33f',*profile),'Unexpected operating profile: units/calibration/force/current/time/boost configuration')
    candidates=b''.join(struct.pack('<33f',cid,0,0,0,325,1,0,0,0,275,325,0,20,2400,100,peak,0,0,0,0,0,0,0,500,2,5000,0,0,0,0,0,0,0)
                        for cid,peak in ((20,4800),(30,7200),(40,9600)))
    require(sym['g_force_servo_candidates']==candidates,'Unexpected candidate catalog: disabled/time/cooling/authorization configuration')
    cfg=sym['g_sd700_auto_target_machine_config']
    require(len(cfg)==76 and struct.unpack_from('<17I',cfg)==(275,20,5,10,200,50,250,10000,20,50,10000,10,40,5000,10,40,30000),'Firmware identity/configuration assertion failed')
    require(cfg[68:70]==b'\1\1' and struct.unpack_from('<I',cfg,72)[0]==325,'Firmware identity/configuration assertion failed')
    # Exact reviewed commissioning release: literal true, never a runtime unlock.
    require(sym['MotorHwReal_OutputArmingAllowed']==bytes.fromhex('01207047'),'Physical output is not commissioning armed')
    require(sym['g_force_build_config']==struct.pack('<38I',4,5000,10,100,8000,800,3000,300,2000,5000,
            10,11,11,50,3,400,1000,1000,30,12000,108000,5000,2,1,2,25,
            10,1,200,500,3500,8500,40000000,300,200,600,100,2),'Build configuration assertion failed')
    # This candidate replaces continuous owner admission with an independent
    # accounted segment contract; require its actual implementation and cutoff.
    for name in ('ForceServo_Prepare','ForceServo_Commit','MotorStopTimer_Arm','MotorStopTimer_CommitArm','TIM5_IRQHandler',
                 'Machine_HandlePressureSample','ForceServoProtocol_Write','ForceServoProtocol_Read',
                 'MotorExecutor_GuardOutput','MotorHwReal_MatchesPlan','ForceServo_TargetAllowed',
                 'ForceServo_Measure','ForceServo_PlanAllowed','ForceServo_CharacterizationPlanValid','ForceServo_CharacterizationDigest',
                 'ForceBuild_Select','ForceBuild_PulseBoost','ForceBuild_CoarseBoost','ForceBuild_ConfigDigest','ForceBuildMachine_Pressure','ForceBuildMachine_Safety',
                 'MotorExecutor_BeginBuild','MotorExecutor_StartBuildSegment','MotorExecutor_EndBuildSegment',
                 'MotorExecutor_AcceptBuildPost','MotorExecutor_GetBuildSnapshot','MotorStopTimer_BuildHandoff','MotorStopTimer_BuildPulse'):
        require(name in sym and len(sym[name])>0,'Missing active implementation: '+name)
    require(sym['MotorExecutor_BeginContinuous']==bytes.fromhex('01207047'),
            'Continuous owner admission must return MOTOR_RESULT_INVALID unconditionally')
    require('Machine_ConsumePressFeedback' not in sym,'Legacy boost owner linked in ForceServo build')


def verify(objcopy_cross_check=False):
    entries={}
    for line in (ROOT/'Firmware/ForceServo1.SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split(' *',1)
        require(name not in entries,'Duplicate firmware manifest entry')
        entries[name]=digest
    expected={f'../{FW_RELATIVE}/{STEM}.{ext}':digest for ext,digest in PINNED_HASHES.items()}
    require(entries==expected,'Exact pinned commissioning current pair required')
    for name,digest in entries.items():
        require(sha(ROOT/'Firmware'/name)==digest,'Hash mismatch: '+name)
    elf=FW/(STEM+'.elf')
    image=verify_contents(elf.read_bytes(),(FW/(STEM+'.hex')).read_bytes())
    if objcopy_cross_check:
        # Developer-only redundant byte-format cross-check; never required in field.
        import shutil, tempfile
        executable=shutil.which('arm-none-eabi-objcopy')
        require(executable is not None,'Developer cross-check requires arm-none-eabi-objcopy')
        with tempfile.TemporaryDirectory() as d:
            converted=Path(d)/'converted.hex'
            subprocess.run([executable,'-O','ihex',str(elf),str(converted)],check=True)
            require(converted.read_bytes()==(FW/(STEM+'.hex')).read_bytes(),'objcopy HEX/ELF mismatch')
    result=dict(configuration='PASS',candidate='BuildToTarget2_Pulse10msFix1',schema='F10C',build_id='46530111',physical_output='COMMISSIONING_ARMED_WITH_EXISTING_GUARD',
                runtime_target_N=[1,3000],reserved_assist_and_continuous_percent=[0,0],
                progress_anchor='FIRST_VALID_FRESH_FRAME_OF_ACCEPTED_NEW_START',
                start_resets_no_response_or_exposure=False,cooling_anchor='VERIFIED_OFF_AFTER_FAILED_PRELOAD_TRANSITION',interpulse_output='MICRO_FINE_FORWARD_PRELOAD_200_TO_600_BOUNDED_BY_RECEIVE_LEASE_AND_TOTAL_ON',
                command_per_percent=240,maximum_segment_command=8500,continuous_output_available=False,release_output_available=False,
                boot_press_cap=0,boot_assist_command=0,boot_target_valid=False,atomic_plan_required=True,one_START_per_plan=True,
                fixed_kp_ki_kd=[10,0,0],measurement_filter_s=0,
                build_profile=dict(approach_base_command=5000,coarse_boost_step=500,coarse_boost_max=3500,
                    coarse_check_ms=200,approach_ceiling=8500,approach_hard_ms=100,approach_total_ms=8000,
                    approach_command_ms_budget=40000000,micro_base=[800,3000],micro_boost_max=2000,build_ceiling=5000,
                    fine_base=[400,1000],fine_boost_max=1000,pulse_boost_step=300,
                    boost_reset='ABS_MOVEMENT_GE_1_N_RESETS_BOOST_AND_COUNT',
                    normal_pulse_ms=10,hard_pulse_ms=11,
                    post_pulse_cooldown_ms=30,preload_initial=300,preload_range=[200,600],preload_step=100,preload_droop='STRICTLY_GREATER_THAN_2_N',
                    preload_resets_on_START=False,preload_time_charged=True,total_on_reservation_ms=12000,uninterrupted_off_rest_ms=108000,
                    full_rest_required_after_boot=True,no_response_ms=5000,credible_net_progress_N=2),
                session_ms=0,build_ms=0,capture_ms=0,overall_time_limit='NONE',
                contact_N=10,zero_start='INDEPENDENT_BOUNDED_APPROACH_FOR_ERROR_GT_3_BEFORE_CONTACT',
                below3000='FIRST_VALID_REACH_OFF_MONITOR_ONLY_NO_HOLD',
                target3000='FIRST_VALID_REACH_OFF_MONITOR_ONLY_NO_HOLD',tracking_fault='DISABLED_DIAGNOSTIC_ONLY',
                sensor_unit_source='USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT',scale=1,offset=0,raw_trip_N=3000,qualification_bits=0,
                feedback_gap_ms=125,lease_ms=130,sample_age_ms=20,release_deadtime_ms=2,
                ratings='NOT_A_MOTOR_RATING; NOT_A_THERMAL_RATING; NOT_A_CONTINUOUS_RATING',
                load_bytes_compared=len(image.load_bytes),offline_verifier='PURE_PYTHON',
                hex_sha256=sha(FW/(STEM+'.hex')),elf_sha256=sha(elf),physical_test='NOT_RUN')
    print(json.dumps(result,indent=2)); return result

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--objcopy-cross-check',action='store_true',help='Optional developer-only ARM objcopy comparison')
    verify(parser.parse_args().objcopy_cross_check)
