"""ForceServo1 schema/metrics. All performance is observed, never hardware certification."""
import argparse, csv, json, math, re, struct, statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent

def schema(characterization=False):
    header=(ROOT/'Application/force_servo.h').read_text(encoding='utf-8')
    output_profile=(ROOT/'Application/force_servo_output_profile.h').read_text(encoding='utf-8')
    profile_header=(ROOT/'Application/force_servo_profile.h').read_text(encoding='utf-8')
    # Deliberately select the named C branches; no external compiler in the field.
    def defines(text): return dict(re.findall(r'#define (FS_\w+|FORCE_SERVO_SCHEMA|FORCE_SERVO_BUILD_ID) ([\w.]+)',text))
    constants=defines(header+'\n'+output_profile+'\n'+profile_header)
    identity=header.split('#if SD700_FORCE_CHARACTERIZATION',1)[1].split('#endif',1)[0].split('#else')
    constants.update(defines(identity[0 if characterization else 1]))
    constants.update(defines(header.split('#if SD700_FORCE_SERVO_COMMISSIONING',1)[1].split('#else',1)[0]))
    branches=output_profile.split('#if SD700_FORCE_CHARACTERIZATION',1)[1].split('#endif',1)[0]
    char,legacy=branches.split('#elif SD700_FORCE_SERVO_COMMISSIONING')
    constants.update(defines(char if characterization else legacy.split('#else',1)[0]))
    boost=output_profile.split('#define FS_PEAK_PRESS 0',1)[1].split('#endif',1)[0].split('#else')
    constants.update(defines(boost[0 if characterization else 1]))
    executor=output_profile.split('#if SD700_FORCE_CHARACTERIZATION')[-1]
    constants.update(defines(executor.split('#elif',1)[0] if characterization else executor.split('#else',1)[1].split('#endif',1)[0]))
    units=profile_header.split('#if SD700_FORCE_CHARACTERIZATION',1)[1].split('#endif',1)[0].split('#else')
    constants.update(defines(units[0 if characterization else 1]))
    def number(value):
        seen=set()
        while value in constants:
            if value in seen: raise ValueError('Cyclic profile constant: '+value)
            seen.add(value); value=constants[value]
        return float(value.rstrip('fU'))
    params=[]
    for n,d,lo,hi in re.findall(r'X\((\w+),([^,]+),([^,]+),([^\)]+)\)',header):
        params.append(dict(name=n,default=number(d),minimum=number(lo),maximum=number(hi)))
    diag=(ROOT/'Application/force_servo_machine.h').read_text()
    u,f=diag.split('#define FORCE_SERVO_DIAG_FLOAT(X)',1)
    build_id=int(constants['FORCE_SERVO_BUILD_ID'].rstrip('U'),16)
    schema_id=int(constants['FORCE_SERVO_SCHEMA'].rstrip('U'),16)
    profile_header=(ROOT/'Application/force_servo_profile.h').read_text(encoding='utf-8')
    profile_fields=[dict(name=n,default=number(d)) for n,d in re.findall(r'X\((\w+),([^,)]+)\)',profile_header.split('#define FORCE_SERVO_PROFILE_FIELDS(X)',1)[1].split('typedef struct',1)[0])]
    defaults={p['name']:p['default'] for p in profile_fields}
    candidate_line=profile_header.split('#define FORCE_SERVO_CANDIDATES(X)',1)[1].splitlines()[0]
    candidates=[]
    for identity,peak in re.findall(r'X\((\d+),(\d+)\)',candidate_line):
        candidate=dict(defaults,id=int(identity),unit=0,raw_trip=325,operating_max=275,force_trip=325,
            boost_ms=0,boost_total_ms=0,taper_margin=0,assist_rise_ms=0,assist_end_ms=0,off_ms=0,response_units=0,excessive_rise_units=0,
            continuous_press=2400,peak_press=int(peak),
            energized_ms=0,session_ms=0,capture_ms=0,build_ms=0,experiment_enabled=0,limits_source=0)
        candidates.append(candidate)
    return dict(characterization=characterization,profile=profile_fields,candidates=candidates,schema=schema_id,build_id=build_id,parameters=params,
                powered_test_ready=bool(number('FS_POWERED_TEST_READY')),
                press_profile_ceiling=number('FS_PRESS_PROFILE_CEILING'),
                live_executor_press_ceiling=number('FS_EXECUTOR_PRESS_CEILING'),
                live_continuous_ceiling=number('FS_CONTINUOUS_CEILING'),
                live_approved_peak_ms=number('FS_APPROVED_PEAK_MS'),
                release_profile_ceiling=number('FS_RELEASE_PROFILE_CEILING'),
                u32=re.findall(r'X\((\w+)\)',u),floats=re.findall(r'X\((\w+)\)',f.split('typedef struct')[0]))

def digest(config):
    h=2166136261
    for p in schema()['parameters']:
        for b in struct.pack('<f',config[p['name']]): h=((h^b)*16777619)&0xffffffff
    return h

def validate(config):
    def check(ok,reason='Invalid configuration relationship'):
        if not ok: raise AssertionError(reason)
    s=schema(); check(set(config)=={p['name'] for p in s['parameters']},'Exact complete parameter set required')
    for p in s['parameters']:
        v=config[p['name']]
        check(isinstance(v,(int,float)) and math.isfinite(v) and p['minimum']<=v<=p['maximum'],p['name'])
        if p['name'].endswith('_ms'): check(int(v)==v,p['name'])
    c=config
    check(c['integral_min']<c['integral_max'] and c['hold_enter']<c['hold_exit'])
    check(c['control_min_ms']<c['feedback_gap_ms']<c['lease_ms'] and c['sample_age_ms']<c['lease_ms'])
    check(c['tracking_gain']*c['feedback_gap_ms']*.001<=1)
    check(c['saturation_ms']<=c['session_ms'] and c['tracking_ms']<=c['session_ms'])
    profile={p['name']:p['default'] for p in s['profile']}
    check(c['press_cap']<=profile['continuous_press'] and c['release_cap']<=profile['release'],
          'Configuration exceeds continuous profile; peak is not writable continuous authority')
    return digest(c)

def delta(a,b,bits=32): return (int(a)-int(b))&((1<<bits)-1)

def target_summary(rows, valid):
    """Observed snapshots only. A nonzero command is not measured motion."""
    target=next((r.get('target') for r in reversed(rows) if r.get('target',0)>0),None)
    unit=next((r.get('unit',0) for r in reversed(rows)),0)
    def pressure(r): return r.get('measured',r.get('latest_raw',r.get('raw'))) if r.get('measured_valid',1) else None
    pre=[r for r in rows if r.get('phase') in ('PREFLIGHT','TARGET_READBACK')]
    all_runs=[r for r in rows if r.get('phase')=='RUN']
    origin=next((r['start_requested_ms'] for r in all_runs if 'start_requested_ms' in r),None)
    runs=[r for r in all_runs if not r.get('start_pending') and
          (origin is None or delta(r.get('latest_received_ms',r.get('received_ms',0)),origin)<(1<<31))]
    control=next((r for r in runs if r.get('state') in (13,14) and r.get('session') and r.get('lease_active')),None)
    commanded=next((r for r in valid if r.get('current_committed',0)!=0),None)
    peak=max((pressure(r) for r in runs if pressure(r) is not None),default=None)
    final=pressure(rows[-1]) if rows else None
    reached=next((r for r in runs if target is not None and pressure(r) is not None and pressure(r)>=target),None)
    ninety=next((r for r in runs if target is not None and pressure(r) is not None and pressure(r)>=target*.9),None)
    def elapsed(r, key='latest_received_ms'):
        return delta(r.get(key,r.get('received_ms',0)),origin) if r and origin is not None else None
    saturation=sum(bool(r.get('saturated',int(r['limits'])&1)) for r in valid)
    session=control.get('session') if control else None
    peaks=[r for r in rows if session is not None and r.get('session')==session and
           not r.get('start_pending') and 'session_peak_raw' in r]
    mcu_peak=max((r.get('session_peak_measured',r['session_peak_raw']) for r in peaks),default=None)
    left=None; previously_saturated=False
    for r in valid:
        saturated=bool(int(r['limits'])&1)
        if previously_saturated and not saturated and left is None: left=elapsed(r,'control_at_ms')
        previously_saturated |= saturated
    initial=pressure(pre[-1]) if pre else None
    return dict(target_units=target,unit='N' if unit in (1,2) else 'LEGACY_CONTROL_UNITS',initial_pressure_units=pressure(pre[-1]) if pre else None,
        initial_pressure_source=pre[-1]['phase'] if pre else 'UNAVAILABLE',
        time_to_control_start_ms=elapsed(control,'session_started_ms'),
        time_to_first_nonzero_command_ms=elapsed(commanded,'control_at_ms'),motion_start='NOT_MEASURED',
        time_to_90_ms=elapsed(ninety),time_to_target_ms=elapsed(reached),
        peak_pressure_units=peak,peak_source='PC_SAMPLED_PEAK',
        mcu_session_peak_units=mcu_peak,mcu_peak_source=('VALID_ORDERED_FRESH_IN_RANGE_SESSION_SAMPLES' if any('session_peak_measured' in r for r in peaks) else 'VALID_ORDERED_FRESH_SESSION_SAMPLES') if peaks else 'UNAVAILABLE',
        mcu_session_peak_raw_counts=max((r['session_peak_raw'] for r in peaks),default=None),
        mcu_peak_overshoot_units=max(0,mcu_peak-target) if mcu_peak is not None and target is not None else None,
        sampled_pressure_rise_units=peak-initial if peak is not None and initial is not None else None,
        first_observed_amplitude_saturation_exit_ms=left,
        maximum_reported_saturation_ms=max((r.get('saturated_ms',0) for r in runs),default=0),
        operator_reported_physical_motion='NOT_REPORTED; see field_notes',
        operator_reported_physical_stop='NOT_REPORTED; see field_notes',
        overshoot_units=max(0,peak-target) if peak is not None and target is not None else None,
        peak_minus_target_units=peak-target if peak is not None and target is not None else None,
        final_pressure_units=final,final_error_units=target-final if final is not None and target is not None else None,
        pre_stop_pressure_units=pressure(runs[-1]) if runs else None,
        saturated_sample_percent=100*saturation/len(valid) if valid else None,
        output_saturated_observed=bool(saturation) if valid else None,
        hold_entered=any(r['state']==14 for r in valid),
        hold_active_at_last_control=bool(valid and valid[-1]['state']==14 and valid[-1]['lease_active']),
        not_reached_assessment=('TARGET_REACHED' if reached else 'NO_CONTROL_DATA' if not valid else
            'SATURATION_OBSERVED_NO_PHYSICAL_CAUSE_ESTABLISHED' if saturation else 'TARGET_NOT_REACHED_NO_PHYSICAL_CAUSE_ESTABLISHED'),
        assessment_limit='Saturation binds software authority; it does not prove an electrical/mechanical limit. Polling may miss peaks.')


def metrics(rows):
    """Conservative contiguous control-sample statistics; no interpolation over gaps."""
    def measured(r): return r.get('measured',r['raw'])
    valid=[]; previous=None; drops=duplicates=0
    for r in rows:
        if (not r.get('measured_valid',1) or not r.get('control_sequence') or r.get('phase')!='RUN' or r.get('fault') or
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
    result.update(target_summary(rows,valid))
    statuses={0:'NO_BUILD_COMMAND',1:'SATURATING_WITH_PROGRESS',2:'COMMAND_WITHOUT_MEASURED_FORCE_RESPONSE',3:'COMMAND_WITH_PROGRESS'}
    result['observed_progress_statuses']={name:sum(r.get('progress_status')==code for r in valid) for code,name in statuses.items()}
    result['force_N_status']='USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT' if any(r.get('unit')==2 for r in rows) else 'CALIBRATED_PROFILE' if result['unit']=='N' else 'NOT_CALIBRATED_NO_N_MEASUREMENT'
    # A short event can fall entirely between PC polls. Use the persistent MCU
    # event fields, including STOP/fault rows; never interpolate pressure at its end.
    event=next((r for r in reversed(rows) if r.get('boost_duration_ms',0)>0),None)
    result['boost_event']=None if event is None else dict(
        session=event.get('session'), active=bool(event.get('boost_active')),
        peak_command=event.get('boost_peak_command') or None,
        configured_duration_ms=event.get('boost_duration_ms'), reserved_total_ms=event.get('boost_spent_ms'),
        started_ms=event.get('boost_started_ms'), end_ms=event.get('boost_end_ms'),
        hard_deadline_ms=event.get('boost_deadline_ms'),
        elapsed_ms=event.get('boost_elapsed_ms'), end_reason=event.get('boost_end_reason'),
        handoff_command=event.get('boost_handoff_command') if event.get('boost_end_reason')==2 else None,
        pressure_before=event.get('boost_pressure_before'), before_received_ms=event.get('boost_before_received_ms'),
        pressure_after=event.get('boost_pressure_after') if event.get('boost_after_valid') else None,
        after_received_ms=event.get('boost_after_received_ms') if event.get('boost_after_valid') else None,
        admission_reason=event.get('assist_admission'), exit_reason=event.get('assist_exit'),
        requested_peak=event.get('assist_requested_peak'), maximum_committed_ccr=event.get('assist_peak_ccr'),
        rise_ms=event.get('assist_rise_ms'), planned_end_ms=event.get('assist_normal_end_ms'),
        peak_pressure=event.get('assist_pressure_peak'), peak_pressure_scope='SAMPLES_WHILE_LOGICALLY_BOOST_ACTIVE_NOT_PWM_MEASUREMENT',
        response_pressure_peak=event.get('assist_response_peak'), response_pending=event.get('assist_response_pending'),
        after_check_result=event.get('assist_after_result'),
        after_sample_hi=event.get('boost_after_sample_hi') if event.get('boost_after_valid') else None,
        after_sample_lo=event.get('boost_after_sample_lo') if event.get('boost_after_valid') else None,
        measurement_limit='MCU command/register event, not measured waveform. After pressure is a fresh ordered in-range frame at/after event end with a later sequence; absent until valid. The response peak adds only the first evaluated post-handoff frame while the session remains active. Late STOP/fault telemetry does not evaluate or revive output. Abort elapsed is a capped service-time bound, not actual PWM duration.')
    result['progress_limit']='Bounded net-change diagnostic, not a stall or physical-root-cause diagnosis.'
    # tim2/tim3 are planned counts; output_off includes the hardware guard's register checks.
    # Neither is an externally measured waveform or physical-stop certificate.
    if not valid: return result
    result['observed_peak_units']=max(measured(r) for r in valid)
    result['observed_overshoot_units']=max(0,max(measured(r)-r['target'] for r in valid))
    for r in valid:
        if measured(r)>=.9*r['target'] and result['contact_to_90_ms'] is None:
            result['contact_to_90_ms']=delta(r['received_ms'],r['contact_at_ms'])
        if abs(measured(r)-r['target'])<=r.get('hold_enter_units',5) and result['contact_to_target_pm5_ms'] is None:
            result['contact_to_target_pm5_ms']=delta(r['received_ms'],r['contact_at_ms'])
    segments=[]; segment=[]
    for r in valid:
        healthy_hold=r['state']==14 and r['lease_active']==1 and abs(measured(r)-r['target'])<=r.get('hold_enter_units',5)
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
        result['steady_error_units']=statistics.mean(measured(r)-r['target'] for r in best)
        result['observed_p2p_units']=max(measured(r) for r in best)-min(measured(r) for r in best)
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

def characterization_metrics(rows, metadata):
    plan=metadata.get('runtime_plan')
    if not plan: return {}
    selected=[r for r in rows if r.get('plan_version')==plan['version'] and r.get('plan_digest')==plan['digest']]
    runs=[r for r in selected if r.get('phase') in ('RUN','STOP_READBACK') and r.get('session') and not r.get('start_pending')]
    final=runs[-1] if runs else {}
    holds=[r for r in runs if r.get('state')==14 and r.get('lease_active') and r.get('measured_valid') and not r.get('fault')]
    forces=[r['measured'] for r in holds]
    before=next((r for r in reversed(selected) if r.get('phase')=='TARGET_READBACK' and r.get('measured_valid')),None)
    response=next((r for r in runs if r.get('measured_valid') and before and r['measured']>=before['measured']+2),None)
    event=next((r for r in reversed(runs) if r.get('boost_duration_ms')),None)
    reason=int(final.get('run_reason',0))
    reasons={0:'NO_MCU_TERMINATION_RECORDED',1:'OPERATOR_STOP',2:'FAULT',3:'TARGET_NOT_REACHED_WITHIN_SESSION',
             4:'SESSION_COMPLETE',5:'BOUNDARY_TARGET_REACHED'}
    return dict(TargetForceN=plan['target_N'],AssistPercentRequested=plan['assist_percent'],
        AssistCommandRequested=plan['assist_command'],AssistCommandCommitted=event.get('boost_peak_command') if event else None,
        AssistPeakCCR=event.get('assist_peak_ccr') if event else None,
        ContinuousPercentRequested=plan['continuous_percent'],ContinuousCapCommand=plan['continuous_cap'],
        force_before_assist_N=event.get('boost_pressure_before') if event else None,
        first_fresh_force_after_assist_N=event.get('boost_pressure_after') if event and event.get('boost_after_valid') else None,
        assist_delta_N=event['boost_pressure_after']-event['boost_pressure_before'] if event and event.get('boost_after_valid') else None,
        target_reached=bool(final.get('target_reached')),
        time_to_target_ms=delta(final['target_reached_ms'],final['session_started_ms']) if final.get('target_reached') else None,
        time_to_observed_force_response_ms=delta(response['latest_received_ms'],response['session_started_ms']) if response else None,
        motion_start='NOT_MEASURED',session_timeout=reason in (3,4),mcu_stop_reason=reasons.get(reason,'UNKNOWN'),
        hold_sample_min_N=min(forces) if forces else None,hold_sample_max_N=max(forces) if forces else None,
        hold_sample_mean_N=statistics.mean(forces) if forces else None,
        hold_duration_mcu_ms=max((r.get('hold_ms',0) for r in runs),default=0),
        runtime_measurement_limit='Sampled HOLD statistics; polling gaps remain unknown. MCU command/CCR is not a measured waveform. PSU setting is not winding current.',
        fixed_envelope='SHORTEST_SOFTWARE_VALID_SUPERVISED_CHARACTERIZATION_ENVELOPE',
        ratings='NOT_A_MOTOR_RATING; NOT_A_THERMAL_RATING; NOT_A_CONTINUOUS_RATING')


def decode_csv_row(row):
    # Config readback can fail before STOP diagnostics are collected. Preserve
    # the unavailable feedback budget as unknown; never invent a numeric value.
    return {k: (v if k in ('phase', 'firmware_sha256') else
                None if k in ('feedback_gap_ms','hold_enter_units') and v == '' else float(v))
            for k, v in row.items()}


def self_test():
    s=schema(); assert len(s['parameters'])==25 and len(s['u32'])==86 and len(s['floats'])==27 and len(s['profile'])==33 and len(s['candidates'])==3
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
    ap.add_argument('--characterization-schema',action='store_true')
    ap.add_argument('--schema',action='store_true'); ap.add_argument('--defaults',action='store_true')
    ap.add_argument('--validate'); ap.add_argument('--report'); ap.add_argument('--metadata')
    a=ap.parse_args()
    if a.self_test: self_test()
    elif a.characterization_schema: print(json.dumps(schema(True)))
    elif a.schema: print(json.dumps(schema()))
    elif a.defaults: print(json.dumps({p['name']:p['default'] for p in schema()['parameters']},indent=2))
    elif a.validate: print(validate(json.loads(Path(a.validate).read_text(encoding='utf-8-sig'))))
    elif a.report:
        rows=[]
        for r in csv.DictReader(Path(a.report).open(encoding='utf-8-sig')):
            rows.append(decode_csv_row(r))
        result=metrics(rows); metadata=json.loads(Path(a.metadata).read_text(encoding='utf-8-sig'))
        result.update(metadata)
        result.update(characterization_metrics(rows,metadata))
        result['binary_verification']='OPERATOR_ATTESTATION_ONLY_NOT_MCU_BINARY_VERIFICATION'
        text=json.dumps(result,indent=2,ensure_ascii=False)
        report=Path(a.report).with_suffix('.report.txt')
        with report.open('x',encoding='utf-8') as stream: stream.write(text+'\n')
        print(text)

if __name__=='__main__': main()
