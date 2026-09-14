"""Synthetic schema/report invariants; not evidence of physical force or HOLD."""
from pathlib import Path
import sys, unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from force_servo_data import schema,build_to_target_metrics,select_build_branch

class BuildData(unittest.TestCase):
    def test_exact_schema_and_profile(self):
        s=schema(True,True); c={p['name']:p['default'] for p in s['parameters']}
        self.assertEqual((s['schema'],s['build_id'],len(s['u32']),len(s['floats'])),(0xF10A,0x4653010C,112,30))
        self.assertEqual(len(s['build_profile']),26); self.assertEqual(s['build_digest'],471320274)
        self.assertEqual((s['live_executor_press_ceiling'],s['live_continuous_ceiling']),(7000,0))
        self.assertEqual([c[k] for k in ('kp','ki','kd','press_cap','session_ms','lease_ms','feedback_gap_ms')],[10,0,0,0,0,130,125])
        self.assertEqual(len(set(s['u32']+s['floats'])),142)
        self.assertEqual([p['contact'] for p in s['candidates']],[20,20,20])
        self.assertEqual(s['build_profile']['total_on_ms'],12000)
        self.assertEqual(s['build_profile']['full_rest_ms'],108000)
    def test_historical_schema_selection(self):
        self.assertEqual(schema(True)['schema'],0xF109)
        self.assertEqual(len(schema()['u32']),86)
        with self.assertRaises(ValueError): schema(False,True)
        with self.assertRaises(ValueError): select_build_branch('#if SD700_BUILD_TO_TARGET\nx',True)
        text='#ifndef G\n#if SD700_BUILD_TO_TARGET\na\n#if OTHER\nb\n#endif\n#else\nc\n#endif\n#endif\n'
        self.assertEqual(select_build_branch(text,False),'#ifndef G\nc\n#endif\n')
        self.assertIn('a\n#if OTHER\nb\n#endif',select_build_branch(text,True))
    def fixture(self):
        meta=dict(build_profile=schema(True,True)['build_profile'],runtime_plan=dict(version=1,digest=2))
        row=dict(phase='RUN',session=1,plan_version=1,plan_digest=2,measured_valid=1,
                 target_reached=1,state=16,output_off=1,current_committed=0,tim2=0,tim3=0,lease_active=0,
                 measured=250,latest_received_ms=10000)
        return meta,row
    def test_reached_and_off_decay_are_not_hold(self):
        meta,row=self.fixture()
        result=build_to_target_metrics([row,dict(row,measured=235,latest_received_ms=70000)],meta)
        self.assertTrue(result['target_reached']);self.assertEqual(result['off_monitor_drop_N'],15)
        self.assertEqual(result['off_monitor_observed_ms'],60000)
        self.assertIn('NO_ACTIVE_HOLD',result['stable_hold'])
    def test_fault_output_and_stale_plan_excluded(self):
        meta,row=self.fixture()
        for change in ({'fault':2},{'output_off':0},{'tim3':1},{'lease_active':1},{'measured_valid':0},
                       {'plan_version':2},{'plan_digest':3},{'start_pending':1},{'state':14}):
            with self.subTest(change=change):
                r=build_to_target_metrics([dict(row,**change)],meta)
                self.assertEqual(r['off_monitor_samples'],0);self.assertIsNone(r['off_monitor_min_N'])
    def test_stop_persistent_target_and_paired_post_event(self):
        meta,row=self.fixture()
        row.update(phase='STOP_READBACK',post_pulse_valid=1,post_pulse_request=12,segment_request=13,
                   pulse_force_before=200,pulse_force_after=204,post_pulse_received_ms=9000)
        r=build_to_target_metrics([row],meta)
        self.assertTrue(r['target_reached']); self.assertEqual(r['last_post_pulse']['request'],12)
        self.assertEqual(r['last_post_pulse']['after_N']-r['last_post_pulse']['before_N'],4)
        self.assertEqual(r['off_monitor_samples'],0)

if __name__=='__main__': unittest.main(verbosity=2)
