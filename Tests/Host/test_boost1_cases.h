/* Current production path, synthetic pressure/registers. The high-authority
 * fixtures have invented test-only3/9/12 ms timing and1000 ms cooling, NOT a
 * proposed field duration or thermal rating. Prior Boost1 safety cases retained. */
static void TestAuthority2LiveEnvelope(void)
{
 fixture(); machine.servo.profile=g_force_servo_profile;
 assert(machine.servo.profile.id==4 && machine.servo.profile.peak_press==0);
 assert(machine.servo.profile.continuous_press==720 && machine.servo.profile.release==100);
 assert(machine.servo.profile.experiment_enabled==1 && !ForceServo_BoostQualified(&machine.servo.profile));
 for (unsigned i=0;i<FORCE_SERVO_CANDIDATE_COUNT;i++) {
     const ForceServoProfile *p=&g_force_servo_candidates[i]; float seconds=0;
     assert(ForceServo_ProfileValid(p) && !p->experiment_enabled && p->limits_source==0);
     assert(p->continuous_press==2400 && p->peak_press==(i==0 ? 4800 : i==1 ? 7200 : 9600));
     assert(p->boost_ms==0 && p->off_ms==0 && !ForceServo_BoostQualified(p));
     assert(ForceServo_PlanAllowed(p,&machine.servo.config,27,250,&seconds)==FS_EXPERIMENT_LIMITS_UNREVIEWED);
     assert(write_reg(FS_REG_PROFILE_SELECT,(uint16_t)p->id)==COMMAND_NOT_READY); off();
     assert(machine.servo.profile.id==4);
     for (unsigned j=0;j<FORCE_SERVO_PROFILE_WORDS;j++) {
         uint16_t word; uint32_t bits; memcpy(&bits,(const unsigned char*)p+(j/2)*4,4);
         assert(ForceServoProtocol_Read(&machine,true,(uint16_t)(0x400+i*FORCE_SERVO_PROFILE_WORDS+j),&word));
         assert(word==(uint16_t)(j%2 ? bits : bits>>16));
     }
 }
 ForceServoConfig c=machine.servo.config; c.press_cap=721; stage(&c);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE);
#if !FS_SYNTHETIC_BOOST
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,721,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 token=continuous(); assert(MotorExecutor_SetContinuousBudget(token,now,5000,720));
 assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(!MotorExecutor_ArmContinuousBoost(token,now,12));
 assert(!MotorExecutor_SetContinuousBoostPlan(token,now,3,9,4800,100));
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
#endif
}
#if FS_SYNTHETIC_BOOST
static void authority2_profile(unsigned index,float ki)
{
 fixture(); machine.servo.profile=g_force_servo_candidates[index];
 ForceServoProfile *p=&machine.servo.profile;
 p->experiment_enabled=1; p->limits_source=2;
 p->energized_ms=5000; p->session_ms=5000; p->capture_ms=5000; p->build_ms=5000;
 p->boost_ms=12; p->boost_total_ms=12; p->assist_rise_ms=3; p->assist_end_ms=9;
 p->off_ms=1000; p->taper_margin=20; p->response_units=2; p->excessive_rise_units=10;
 machine.servo.config.press_cap=2400; machine.servo.config.ki=ki;
 assert(ForceServo_ProfileConfigValid(p,&machine.servo.config));
 Machine_Tick(&machine,now);
}
static void authority2_admit(unsigned index,float ki)
{
 authority2_profile(index,ki); start250(27);
 for (unsigned i=0;i<30 && !machine.servo.diagnostic.boost_active;i++) {
     sample(27,101); assert(machine.state==FORCE_BUILD);
 }
 assert(machine.servo.diagnostic.boost_active && machine.servo.diagnostic.boost_spent_ms==12);
 assert(machine.servo.diagnostic.boost_before_received_ms==now);
 assert(machine.servo.diagnostic.assist_requested_peak==(uint32_t)machine.servo.profile.peak_press);
}
static void TestBoost1PeakHandoff(void)
{
 const unsigned commands[]={4800,7200,9600},ccrs[]={960,1440,1920};
 for (unsigned candidate=0;candidate<3;candidate++) {
     authority2_admit(candidate,0); uint32_t began=now,rx=machine.pressure.received_at_ms;
     uint32_t compare=TIM5->CCR1,controls=machine.servo.diagnostic.control_sequence;
     uint64_t sequence=machine.servo.last_control_sequence;
     int32_t lower=machine.servo.boost_handoff_request;
     float integral=machine.servo.controller.integral;
     uint32_t initial=MotorExecutor_GetSnapshot()->command_mv;
     assert(MotorExecutor_ContinuousBoostCommand(machine.servo.token,now-1)==(int32_t)initial);
     for (unsigned elapsed=1;elapsed<=9;elapsed++) {
         advance(1); Machine_Tick(&machine,now); assert(machine.state==FORCE_BUILD);
         if (elapsed<3) {
             assert(MotorExecutor_GetSnapshot()->command_mv==initial+(commands[candidate]-initial)*elapsed/3);
             assert(TIM5->CCR1==compare);
         } else if (elapsed<9) {
             assert(TIM3->CCR3==ccrs[candidate] && MotorExecutor_GetSnapshot()->command_mv==commands[candidate]);
             assert(TIM5->CCR1==compare);
         }
         assert(machine.servo.last_control_sequence==sequence && machine.servo.diagnostic.control_sequence==controls);
         assert(machine.servo.controller.integral==integral); /* no stale-frame integration */
     }
     assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.assist_exit==1);
     assert(machine.servo.diagnostic.boost_end_reason==2 && machine.servo.diagnostic.boost_elapsed_ms==9);
     assert(machine.servo.diagnostic.boost_handoff_command==lower && lower>0 && lower<=2400);
     assert(machine.servo.diagnostic.boost_peak_command==commands[candidate] && machine.servo.diagnostic.assist_peak_ccr==ccrs[candidate]);
     assert(TIM3->CCR3<=480 && MotorExecutor_GetSnapshot()->logical_deadline_ms==rx+130);
     assert(!machine.servo.diagnostic.boost_after_valid);
     sample(28,101-(now-began));
     assert(machine.state==FORCE_BUILD && machine.servo.diagnostic.boost_after_valid);
     assert(machine.servo.diagnostic.boost_pressure_after==28 && machine.servo.diagnostic.boost_after_received_ms==rx+101);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 }
 authority2_profile(0,0); machine.servo.profile.peak_press=0;
 machine.servo.config.kp=100; start250(27);
 for (int i=0;i<30;i++) sample(27,101);
 assert(machine.servo.diagnostic.current_committed==2400 && TIM3->CCR3==480);
 sample(249,101); assert(machine.servo.diagnostic.current_committed==100); /* ceiling, never minimum */
 sample(250,101); assert(machine.servo.diagnostic.current_committed==0);
}
static void pending_assist_compare(void) { TIM5->CNT=TIM5->CCR1; TIM5->SR|=TIM_SR_CC1IF; }
static void expire_after_assist_lower(void)
{ if (TIM3->CCR3<=480) pending_assist_compare(); else dsb_hook=expire_after_assist_lower; }
static void TestBoost1Safety(void)
{
 for (int mode=0;mode<10;mode++) {
     authority2_admit(2,0); advance(3); Machine_Tick(&machine,now); assert(TIM3->CCR3==1920);
     uint32_t token=machine.servo.token;
     if (mode==0) { advance(8); off(); assert(!MotorStopTimer_IsArmed()); Machine_Tick(&machine,now); }
     if (mode==1) assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);
     if (mode==2) sample(325,1);
     if (mode==3) sample_at(27,++seq,now,false);
     if (mode==4) { advance(6); dsb_hook=pending_assist_compare; Machine_Tick(&machine,now); }
     if (mode==5) { TIM2->CCR3=1; Machine_Tick(&machine,now); }
     if (mode==6) { advance(6); Machine_Tick(&machine,now); advance(117); Machine_Tick(&machine,now); assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT); }
     if (mode==7) { advance(6); Machine_Tick(&machine,now); advance(120); off(); Machine_Tick(&machine,now); }
     if (mode==8) { sample_at(27,seq-1,now,true); }
     if (mode==9) { advance(6); dsb_hook=expire_after_assist_lower; Machine_Tick(&machine,now); }
     off(); assert(machine.state==FAULT || (mode==1 && machine.state==IDLE));
     assert(machine.servo.diagnostic.boost_spent_ms==12);
     assert(MotorExecutor_HandoffContinuousBoost(token,now,100)!=MOTOR_RESULT_OK); off();
     for (int i=0;i<3;i++) { sample(27,101); Machine_Tick(&machine,now); off(); }
     if (machine.state==FAULT) { assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED); }
     sample(27,101); off(); assert(machine.state==IDLE && !machine.servo.active);
 }
 authority2_admit(0,0); uint32_t compare=TIM5->CCR1; int32_t actual; bool interlock;
 assert(MotorExecutor_UpdateContinuous(machine.servo.token,seq,now,now,130,20,2,4800,&actual,&interlock)==MOTOR_RESULT_INVALID);
 assert(TIM5->CCR1==compare && machine.servo.diagnostic.boost_spent_ms==12);
 assert(!MotorExecutor_ArmContinuousBoost(machine.servo.token,now,12));
 assert(MotorExecutor_HandoffContinuousBoost(machine.servo.token,now,2401)!=MOTOR_RESULT_OK); off();
 /* A newly available unsafe frame must not cause even a transient ramp write. */
 for (int mode=0;mode<2;mode++) {
     authority2_admit(2,0); uint32_t requests=MotorExecutor_GetSnapshot()->request_sequence;
     sample(mode==0 ? 325 : 37,3); off();
     assert(machine.state==FAULT && MotorExecutor_GetSnapshot()->request_sequence==requests);
 }
}
static void TestBoost1AdmissionAndBudget(void)
{
 for (int mode=0;mode<4;mode++) {
     authority2_profile(0,0);
     if (mode==0) machine.servo.config.kp=0;
     if (mode==1) start250(0); else if (mode==2) start250(245); else start250(27);
     if (mode==3) { machine.servo.diagnostic.boost_spent_ms=12; }
     for (int i=0;i<20;i++) { sample(mode==1 ? 0 : mode==2 ? 245 : 27,101); assert(machine.state!=FAULT); }
     assert(!machine.servo.diagnostic.boost_active && TIM3->CCR3<=480);
 }
 authority2_admit(0,0); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); assert(machine.servo.diagnostic.rejection==FS_COOLING_REQUIRED);
 sample(27,1000); start250(27);
 for (int i=0;i<20;i++) { sample(27,101); assert(machine.state!=FAULT && TIM3->CCR3<=480); }
 assert(machine.servo.diagnostic.boost_spent_ms==12 && !machine.servo.diagnostic.boost_active);
 authority2_profile(0,0); ForceServoProfile p=machine.servo.profile;
 p.assist_end_ms=11; assert(!ForceServo_ProfileValid(&p));
 p=machine.servo.profile; p.assist_rise_ms=9; assert(!ForceServo_ProfileValid(&p));
 p=machine.servo.profile; p.off_ms=0; assert(!ForceServo_ProfileValid(&p));
 p=machine.servo.profile; p.peak_press=9601; assert(!ForceServo_ProfileValid(&p));
 p=machine.servo.profile; p.boost_total_ms=11; assert(!ForceServo_ProfileValid(&p));
 p=machine.servo.profile; p.continuous_press=2401; assert(!ForceServo_ProfileValid(&p));
}
static void TestAuthority2PIAndExit(void)
{
 for (int mode=0;mode<3;mode++) {
     authority2_admit(1,.8f); if (mode==1) machine.servo.profile.excessive_rise_units=500;
     float integral=machine.servo.controller.integral;
     assert(fabsf(integral)<100); /* no peak-sized back-calculation into I */
     advance(3); Machine_Tick(&machine,now); assert(TIM3->CCR3==1440);
     assert(machine.servo.controller.integral==integral);
     sample(mode==0 ? 29 : mode==1 ? 245 : 37,1);
     assert(!machine.servo.diagnostic.boost_active);
     if (mode==2) { assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE); off(); }
     else { assert(machine.state!=FAULT && TIM3->CCR3<=480); assert(machine.servo.diagnostic.assist_exit==(mode==0 ? 2U : 3U)); }
 }
 authority2_admit(0,.8f); uint32_t initial=MotorExecutor_GetSnapshot()->command_mv;
 advance(9); Machine_Tick(&machine,now);
 assert(MotorExecutor_GetSnapshot()->command_mv<=initial); /* Missed rise cannot turn handoff into an increase. */
 sample(27,92);
 for (int i=0;i<15;i++) sample(27,101);
 float before=machine.servo.controller.integral;
 sample(249,101); assert(machine.state==FORCE_HOLD);
 assert(machine.servo.controller.integral!=0 && before!=0); /* BUILD->HOLD does not reset I */
 sample(249,101); assert(machine.servo.diagnostic.hold_ms==101);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 /* Target is not hardcoded: same finite path can admit at Target137. */
 authority2_profile(0,0); start(137);
 for (int i=0;i<10 && !machine.servo.diagnostic.boost_active;i++) sample(30,101);
 assert(machine.servo.diagnostic.boost_active);
 /* Fresh negative ordinary demand exits assist through the existing OFF interlock. */
 authority2_admit(0,.8f); machine.servo.controller.integral=-900;
 uint32_t requests=MotorExecutor_GetSnapshot()->request_sequence;
 sample(27,5); off();
 assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.assist_exit==3);
 assert(MotorExecutor_GetSnapshot()->request_sequence==requests+1);
 assert(machine.servo.diagnostic.limits&FS_LIMIT_INTERLOCK);
}
static void TestAuthority2WrapAndCooling(void)
{
 authority2_profile(2,0); now=UINT32_MAX-210; machine.pressure.received_at_ms=now; sample_at(27,++seq,now,true); start250(27);
 while (!machine.servo.diagnostic.boost_active) { sample(27,101); assert(machine.state!=FAULT); }
 uint32_t deadline=machine.servo.diagnostic.boost_deadline_ms;
 advance(3); Machine_Tick(&machine,now); assert(TIM3->CCR3==1920);
 advance(6); Machine_Tick(&machine,now); assert(machine.state!=FAULT && !machine.servo.diagnostic.boost_active);
 assert((uint32_t)(deadline-now)==3);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 uint32_t until=machine.servo.cooling_until_ms;
 ForceServoConfig c=machine.servo.config; c.kp=11; stage(&c); assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_ACCEPTED);
 assert(machine.servo.cooling_until_ms==until && machine.servo.diagnostic.boost_spent_ms==12);
 assert(write_reg(FS_REG_PROFILE_SELECT,40)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
 sample(27,999); assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
 sample(27,1); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); off();
 sample(27,5); assert(machine.state==FORCE_BUILD);
 for (int i=0;i<10;i++) { sample(27,101); assert(!machine.servo.diagnostic.boost_active && TIM3->CCR3<=480); }
}
#endif
