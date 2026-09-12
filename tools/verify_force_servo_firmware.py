"""Exact candidate hashes, ARM ELF data, active control symbols and physical lock."""
import hashlib, json, struct, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
STEM='SD700_ForceServo1_RealBench_Locked_Release'
FW=ROOT/'output/ForceServo1/firmware'

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()

def symbols(path):
    data=path.read_bytes()
    assert data[:6]==b'\x7fELF\x01\x01' and struct.unpack_from('<H',data,18)[0]==40
    off=struct.unpack_from('<I',data,32)[0]; size,count=struct.unpack_from('<HH',data,46)
    sections=[struct.unpack_from('<10I',data,off+i*size) for i in range(count)]
    result={}
    for section in sections:
        if section[1]!=2: continue
        st=sections[section[6]]; strings=data[st[4]:st[4]+st[5]]
        for pos in range(section[4],section[4]+section[5],section[9]):
            n,v,length,info,other,idx=struct.unpack_from('<IIIBBH',data,pos)
            name=strings[n:strings.index(b'\0',n)].decode('ascii')
            if not length or not 0<idx<len(sections): continue
            sec=sections[idx]; at=sec[4]+(v&~1 if info&15==2 else v)-sec[3]
            if sec[1]!=8: result[name]=data[at:at+length]
    return result

def verify():
    entries={}
    for line in (ROOT/'Firmware/ForceServo1.SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split(' *',1); assert name not in entries
        entries[name]=digest
    expected={f'../output/ForceServo1/firmware/{STEM}.{ext}' for ext in ('elf','hex')}
    assert set(entries)==expected,'Exact locked current pair required'
    for name,digest in entries.items(): assert sha(ROOT/'Firmware'/name)==digest,'Hash mismatch: '+name
    elf=FW/(STEM+'.elf'); sym=symbols(elf)
    assert struct.unpack('<8I',sym['g_force_servo_contract'])==(0xF101,0x46530101,0,275,325,30000,5000,800)
    defaults=(1,0,0,.02,20,40,1000,250,100,-1000,1000,5,0,5,40,20,50,5,10,45000,5000,50,10000,2)
    assert sym['g_force_servo_default_config']==struct.pack('<24f',*defaults),'Unexpected default parameter group'
    cfg=sym['g_sd700_auto_target_machine_config']
    assert len(cfg)==76 and struct.unpack_from('<17I',cfg)==(275,20,5,10,200,50,250,10000,20,50,10000,10,40,5000,10,40,30000)
    assert cfg[68:70]==b'\1\1' and struct.unpack_from('<I',cfg,72)[0]==325
    # ARM Thumb release implementation must return literal false, with no runtime bypass.
    assert sym['MotorHwReal_OutputArmingAllowed']==bytes.fromhex('00207047'),'Physical output is not compiled locked'
    for name in ('ForceServo_Prepare','ForceServo_Commit','MotorExecutor_UpdateContinuous',
                 'MotorStopTimer_RenewLease','MotorStopTimer_ArmLease','TIM5_IRQHandler',
                 'Machine_HandlePressureSample','ForceServoProtocol_Write','ForceServoProtocol_Read'):
        assert name in sym and len(sym[name])>0,'Missing active implementation: '+name
    assert 'Machine_ConsumePressFeedback' not in sym,'Legacy boost owner linked in ForceServo build'
    # Confirm HEX was generated from this ELF; compare every byte, not just independent hashes.
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        converted=Path(d)/'converted.hex'
        subprocess.run(['arm-none-eabi-objcopy','-O','ihex',str(elf),str(converted)],check=True)
        assert converted.read_bytes()==(FW/(STEM+'.hex')).read_bytes(),'HEX/ELF mismatch'
    result=dict(configuration='PASS',schema='F101',physical_output='LOCKED',
                convergence_timeout_ms=30000,total_session_ms=45000,
                hex_sha256=sha(FW/(STEM+'.hex')),elf_sha256=sha(elf),physical_test='NOT_RUN')
    print(json.dumps(result,indent=2)); return result

if __name__=='__main__': verify()
