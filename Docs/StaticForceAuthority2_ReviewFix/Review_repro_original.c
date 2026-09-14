
/* Independent reviewer reproduction; host-only, no firmware/profile edits. */
#define main upstream_test_main
#include "Tests/Host/test_force_servo.c"
#undef main
int main(void) {
    for (int mode=0; mode<3; ++mode) {
        authority2_admit(0,0);
        uint32_t began=now;
        printf("CASE=%d start=%u pressure_before=%.0f rise_threshold=%.0f\n",mode,now,
            machine.servo.diagnostic.boost_pressure_before,machine.servo.profile.excessive_rise_units);
        advance(3); Machine_Tick(&machine,now);
        printf("peak command=%u ccr=%u active=%u\n",MotorExecutor_GetSnapshot()->command_mv,
            (unsigned)TIM3->CCR3,machine.servo.diagnostic.boost_active);
        if (mode==0) {
            sample(37,1); /* Fresh excessive rise while assist still active. */
        } else {
            advance(6); Machine_Tick(&machine,now);
            printf("handoff elapsed=%u command=%u ccr=%u active=%u exit=%u\n",now-began,
                MotorExecutor_GetSnapshot()->command_mv,(unsigned)TIM3->CCR3,
                machine.servo.diagnostic.boost_active,machine.servo.diagnostic.assist_exit);
            sample(mode==1 ? 37 : 325,92); /* Next sensor frame101 ms after assist admission. */
        }
        printf("post elapsed=%u measured=%u fault=%u detail=%u state=%u command=%u ccr=%u after_valid=%u after=%.0f assist_peak=%.0f\n",
            now-began,machine.pressure.raw_pressure_counts,(unsigned)machine.fault,
            (unsigned)machine.fault_detail,(unsigned)machine.state,MotorExecutor_GetSnapshot()->command_mv,
            (unsigned)TIM3->CCR3,machine.servo.diagnostic.boost_after_valid,
            machine.servo.diagnostic.boost_pressure_after,machine.servo.diagnostic.assist_pressure_peak);
        if (mode==0) { assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE); off(); }
        if (mode==1) {
            assert(machine.fault==FAULT_NONE); /* Reproduced missing post-assist excessive-rise check. */
            assert(machine.servo.diagnostic.boost_after_valid);
            assert(machine.servo.diagnostic.boost_pressure_after-machine.servo.diagnostic.boost_pressure_before>=machine.servo.profile.excessive_rise_units);
        }
        if (mode==2) { assert(machine.fault==FAULT_OVERPRESSURE); off(); }
    }
    puts("REPRODUCTION_CONFIRMED; synthetic ONLY; live assist DISABLED; hardware NOT_RUN");
    return 0;
}
