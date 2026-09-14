/* Current live configuration through production machine/executor/HW/TIM5 shims.
 * Synthetic pressure/registers only. No physical motion or waveform claim. */
static void boost1_fixture(void)
{
 fixture(); machine.servo.profile=g_force_servo_profile;
 Machine_Tick(&machine,now); start250(27);
 for (unsigned i=0;i<30 && !machine.servo.diagnostic.boost_active;i++) {
     sample(27,101); assert(machine.state==FORCE_BUILD);
 }
 assert(machine.servo.diagnostic.boost_active);
 assert(machine.servo.diagnostic.boost_peak_command==6000);
 assert(machine.servo.diagnostic.current_committed==6000);
 assert(TIM2->CCR3==0 && TIM3->CCR3==1200);
 assert(machine.servo.diagnostic.boost_spent_ms==10);
 assert(machine.servo.diagnostic.boost_duration_ms==10);
 assert(machine.servo.diagnostic.boost_pressure_before==27);
 assert(machine.servo.diagnostic.boost_before_received_ms==now);
 assert(!machine.servo.diagnostic.boost_after_valid);
 assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
}
static void TestBoost1PeakHandoff(void)
{
 boost1_fixture();
 uint32_t received=machine.pressure.received_at_ms,deadline=machine.servo.diagnostic.boost_deadline_ms;
 uint64_t sequence=machine.servo.last_control_sequence;
 uint32_t controls=machine.servo.diagnostic.control_sequence;
 int32_t lower=machine.servo.boost_handoff_request;
 assert(lower>0 && lower<=720 && deadline==now+10);
 advance(7); Machine_Tick(&machine,now);
 assert(machine.servo.diagnostic.boost_active && TIM3->CCR3==1200);
 advance(1); Machine_Tick(&machine,now);
 assert(machine.state==FORCE_BUILD && !machine.servo.diagnostic.boost_active);
 assert(machine.servo.diagnostic.boost_end_reason==2 && machine.servo.diagnostic.boost_elapsed_ms==8);
 assert(machine.servo.diagnostic.boost_handoff_command==lower);
 assert(machine.servo.diagnostic.current_committed==lower && TIM3->CCR3<=144);
 assert(machine.servo.last_control_sequence==sequence && machine.servo.diagnostic.control_sequence==controls);
 assert(machine.servo.controller.previous_committed==lower);
 assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==received+130); /* never now+130 */
 assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
 sample(28,93); /* next real frame at the observed 101 ms cadence */
 assert(machine.state==FORCE_BUILD && machine.servo.diagnostic.boost_after_valid);
 assert(machine.servo.diagnostic.boost_pressure_after==28);
 assert(machine.servo.diagnostic.boost_after_received_ms==received+101);
 for (int i=0;i<15;i++) { sample(28,101); assert(machine.state!=FAULT && TIM3->CCR3<=144); }
 assert(machine.servo.diagnostic.boost_spent_ms==10 && machine.servo.diagnostic.boost_peak_command==6000);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 assert(machine.servo.diagnostic.boost_end_reason==2); /* event survives STOP */
 boost1_fixture(); sample(249,5); /* early target taper still overrides */
 assert(machine.state!=FAULT && !machine.servo.diagnostic.boost_active);
 assert(machine.servo.diagnostic.boost_end_reason==2 && machine.servo.diagnostic.boost_elapsed_ms==5);
 assert(machine.servo.diagnostic.current_committed<=720);
}
static void boost1_pending_compare(void)
{ TIM5->CNT=TIM5->CCR1; TIM5->SR|=TIM_SR_CC1IF; }
static void boost1_expire_after_lower(void)
{
 if (TIM3->CCR3<=144) boost1_pending_compare();
 else dsb_hook=boost1_expire_after_lower;
}
static void TestBoost1Safety(void)
{
 for (int mode=0;mode<10;mode++) {
     boost1_fixture(); uint32_t token=machine.servo.token;
     if (mode==0) { advance(9); off(); assert(!MotorStopTimer_IsArmed()); Machine_Tick(&machine,now); }
     if (mode==1) { advance(3); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); }
     if (mode==2) sample(325,1);
     if (mode==3) sample_at(27,++seq,now,false);
     if (mode==4) { advance(8); dsb_hook=boost1_pending_compare; Machine_Tick(&machine,now); }
     if (mode==5) { TIM2->CCR3=1; Machine_Tick(&machine,now); }
     if (mode==6) { advance(8); Machine_Tick(&machine,now); advance(118); Machine_Tick(&machine,now);
         assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT); }
     if (mode==7) { advance(8); Machine_Tick(&machine,now); advance(121); off(); Machine_Tick(&machine,now); }
     if (mode==9) { advance(8); dsb_hook=boost1_expire_after_lower; Machine_Tick(&machine,now); }
     if (mode==8) { advance(1); sample_at(27,++seq,now-21,true); }
     off(); assert(machine.state==FAULT || (mode==1 && machine.state==IDLE));
     assert(machine.servo.diagnostic.boost_spent_ms==10);
     assert(MotorExecutor_HandoffContinuousBoost(token,now,720)!=MOTOR_RESULT_OK); off();
     for (int i=0;i<3;i++) { sample(27,101); Machine_Tick(&machine,now); off(); }
     if (machine.state==FAULT) {
         assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
         assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED); off();
     }
     sample(27,101); off(); assert(machine.state==IDLE && !machine.servo.active);
 }
 boost1_fixture(); uint32_t compare=TIM5->CCR1;
 int32_t actual; bool interlock;
 assert(MotorExecutor_UpdateContinuous(machine.servo.token,seq,now,now,130,20,2,6000,&actual,&interlock)==MOTOR_RESULT_INVALID);
 assert(TIM5->CCR1==compare && machine.servo.diagnostic.boost_spent_ms==10);
 assert(!MotorExecutor_ArmContinuousBoost(machine.servo.token,now,10));
 assert(MotorExecutor_HandoffContinuousBoost(machine.servo.token,now,721)!=MOTOR_RESULT_OK); off();
}
static void TestBoost1AdmissionAndBudget(void)
{
 fixture(); machine.servo.profile=g_force_servo_profile;
 assert(ForceServo_BoostQualified(&machine.servo.profile));
 assert(machine.servo.profile.id==3 && machine.servo.profile.qualifications==0);
 ForceServoConfig c=machine.servo.config; c.press_cap=721;
 stage(&c); assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE); off();
 ForceServoProfile p=g_force_servo_profile; p.boost_ms=11; assert(!ForceServo_ProfileValid(&p));
 p=g_force_servo_profile; p.peak_press=6001; assert(!ForceServo_ProfileValid(&p));
 p=g_force_servo_profile; p.boost_total_ms=20; assert(!ForceServo_ProfileValid(&p));
 for (int mode=0;mode<3;mode++) {
     fixture(); machine.servo.profile=g_force_servo_profile;
     if (mode==0) machine.servo.config.kp=2;
     if (mode==1) machine.servo.config.ki=.1f;
     start(mode==2 ? 60 : 250);
     for (int i=0;i<25;i++) { sample(30,101); assert(machine.state!=FAULT && TIM3->CCR3<=144); }
     assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.boost_spent_ms==0);
 }
 boost1_fixture(); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 start250(27);
 for (int i=0;i<25;i++) { sample(27,101); assert(machine.state!=FAULT && TIM3->CCR3<=144); }
 assert(machine.servo.diagnostic.boost_spent_ms==10 && !machine.servo.diagnostic.boost_active);
 /* Stronger existing cumulative reservation: even explicit STOP/new START cannot refund it. */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
#if !FS_SYNTHETIC_BOOST
 uint32_t token=continuous(); int32_t actual; bool interlock;
 assert(update(token,721,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 token=continuous(); assert(!MotorExecutor_SetContinuousBudget(token,now,5000,721));
 assert(MotorExecutor_SetContinuousBudget(token,now,5000,720));
 assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(!MotorExecutor_ArmContinuousBoost(token,now,11));
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
#endif
}
