"""ForceServo1 schema/metrics. All performance is observed, never hardware certification."""
import argparse, csv, json, math, re, struct, statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent

def schema():
    header=(ROOT/'Application/force_servo.h').read_text()
    # Current field profile only. The locked C profile retains its old defaults.
    profile=header.split('#if SD700_FORCE_SERVO_COMMISSIONING',1)[1].split('#else',1)[0]
    constants=dict(re.findall(r'#define (FS_\w+) ([0-9.]+)',profile))
    def number(value): return float(constants.get(value,value).rstrip('f'))
    params=[]
    for n,d,lo,hi in re.findall(r'X\((\w+),([^,]+),([^,]+),([^\)]+)\)',header):
        params.append(dict(name=n,default=number(d),minimum=number(lo),maximum=number(hi)))
    diag=(ROOT/'Application/force_servo_machine.h').read_text()
    u,f=diag.split('#define FORCE_SERVO_DIAG_FLOAT(X)',1)
    build_id=int(re.search(r'#define FORCE_SERVO_BUILD_ID (0x[0-9A-Fa-f]+)U',header)[1],16)
    schema_id=int(re.search(r'#define FORCE_SERVO_SCHEMA (0x[0-9A-Fa-f]+)U',header)[1],16)
    return dict(schema=schema_id,build_id=build_id,parameters=params,
                u32=re.findall(r'X\((\w+)\)',u),floats=re.findall(r'X\((\w+)\)',f.split('typedef struct')[0]))

def digest(config):
    h=2166136261
    for p in schema()['parameters']:
        for b in struct.pack('<f',config[p['name']]): h=((h^b)*16777619)&0xffffffff
    return h

def validate(config):
    s=schema(); assert set(config)=={p['name'] for p in s['parameters']},'Exact complete parameter set required'
    for p in s['parameters']:
        v=config[p['name']]
        assert isinstance(v,(int,float)) and math.isfinite(v) and p['minimum']<=v<=p['maximum'],p['name']
        if p['name'].endswith('_ms'): assert int(v)==v,p['name']
    c=config
    assert c['integral_min']<c['integral_max'] and c['hold_enter']<c['hold_exit']
    assert c['control_min_ms']<c['feedback_gap_ms']<c['lease_ms'] and c['sample_age_ms']<c['lease_ms']
    assert c['tracking_gain']*c['feedback_gap_ms']*.001<=1
    assert c['saturation_ms']<=c['session_ms'] and c['tracking_ms']<=c['session_ms']
    return digest(c)

def delta(a,b,bits=32): return (int(a)-int(b))&((1<<bits)-1)

def target250_summary(rows, valid):
    """Observed snapshots only. A nonzero command is not measured motion."""
    pre=[r for r in rows if r.get('phase') in ('PREFLIGHT','TARGET_READBACK')]
    all_runs=[r for r in rows if r.get('phase')=='RUN']
    origin=next((r['start_requested_ms'] for r in all_runs if 'start_requested_ms' in r),None)
    runs=[r for r in all_runs if not r.get('start_pending') and
          (origin is None or delta(r.get('latest_received_ms',r.get('received_ms',0)),origin)<(1<<31))]
    control=next((r for r in runs if r.get('state') in (13,14) and r.get('session') and r.get('lease_active')),None)
    commanded=next((r for r in valid if r.get('current_committed',0)!=0),None)
    peak=max((r.get('latest_raw',r['raw']) for r in runs),default=None)
    final=rows[-1].get('latest_raw') if rows else None
    reached=next((r for r in runs if r.get('latest_raw',r['raw'])>=250),None)
    ninety=next((r for r in runs if r.get('latest_raw',r['raw'])>=225),None)
    def elapsed(r, key='latest_received_ms'):
        return delta(r.get(key,r.get('received_ms',0)),origin) if r and origin is not None else None
    saturation=sum(bool(r.get('saturated',int(r['limits'])&1)) for r in valid)
    return dict(target_units=250,initial_pressure_units=pre[-1].get('latest_raw') if pre else None,
        initial_pressure_source=pre[-1]['phase'] if pre else 'UNAVAILABLE',
        time_to_control_start_ms=elapsed(control,'session_started_ms'),
        time_to_first_nonzero_command_ms=elapsed(commanded,'control_at_ms'),motion_start='NOT_MEASURED',
        time_to_90_ms=elapsed(ninety),time_to_first_250_ms=elapsed(reached),
        peak_pressure_units=peak,overshoot_units=max(0,peak-250) if peak is not None else None,
        peak_minus_target_units=peak-250 if peak is not None else None,
        final_pressure_units=final,final_error_units=250-final if final is not None else None,
        pre_stop_pressure_units=runs[-1].get('latest_raw',runs[-1]['raw']) if runs else None,
        saturated_sample_percent=100*saturation/len(valid) if valid else None,
        output_saturated_observed=bool(saturation) if valid else None,
        hold_entered=any(r['state']==14 for r in valid),
        hold_active_at_last_control=bool(valid and valid[-1]['state']==14 and valid[-1]['lease_active']),
        not_reached_assessment=('TARGET_REACHED' if reached else 'NO_CONTROL_DATA' if not valid else
            'AUTHORITY_OR_HARDWARE_LIMIT_POSSIBLE' if saturation else 'CONTROLLER_TUNING_REVIEW_REQUIRED'),
        assessment_limit='Saturation binds software authority; it does not prove an electrical/mechanical limit. Polling may miss peaks.')


def metrics(rows):
    """Conservative contiguous control-sample statistics; no interpolation over gaps."""
    valid=[]; previous=None; drops=duplicates=0
    for r in rows:
        if (not r.get('control_sequence') or r.get('phase')!='RUN' or r.get('fault') or
            r.get('start_pending') or r.get('state') not in (13,14) or not r.get('lease_active')): continue
        identity=(r['session'],r['config_version'],r['config_digest'])
        if previous and identity!=previous[0]: previous=None
        seq=int(r['control_sequence'])
        if previous:
            ds=delta(seq,previous[1])
            if ds==0: duplicates+=1; continue
            if ds>=1<<31: continue
            drops+=ds-1
        valid.append(r); previous=(identity,seq)
    result=dict(data_status='INSUFFICIENT_DATA',observed_control_points=len(valid),
                dropped_control_updates=drops,duplicate_polls=duplicates,
                control_coverage_fraction=len(valid)/(len(valid)+drops) if valid else None,
                contact_to_90_ms=None,contact_to_target_pm5_ms=None,observed_overshoot_units=None,
                observed_settling_ms=None,qualified_hold_ms=None,steady_error_units=None,
                observed_peak_units=None,observed_p2p_units=None,observed_saturation_ms=0,
                contact_lost_count=max((int(r.get('contact_lost_count',0)) for r in rows),default=0),
                hardware_stop_validation='NOT_VALIDATED',static_250_performance='NOT_VALIDATED',
                rotating_load_performance='NOT_TESTED',extension_500='NOT_TESTED',
                current='NOT_MEASURED',temperature='NOT_MEASURED')
    stops=[r for r in rows if r.get('phase')=='STOP_READBACK']
    result['StopVerified']=bool(stops and all(int(stops[-1].get(k,1))==0 for k in ('current_committed','tim2','tim3','lease_active'))
                               and int(stops[-1].get('output_off',0))==1 and int(stops[-1].get('start_pending',0))==0
                               and int(stops[-1].get('state',0)) in (1,9))
    result.update(target250_summary(rows,valid))
    # Register readback is software stop evidence only, independent scope/driver validation remains NOT_VALIDATED.
    if not valid: return result
    result['observed_peak_units']=max(r['raw'] for r in valid)
    result['observed_overshoot_units']=max(0,max(r['raw']-r['target'] for r in valid))
    for r in valid:
        if r['raw']>=.9*r['target'] and result['contact_to_90_ms'] is None:
            result['contact_to_90_ms']=delta(r['received_ms'],r['contact_at_ms'])
        if abs(r['raw']-r['target'])<=5 and result['contact_to_target_pm5_ms'] is None:
            result['contact_to_target_pm5_ms']=delta(r['received_ms'],r['contact_at_ms'])
    segments=[]; segment=[]
    for r in valid:
        healthy_hold=r['state']==14 and r['lease_active']==1 and abs(r['raw']-r['target'])<=5
        contiguous=bool(segment and r['session']==segment[-1]['session'] and
                        r['config_digest']==segment[-1]['config_digest'] and
                        delta(r['control_sequence'],segment[-1]['control_sequence'])==1 and
                        0<delta(r['received_ms'],segment[-1]['received_ms'])<=r['feedback_gap_ms'])
        if not healthy_hold or (segment and not contiguous):
            if len(segment)>=2: segments.append(segment)
            segment=[]
        if healthy_hold: segment.append(r)
    if len(segment)>=2: segments.append(segment)
    if segments:
        best=max(segments,key=lambda s:delta(s[-1]['received_ms'],s[0]['received_ms']))
        result['qualified_hold_ms']=delta(best[-1]['received_ms'],best[0]['received_ms'])
        result['steady_error_units']=statistics.mean(r['raw']-r['target'] for r in best)
        result['observed_p2p_units']=max(r['raw'] for r in best)-min(r['raw'] for r in best)
        # Only the terminal observed contiguous segment can be called observed settling.
        if best[-1] is valid[-1]: result['observed_settling_ms']=delta(best[0]['received_ms'],best[0]['contact_at_ms'])
    if len(valid)>=2 and drops==0 and segments: result['data_status']='OBSERVED_CONTIGUOUS_TRACKING_NOT_ACCEPTANCE'
    for a,b in zip(valid,valid[1:]):
        if (a['session']==b['session'] and a['config_digest']==b['config_digest'] and
            delta(b['control_sequence'],a['control_sequence'])==1 and a.get('saturated',int(a['limits'])&1) and
            delta(b['received_ms'],a['received_ms'])<=a['feedback_gap_ms']):
            result['observed_saturation_ms']+=delta(b['received_ms'],a['received_ms'])
    result['coverage_limit']='Snapshot polling can miss peaks. Gaps excluded; no full-bandwidth stability or settling claim.'
    return result

def decode_csv_row(row):
    # Config readback can fail before STOP diagnostics are collected. Preserve
    # the unavailable feedback budget as unknown; never invent a numeric value.
    return {k: (v if k in ('phase', 'firmware_sha256') else
                None if k == 'feedback_gap_ms' and v == '' else float(v))
            for k, v in row.items()}


def self_test():
    s=schema(); assert len(s['parameters'])==24 and len(s['u32'])==45 and len(s['floats'])==16
    c={p['name']:p['default'] for p in s['parameters']}; validate(c)
    bad=dict(c,lease_ms=10)
    try: validate(bad)
    except AssertionError: pass
    else: raise AssertionError('unsafe timing accepted')
    row=dict(control_sequence=1,session=1,config_version=1,config_digest=2,phase='RUN',fault=0,
             raw=250,target=250,received_ms=100,contact_at_ms=0,state=14,lease_active=1,
             feedback_gap_ms=40,limits=0)
    assert metrics([row])['data_status']=='INSUFFICIENT_DATA'
    assert metrics([row])['observed_p2p_units'] is None
    r2=dict(row,control_sequence=2,received_ms=110)
    assert metrics([row,r2])['qualified_hold_ms']==10
    assert metrics([row,r2])['observed_p2p_units']==0
    r3=dict(row,control_sequence=4,received_ms=130)
    m=metrics([row,r2,r3]); assert m['dropped_control_updates']==1 and m['data_status']=='INSUFFICIENT_DATA'
    assert metrics([row,dict(r2,state=7)])['qualified_hold_ms'] is None
    assert metrics([row,row,r2])['duplicate_polls']==1
    assert metrics([row,dict(r2,session=2)])['qualified_hold_ms'] is None
    assert metrics([row,dict(r2,received_ms=200)])['qualified_hold_ms'] is None
    assert decode_csv_row({'feedback_gap_ms':''})['feedback_gap_ms'] is None
    try: decode_csv_row({'raw':''})
    except ValueError: pass
    else: raise AssertionError('Missing pressure must not become a valid numeric measurement')
    print('FORCE_SERVO_DATA_TESTS=11 PASS SYNTHETIC; no serial I/O')

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--self-test',action='store_true')
    ap.add_argument('--schema',action='store_true'); ap.add_argument('--defaults',action='store_true')
    ap.add_argument('--validate'); ap.add_argument('--report'); ap.add_argument('--metadata')
    a=ap.parse_args()
    if a.self_test: self_test()
    elif a.schema: print(json.dumps(schema()))
    elif a.defaults: print(json.dumps({p['name']:p['default'] for p in schema()['parameters']},indent=2))
    elif a.validate: print(validate(json.loads(Path(a.validate).read_text(encoding='utf-8-sig'))))
    elif a.report:
        rows=[]
        for r in csv.DictReader(Path(a.report).open(encoding='utf-8-sig')):
            rows.append(decode_csv_row(r))
        result=metrics(rows); metadata=json.loads(Path(a.metadata).read_text(encoding='utf-8-sig'))
        result.update(metadata)
        result['binary_verification']='OPERATOR_ATTESTATION_ONLY_NOT_MCU_BINARY_VERIFICATION'
        text=json.dumps(result,indent=2,ensure_ascii=False)
        report=Path(a.report).with_suffix('.report.txt')
        with report.open('x',encoding='utf-8') as stream: stream.write(text+'\n')
        print(text)

if __name__=='__main__': main()
