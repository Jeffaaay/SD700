"""Exact BuildToTarget3_SourcePort1 hashes, ARM ELF data and bounded arming profile."""
import argparse, hashlib, json, struct, subprocess
from firmware_image import ElfImage, compare_images, require
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
STEM='SD700_ForceServo1_BuildToTarget3_SourcePort1_RealBench_Release'
FW_RELATIVE='output/BuildToTarget3_SourcePort1/firmware'
FW=ROOT/FW_RELATIVE
PINNED_HASHES={
    'hex':'15CD52ECAB58E15B11EE77C23E097BDDC8AD94439C7B6448B2C069504DE8C683',
    'elf':'93CA20E52825A9D21BF6697E28A6D5091432BA87DD888341724D169A23D6BF42',
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
    require(sym['g_force_servo_contract']==struct.pack('<20I',0xF10D,0x46530113,1,3000,3000,0,12000,100,250,250,125,130,0,100,1,1,12000,0,4,3),'Firmware identity/configuration assertion failed')
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
    require(sym['g_force_build_config']==struct.pack('<45I',
            5,5000,10,8000,8000,800,3000,500,9000,12000,9,15,3,50,3,2000,12000,0,
            30,0,108000,45000,2,1,2,25,2,1,200,0,0,5000,40000000,0,0,0,0,0,
            10000,12,7000,8,400,1,10),'Build configuration assertion failed')
    # This candidate replaces continuous owner admission with an independent
    # accounted segment contract; require its actual implementation and cutoff.
    for name in ('ForceServo_Prepare','ForceServo_Commit','MotorStopTimer_Arm','MotorStopTimer_CommitArm','TIM5_IRQHandler',
                 'Machine_HandlePressureSample','ForceServoProtocol_Write','ForceServoProtocol_Read',
                 'MotorExecutor_GuardOutput','MotorHwReal_MatchesPlan','ForceServo_TargetAllowed',
                 'ForceServo_Measure','ForceServo_PlanAllowed','ForceServo_CharacterizationPlanValid','ForceServo_CharacterizationDigest',
                 'ForceBuildSource_Reset','ForceBuildSource_Step','MotorHwReal_BuildBrake','MotorHwReal_IsBraking','MotorHwReal_BuildFromBrake','MotorExecutor_RefreshBuildFeedback','ForceBuild_ConfigDigest','ForceBuildMachine_Pressure','ForceBuildMachine_Safety',
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
    result=dict(configuration='PASS',candidate='BuildToTarget3_SourcePort1',schema='F10D',build_id='46530113',physical_output='COMMISSIONING_ARMED_WITH_EXISTING_GUARD',
                runtime_target_N=[1,3000],reserved_assist_and_continuous_percent=[0,0],
                progress_anchor='FIRST_VALID_FRESH_FRAME_OF_ACCEPTED_NEW_START',
                start_resets_no_response_or_exposure=False,cooling_anchor='VERIFIED_OFF_AFTER_FAILED_PRELOAD_TRANSITION',interpulse_output='BRAKE_BOTH_DRIVERS_ENABLED_ZERO_COMPARE_WITH_FRESH_RECEIVE_LEASE',
                command_per_percent=240,maximum_segment_command=12000,continuous_output_available=False,release_output_available=False,
                boot_press_cap=0,boot_assist_command=0,boot_target_valid=False,atomic_plan_required=True,one_START_per_plan=True,
                fixed_kp_ki_kd=[10,0,0],measurement_filter_s=0,
                source_pressure_control_sha256='E6E08DC4E370A853EA9A586D765D97D5B6A1E7D6DDC9760202B947F99FB4D512',
                build_profile=dict(approach_command=5000,approach='CONTINUOUS_RECEIVE_LEASE_WITH_8000_MS_BOUND',
                    far_250_cap=10000,far_250_normal_ms=[9,12],precision_250_cap=7000,precision_250_normal_ms=[2,8],
                    stage_caps='ACTUAL_SOURCE_FUNCTIONS; GLOBAL12000_IS_NOT_THE_250_FAR_CAP',
                    pulse_hard_ms='SELECTED_NORMAL_PLUS_1',post_pulse_settle_ms=[30,400],
                    brake='BOTH_DRIVERS_ENABLED_TIM2_TIM3_ZERO; NO_FORWARD_PRELOAD',
                    no_response_ms=45000,net_progress_N=2,total_on_admission_limit_ms=0,
                    approach_reserved_ms=8000,approach_command_ms_budget=40000000,verified_off_cooling_ms=108000,
                    post_pulse_rise_policy='DIAGNOSTIC_ONLY_NOT_AN_INDEPENDENT_TRIP'),
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
