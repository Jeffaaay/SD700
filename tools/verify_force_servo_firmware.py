"""Exact StaticForce3000_1 hashes, ARM ELF data and bounded arming profile."""
import argparse, hashlib, json, struct, subprocess
from firmware_image import ElfImage, compare_images, require
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
STEM='SD700_ForceServo1_StaticForce3000_1_RealBench_Release'
FW_RELATIVE='output/StaticForce3000_1/firmware'
FW=ROOT/FW_RELATIVE
PINNED_HASHES={
    'hex':'39967AF2116B38A5FDB65C2D44AE83C04D5BA7EA3559932B09C64D2FCBE56AD9',
    'elf':'95B4C99A75929D10E26D2DBE4DA8A708089B1285D6C1B50032E99BB326460323',
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
    require(sym['g_force_servo_contract']==struct.pack('<16I',0xF104,0x46530106,1,275,325,5000,720,100,250,250,125,130,720,100,1,1),'Firmware identity/configuration assertion failed')
    defaults=(10,0,0,.02,200,1000,1000,720,100,-1000,1000,5,0,5,125,20,130,5,10,45000,5000,50,10000,2,500)
    require(sym['g_force_servo_default_config']==struct.pack('<25f',*defaults),'Unexpected default parameter group')
    profile=(1,0,0,0,325,1,0,0,0,275,325,0,20,720,100,0,0,0,0,5000,5000,5000,5000,500,2,5000)
    require(sym['g_force_servo_profile']==struct.pack('<26f',*profile),'Unexpected operating profile: units/calibration/force/current/time/boost configuration')
    cfg=sym['g_sd700_auto_target_machine_config']
    require(len(cfg)==76 and struct.unpack_from('<17I',cfg)==(275,20,5,10,200,50,250,10000,20,50,10000,10,40,5000,10,40,30000),'Firmware identity/configuration assertion failed')
    require(cfg[68:70]==b'\1\1' and struct.unpack_from('<I',cfg,72)[0]==325,'Firmware identity/configuration assertion failed')
    # Exact reviewed commissioning release: literal true, never a runtime unlock.
    require(sym['MotorHwReal_OutputArmingAllowed']==bytes.fromhex('01207047'),'Physical output is not commissioning armed')
    for name in ('ForceServo_Prepare','ForceServo_Commit','MotorExecutor_UpdateContinuous',
                 'MotorStopTimer_RenewLease','MotorStopTimer_ArmLease','TIM5_IRQHandler',
                 'Machine_HandlePressureSample','ForceServoProtocol_Write','ForceServoProtocol_Read',
                 'MotorExecutor_GuardOutput','MotorHwReal_MatchesPlan','MotorExecutor_SetContinuousBudget',
                 'MotorExecutor_ArmContinuousBoost','MotorExecutor_EndContinuousBoost','ForceServo_TargetAllowed',
                 'ForceServo_Measure','ForceServo_PlanAllowed'):
        require(name in sym and len(sym[name])>0,'Missing active implementation: '+name)
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
    result=dict(configuration='PASS',candidate='StaticForce3000_1',schema='F104',build_id='46530106',physical_output='COMMISSIONING_ARMED',
                target=250,start_wait_ms=250,press_profile_ceiling=720,release_profile_ceiling=100,press_operating_cap=720,release_operating_cap=100,
                powered_test_ready=True,qualification='SHORT_SUPERVISED_EXPERIMENT_NOT_CONTINUOUS_RATING',output_reduction='IMMEDIATE_MAGNITUDE_REDUCTION',lease_timeout_ms=130,sample_age_ms=20,feedback_gap_ms=125,
                build_budget_ms=5000,energized_budget_ms=5000,total_session_ms=5000,
                unit='LEGACY_CONTROL_UNITS_NOT_N',live_operating_max=275,raw_trip=325,boost_hardware_enabled=False,
                boost_disabled_reason='Missing sensor range/calibration/mechanical/current-and-duty qualifications',
                load_bytes_compared=len(image.load_bytes),offline_verifier='PURE_PYTHON',
                hex_sha256=sha(FW/(STEM+'.hex')),elf_sha256=sha(elf),physical_test='NOT_RUN')
    print(json.dumps(result,indent=2)); return result

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--objcopy-cross-check',action='store_true',help='Optional developer-only ARM objcopy comparison')
    verify(parser.parse_args().objcopy_cross_check)
