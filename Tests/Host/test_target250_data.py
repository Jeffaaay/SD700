"""Target250 reports use observed data, preserve gaps and distinguish unknowns."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from force_servo_data import metrics

def sample(seq, raw, state=13, limits=0, **extra):
    return dict(phase='RUN',control_sequence=seq,session=1,config_version=1,config_digest=1,
        raw=raw,latest_raw=raw,latest_received_ms=100+seq*100,received_ms=100+seq*100,
        control_at_ms=100+seq*100,start_requested_ms=100,session_started_ms=150,
        contact_at_ms=150,target=250,state=state,fault=0,lease_active=1,start_pending=0,
        limits=limits,feedback_gap_ms=125,current_committed=extra.pop('current_committed',50),**extra)

class Target250DataTests(unittest.TestCase):
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
        self.assertEqual(m['time_to_first_250_ms'],300)
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

    def test_not_reached_saturated_vs_unsaturated(self):
        for flag,assessment in [(1,'AUTHORITY_OR_HARDWARE_LIMIT_POSSIBLE'),(0,'CONTROLLER_TUNING_REVIEW_REQUIRED')]:
            m=metrics([sample(1,30,limits=flag),sample(2,59,limits=flag)])
            self.assertIsNone(m['time_to_first_250_ms'])
            self.assertEqual(m['peak_pressure_units'],59)
            self.assertEqual(m['not_reached_assessment'],assessment)
            self.assertFalse(m['hold_entered'])

    def test_pending_previous_session_not_counted(self):
        row=sample(9,250,14,limits=1)
        row.update(start_pending=1,start_requested_ms=1100,latest_received_ms=1000,state=1,lease_active=0)
        m=metrics([row])
        self.assertIsNone(m['time_to_first_250_ms'])
        self.assertEqual(m['observed_control_points'],0)
        self.assertEqual(m['not_reached_assessment'],'NO_CONTROL_DATA')
        self.assertIsNone(m['peak_pressure_units'])

    def test_exact_cap_is_visible_even_without_clipping(self):
        rows=[sample(1,150,saturated=1,current_committed=100),sample(2,150,saturated=1,current_committed=100)]
        m=metrics(rows)
        self.assertEqual(m['saturated_sample_percent'],100)
        self.assertEqual(m['observed_saturation_ms'],100)
        self.assertEqual(m['not_reached_assessment'],'AUTHORITY_OR_HARDWARE_LIMIT_POSSIBLE')

    def test_gaps_not_interpolated(self):
        a,b=sample(1,249,14,1),sample(4,250,14,1)
        m=metrics([a,b]); self.assertIsNone(m['qualified_hold_ms'])
        self.assertEqual(m['observed_saturation_ms'],0)
        b['control_sequence']=2
        m=metrics([a,b]); self.assertEqual(m['observed_saturation_ms'],0)

    def test_lost_echo_no_run_no_motion_claim(self):
        m=metrics([dict(phase='PREFLIGHT',latest_raw=23)])
        self.assertIsNone(m['time_to_first_nonzero_command_ms'])
        self.assertIsNone(m['output_saturated_observed'])
        self.assertFalse(m['StopVerified'])

if __name__=='__main__': unittest.main(verbosity=2)
