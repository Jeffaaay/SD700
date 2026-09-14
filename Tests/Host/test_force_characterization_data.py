"""Synthetic schema/report checks. No physical measurements are manufactured."""
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from force_servo_data import schema,metrics,characterization_metrics

class CharacterizationData(unittest.TestCase):
    def test_fixed_envelope_and_gain_schema(self):
        s=schema(True);p={x['name']:x['default'] for x in s['profile']};c={x['name']:x['default'] for x in s['parameters']}
        self.assertEqual((s['schema'],s['build_id'],len(s['u32']),len(s['floats'])),(0xF109,0x4653010B,86,27))
        self.assertEqual((p['unit'],p['qualifications'],p['scale'],p['offset']),(2,0,1,0))
        self.assertEqual([p[n] for n in ('assist_rise_ms','assist_end_ms','boost_ms','boost_total_ms','off_ms','taper_margin','response_units','excessive_rise_units')],[1,2,4,4,5000,30,2,25])
        self.assertEqual([c[n] for n in ('kp','ki','kd','measurement_filter_s','session_ms','tracking_ms','lease_ms','feedback_gap_ms','sample_age_ms')],[10,0,0,0,0,5000,130,125,20])
        self.assertEqual([p[n] for n in ('energized_ms','session_ms','build_ms','capture_ms')],[0,0,0,0])
        self.assertEqual(p['contact'],20)
        self.assertEqual((p['operating_max'],p['raw_trip'],p['force_trip']),(3000,3000,3000))
        self.assertEqual((s['live_executor_press_ceiling'],s['live_continuous_ceiling']),(9600,2400))
        self.assertTrue(all(not p['experiment_enabled'] for p in s['candidates']))
    def test_boundary_between_polls_is_persistent(self):
        row=dict(phase='STOP_READBACK',plan_version=1,plan_digest=2,session=1,state=1,unit=2,measured_valid=1,
                 measured=3000,raw=3000,target=3000,run_reason=5,target_reached=1,target_reached_ms=1200,session_started_ms=200,
                 output_off=1,current_committed=0,tim2=0,tim3=0,lease_active=0,start_pending=0)
        meta={'runtime_plan':dict(version=1,digest=2,target_N=3000,assist_percent=40,assist_command=9600,continuous_percent=10,continuous_cap=2400)}
        result=characterization_metrics([row],meta)
        self.assertEqual(result['mcu_stop_reason'],'BOUNDARY_TARGET_REACHED');self.assertTrue(result['target_reached'])
        self.assertEqual(result['time_to_target_ms'],1000);self.assertFalse(result['session_timeout'])
        self.assertEqual(result['hold_duration_mcu_ms'],0);self.assertIsNone(result['hold_sample_mean_N'])
        self.assertEqual(metrics([row])['force_N_status'],'USER_CONFIRMED_INSTALLED_SENSOR_OUTPUT_UNIT')
        self.assertTrue(metrics([row])['StopVerified'])
        self.assertEqual(characterization_metrics([dict(row,plan_version=0)],meta)['mcu_stop_reason'],'NO_MCU_TERMINATION_RECORDED')
    def test_historical_session_and_post_assist_measurement_provenance(self):
        row=dict(phase='STOP_READBACK',plan_version=2,plan_digest=3,session=1,run_reason=3,target_reached=0,
            boost_duration_ms=4,boost_peak_command=7200,assist_peak_ccr=1440,boost_pressure_before=30,
            boost_after_valid=1,boost_pressure_after=55,assist_after_result=2)
        meta={'runtime_plan':dict(version=2,digest=3,target_N=1000,assist_percent=30,assist_command=7200,continuous_percent=7.5,continuous_cap=1800)}
        result=characterization_metrics([row],meta)
        self.assertTrue(result['session_timeout']);self.assertFalse(result['target_reached'])
        self.assertEqual(result['mcu_stop_reason'],'TARGET_NOT_REACHED_WITHIN_SESSION')
        self.assertEqual((result['AssistCommandCommitted'],result['AssistPeakCCR'],result['assist_delta_N']),(7200,1440,25))
        self.assertIsNone(characterization_metrics([dict(row,boost_after_valid=0)],meta)['assist_delta_N'])
        self.assertFalse(characterization_metrics([dict(row,run_reason=1)],meta)['session_timeout'])
        self.assertEqual(result['motion_start'],'NOT_MEASURED')

    def test_hold_sample_statistics_remain_observational(self):
        plan=dict(version=1,digest=2,target_N=250,assist_percent=0,assist_command=0,continuous_percent=5,continuous_cap=1200)
        rows=[dict(phase='RUN',plan_version=1,plan_digest=2,session=1,state=14,lease_active=1,measured_valid=1,
                   measured=v,hold_ms=i*100,target_reached=0,run_reason=0) for i,v in enumerate((249,250,251))]
        result=characterization_metrics(rows,{'runtime_plan':plan})
        self.assertEqual((result['hold_sample_min_N'],result['hold_sample_max_N'],result['hold_sample_mean_N'],result['hold_duration_mcu_ms']),(249,251,250,200))
        self.assertIsNone(result['AssistCommandCommitted'])

    def test_operator_ended_hold_after_seventy_seconds_from_zero(self):
        plan=dict(version=1,digest=2,target_N=250,assist_percent=20,assist_command=4800,continuous_percent=5,continuous_cap=1200)
        rows=[dict(phase='PREFLIGHT',measured=0,measured_valid=1),
              dict(phase='RUN',plan_version=1,plan_digest=2,session=1,state=14,lease_active=1,measured_valid=1,
                   measured=250,hold_ms=70000,target_reached=1,target_reached_ms=10000,session_started_ms=0,run_reason=0),
              dict(phase='STOP_READBACK',plan_version=1,plan_digest=2,session=1,state=1,lease_active=0,measured_valid=1,
                   measured=250,hold_ms=70000,target_reached=1,target_reached_ms=10000,session_started_ms=0,run_reason=1)]
        result=characterization_metrics(rows,{'runtime_plan':plan,'maximum_observation_seconds':None})
        self.assertTrue(result['target_reached']);self.assertFalse(result['session_timeout'])
        self.assertEqual(result['mcu_stop_reason'],'OPERATOR_STOP')
        self.assertEqual(result['time_to_target_ms'],10000)
        self.assertEqual(result['hold_duration_mcu_ms'],70000)

if __name__=='__main__': unittest.main(verbosity=2)
