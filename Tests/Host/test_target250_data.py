"""Target250 reports use observed data, preserve gaps and distinguish unknowns."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from force_servo_data import metrics, schema, validate

def sample(seq, raw, state=13, limits=0, **extra):
    return dict(phase='RUN',control_sequence=seq,session=1,config_version=1,config_digest=1,
        raw=raw,latest_raw=raw,latest_received_ms=100+seq*100,received_ms=100+seq*100,
        control_at_ms=100+seq*100,start_requested_ms=100,session_started_ms=150,
        contact_at_ms=150,target=250,state=state,fault=0,lease_active=1,start_pending=0,
        limits=limits,feedback_gap_ms=125,current_committed=extra.pop('current_committed',50),**extra)

class Target250DataTests(unittest.TestCase):
    def test_static_calibrated_metrics_do_not_compare_raw_to_newtons(self):
        a=sample(1,1400,measured=2810,unit=1,progress_status=1)
        b=sample(2,1494,measured=2998,unit=1,progress_status=3,state=14)
        c=sample(3,1495,measured=3000,unit=1,state=14)
        for r in (a,b,c): r['target']=3000
        m=metrics([a,b,c])
        self.assertEqual(m['unit'],'N')
        self.assertEqual(m['target_units'],3000)
        self.assertEqual(m['peak_pressure_units'],3000)
        self.assertEqual(m['observed_peak_units'],3000)
        self.assertEqual(m['final_error_units'],0)
        self.assertEqual(m['qualified_hold_ms'],100)
        self.assertEqual(m['observed_progress_statuses']['SATURATING_WITH_PROGRESS'],1)

    def test_static_selected_low_target_and_legacy_unit(self):
        r=sample(1,58); r['target']=60
        m=metrics([r])
        self.assertEqual(m['target_units'],60)
        self.assertEqual(m['final_error_units'],2)
        self.assertEqual(m['force_N_status'],'NOT_CALIBRATED_NO_N_MEASUREMENT')

    def test_static_profile_is_unqualified_and_finitely_bounded(self):
        p={f['name']:f['default'] for f in schema()['profile']}
        self.assertEqual((p['id'],p['unit'],p['qualifications']),(4,0,0))
        self.assertEqual((p['operating_max'],p['raw_trip']),(275,325))
        self.assertEqual((p['peak_press'],p['boost_ms'],p['boost_total_ms']),(0,0,0))
        self.assertEqual((p['energized_ms'],p['session_ms'],p['capture_ms']),(5000,5000,5000))

    def test_disabled_authority_catalog_is_not_live_authorization(self):
        s=schema()
        self.assertEqual((s['live_executor_press_ceiling'],s['live_continuous_ceiling'],s['live_approved_peak_ms']),(720,720,0))
        self.assertEqual([(p['id'],p['peak_press']) for p in s['candidates']],[(20,4800),(30,7200),(40,9600)])
        for p in s['candidates']:
            self.assertEqual((p['continuous_press'],p['release']),(2400,100))
            for name in ('experiment_enabled','limits_source','boost_ms','boost_total_ms','assist_rise_ms','assist_end_ms','off_ms','energized_ms','session_ms','build_ms','capture_ms'):
                self.assertEqual(p[name],0,name)
        c={p['name']:p['default'] for p in s['parameters']}
        for cap in (2400,4800,7200,9600):
            with self.assertRaises(AssertionError): validate(dict(c,press_cap=cap))

    def test_continuous2_peaks_and_exit(self):
        a=sample(1,22,limits=1,session_peak_raw=260,saturated_ms=4900)
        b=sample(3,249,limits=0,session_peak_raw=260)
        stop=dict(phase='STOP_READBACK',session=1,session_peak_raw=261,latest_raw=249,
                  current_committed=0,tim2=0,tim3=0,lease_active=0,output_off=1,state=1)
        m=metrics([dict(phase='PREFLIGHT',latest_raw=22),a,b,stop])
        self.assertEqual(m['peak_pressure_units'],249)
        self.assertEqual(m['peak_source'],'PC_SAMPLED_PEAK')
        self.assertEqual(m['mcu_session_peak_units'],261)
        self.assertEqual(m['mcu_peak_overshoot_units'],11)
        self.assertEqual(m['sampled_pressure_rise_units'],227)
        self.assertEqual(m['maximum_reported_saturation_ms'],4900)
        self.assertEqual(m['first_observed_amplitude_saturation_exit_ms'],300)
        self.assertEqual(m['observed_saturation_ms'],0) # no interpolation over missing control updates
        self.assertIsNone(metrics([dict(a,start_pending=1,state=1,lease_active=0)])['mcu_session_peak_units'])

    def test_direction_limits_from_authoritative_profile(self):
        s=schema(); c={p['name']:p['default'] for p in s['parameters']}
        self.assertTrue(s['powered_test_ready'])
        self.assertEqual((c['kp'],c['press_cap'],c['release_cap']),(10,720,100))
        self.assertEqual((c['ki'],c['kd']),(0,0))
        for name in ('press','release'):
            profile={p['name']:p['default'] for p in s['profile']}
            bound=profile['continuous_press' if name=='press' else 'release']; good=dict(c,**{name+'_cap':bound})
            validate(good)
            for v in (0,bound+1,float('nan')):
                with self.assertRaises(AssertionError): validate(dict(c,**{name+'_cap':v}))

    def test_boost_event_survives_unsampled_peak_and_stop(self):
        r=sample(1,27,boost_active=0,boost_peak_command=6000,boost_duration_ms=10,
                 boost_spent_ms=10,boost_started_ms=100,boost_end_ms=108,boost_elapsed_ms=8,
                 boost_deadline_ms=110,assist_admission=1,assist_exit=1,assist_requested_peak=6000,assist_peak_ccr=1200,
                 assist_rise_ms=3,assist_normal_end_ms=108,assist_pressure_peak=27,
                 boost_end_reason=2,boost_handoff_command=720,boost_pressure_before=27,
                 boost_before_received_ms=100,boost_after_valid=0,boost_pressure_after=0)
        r['phase']='STOP_READBACK'; r['current_committed']=0
        b=metrics([r])['boost_event']
        self.assertEqual((b['peak_command'],b['handoff_command'],b['elapsed_ms']),(6000,720,8))
        self.assertIsNone(b['pressure_after'])
        self.assertEqual((b['admission_reason'],b['exit_reason'],b['requested_peak'],b['maximum_committed_ccr']),(1,1,6000,1200))
        self.assertEqual((b['rise_ms'],b['planned_end_ms'],b['hard_deadline_ms'],b['peak_pressure']),(3,108,110,27))
        r.update(boost_after_valid=1,boost_pressure_after=28,boost_after_received_ms=201)
        b=metrics([r])['boost_event']
        self.assertEqual((b['pressure_before'],b['pressure_after'],b['after_received_ms']),(27,28,201))
        r['boost_end_reason']=3
        self.assertIsNone(metrics([r])['boost_event']['handoff_command'])
        r['boost_peak_command']=0
        b=metrics([r])['boost_event']
        self.assertEqual(b['reserved_total_ms'],10)
        self.assertIsNone(b['peak_command']) # reserved attempt without a recorded successful peak commit

    def test_full_curve(self):
        rows=[dict(phase='TARGET_READBACK',latest_raw=23)]
        rows += [sample(1,23,limits=1),sample(2,225,limits=1),sample(3,250,14),
                 sample(4,253,14),sample(5,250,14)]
        rows += [dict(phase='STOP_READBACK',latest_raw=249,current_committed=0,tim2=0,tim3=0,
                      lease_active=0,output_off=1,state=1,start_pending=0)]
        m=metrics(rows)
        self.assertEqual(m['initial_pressure_units'],23)
        self.assertEqual(m['time_to_control_start_ms'],50)
        self.assertEqual(m['time_to_first_nonzero_command_ms'],100)
        self.assertEqual(m['time_to_90_ms'],200)
        self.assertEqual(m['time_to_target_ms'],300)
        self.assertEqual(m['peak_pressure_units'],253)
        self.assertEqual(m['overshoot_units'],3)
        self.assertEqual(m['final_pressure_units'],249)
        self.assertEqual(m['final_error_units'],1)
        self.assertEqual(m['saturated_sample_percent'],40)
        self.assertEqual(m['observed_saturation_ms'],200)
        self.assertEqual(m['qualified_hold_ms'],200)
        self.assertEqual(m['observed_settling_ms'],250)
        self.assertTrue(m['hold_entered'] and m['hold_active_at_last_control'] and m['StopVerified'])
        self.assertEqual(m['motion_start'],'NOT_MEASURED')

    def test_post_assist_window_and_unchecked_late_observation(self):
        r=sample(1,37,boost_active=0,boost_duration_ms=12,boost_end_reason=2,
                 boost_pressure_before=27,boost_pressure_after=37,boost_after_valid=1,
                 boost_after_received_ms=201,boost_after_sample_hi=1,boost_after_sample_lo=2,
                 assist_pressure_peak=27,assist_response_peak=37,assist_after_result=2,
                 assist_response_pending=0,boost_handoff_command=89)
        b=metrics([r])['boost_event']
        self.assertEqual((b['peak_pressure'],b['response_pressure_peak'],b['pressure_after']),(27,37,37))
        self.assertIn('NOT_PWM_MEASUREMENT',b['peak_pressure_scope'])
        self.assertEqual((b['after_check_result'],b['response_pending'],b['after_sample_hi'],b['after_sample_lo']),(2,0,1,2))
        # STOP before response: late after data is observational, never a checked response.
        r.update(phase='STOP_READBACK',assist_after_result=0,assist_response_pending=1,assist_response_peak=27)
        b=metrics([r])['boost_event']
        self.assertEqual((b['after_check_result'],b['response_pending'],b['response_pressure_peak'],b['pressure_after']),(0,1,27,37))

    def test_not_reached_saturated_vs_unsaturated(self):
        for flag,assessment in [(1,'SATURATION_OBSERVED_NO_PHYSICAL_CAUSE_ESTABLISHED'),(0,'TARGET_NOT_REACHED_NO_PHYSICAL_CAUSE_ESTABLISHED')]:
            m=metrics([sample(1,30,limits=flag),sample(2,59,limits=flag)])
            self.assertIsNone(m['time_to_target_ms'])
            self.assertEqual(m['peak_pressure_units'],59)
            self.assertEqual(m['not_reached_assessment'],assessment)
            self.assertFalse(m['hold_entered'])

    def test_pending_previous_session_not_counted(self):
        row=sample(9,250,14,limits=1)
        row.update(start_pending=1,start_requested_ms=1100,latest_received_ms=1000,state=1,lease_active=0)
        m=metrics([row])
        self.assertIsNone(m['time_to_target_ms'])
        self.assertEqual(m['observed_control_points'],0)
        self.assertEqual(m['not_reached_assessment'],'NO_CONTROL_DATA')
        self.assertIsNone(m['peak_pressure_units'])

    def test_exact_cap_is_visible_even_without_clipping(self):
        rows=[sample(1,150,saturated=1,current_committed=100),sample(2,150,saturated=1,current_committed=100)]
        m=metrics(rows)
        self.assertEqual(m['saturated_sample_percent'],100)
        self.assertEqual(m['observed_saturation_ms'],100)
        self.assertEqual(m['not_reached_assessment'],'SATURATION_OBSERVED_NO_PHYSICAL_CAUSE_ESTABLISHED')

    def test_gaps_not_interpolated(self):
        a,b=sample(1,249,14,1),sample(4,250,14,1)
        m=metrics([a,b]); self.assertIsNone(m['qualified_hold_ms'])
        self.assertEqual(m['observed_saturation_ms'],0)
        b['control_sequence']=2
        m=metrics([a,b]); self.assertEqual(m['observed_saturation_ms'],0)

    def test_lost_echo_no_run_no_motion_claim(self):
        m=metrics([dict(phase='PREFLIGHT',latest_raw=23)])
        self.assertIsNone(m['target_units'])
        self.assertIsNone(m['final_error_units'])
        self.assertIsNone(m['time_to_first_nonzero_command_ms'])
        self.assertIsNone(m['output_saturated_observed'])
        self.assertFalse(m['StopVerified'])

    def test_invalid_calibrated_measurement_is_unknown_not_zero(self):
        a=sample(1,1000,unit=1,measured=2010,measured_valid=1)
        a['target']=3000
        stop=dict(a,phase='STOP_READBACK',measured=0,measured_valid=0,raw=1600,
                  latest_raw=1600,fault=4,current_committed=0,lease_active=0,output_off=1,tim2=0,tim3=0,state=9)
        m=metrics([a,stop])
        self.assertIsNone(m['final_pressure_units'])
        self.assertIsNone(m['final_error_units'])
        self.assertEqual(m['peak_pressure_units'],2010)

if __name__=='__main__': unittest.main(verbosity=2)
