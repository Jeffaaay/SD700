/* SYNTHETIC calibration, load boundaries and current/time budgets below are
 * deliberately host-only fixtures, never ZD3000 ratings or live authorization. */
static void static_fixture(void)
{
 fixture(); machine.servo.profile=g_force_servo_profile;
 /* Historical non-boost envelope; current live boost is tested separately. */
 machine.servo.profile.id=1; machine.servo.profile.peak_press=0;
 machine.servo.profile.boost_ms=0; machine.servo.profile.boost_total_ms=0; machine.servo.profile.taper_margin=0;
 Machine_Tick(&machine,now);
}
static ForceServoProfile calibrated_fixture(void)
{
 ForceServoProfile p=g_force_servo_profile;
 p.peak_press=0; p.boost_ms=0; p.boost_total_ms=0; p.taper_margin=0;
 p.id=2; p.unit=1; p.qualifications=15;
 p.scale=2; p.offset=10; p.raw_trip=1600;
 p.calibration_min=10; p.calibration_max=3200;
 p.operating_max=3000; p.force_trip=3100; p.hardware_boundary=3200;
 p.contact=40; p.energized_ms=30000; p.session_ms=30000;
 p.capture_ms=30000; p.build_ms=28000;
 return p;
}
static void TestStaticUnitsAndQualification(void)
{
 static_fixture(); ForceServoProfile p=calibrated_fixture(); float m,t;
 assert(ForceServo_ProfileValid(&p));
 assert(ForceServo_Measure(&p,1000,7,&m) && m==2010); /* not control identity */
 assert(ForceServo_TargetAllowed(&p,3000,true)==FS_PROFILE_OK);
 assert(ForceServo_TargetAllowed(&p,3001,true)==FS_TARGET_OUTSIDE_OPERATING_RANGE);
 assert(ForceServo_TargetAllowed(&p,250,false)==FS_WRONG_TARGET_UNIT);
 assert(ForceServo_TargetAllowed(&g_force_servo_profile,250,true)==FS_MISSING_SENSOR_RANGE);
 const unsigned masks[]={14,13,11,7};
 const ForceServoRejection reasons[]={FS_MISSING_SENSOR_RANGE,FS_MISSING_CALIBRATION,
     FS_MISSING_MECHANICAL_LIMIT,FS_MISSING_CURRENT_TIME_LIMIT};
 for (unsigned i=0;i<4;i++) { p.qualifications=(float)masks[i]; assert(ForceServo_TargetAllowed(&p,250,true)==reasons[i]); }
 p=calibrated_fixture(); p.hardware_boundary=3000; assert(!ForceServo_ProfileValid(&p));
 p.force_trip=2950; p.operating_max=2900; assert(ForceServo_ProfileValid(&p));
 assert(ForceServo_TargetAllowed(&p,3000,true)==FS_TARGET_OUTSIDE_OPERATING_RANGE);
 p=calibrated_fixture(); p.calibration_min=100;
 assert(ForceServo_TargetAllowed(&p,60,true)==FS_TARGET_OUTSIDE_CALIBRATION);
 assert(!ForceServo_Measure(&p,30,30,&m));
 assert(write_reg(FS_REG_TARGET_N,3000)==COMMAND_INVALID_VALUE);
 assert(machine.servo.diagnostic.rejection==FS_MISSING_SENSOR_RANGE);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); off();
 machine.servo.profile=calibrated_fixture();
 assert(write_reg(FS_REG_TARGET_N,3000)==COMMAND_ACCEPTED);
 uint16_t word; assert(ForceServoProtocol_Read(&machine,true,FS_REG_TARGET_N,&word) && word==3000);
 assert(write_reg(FS_REG_PROFILE_SELECT,1)==COMMAND_INVALID_VALUE);
 assert(!machine.target_valid); off();
 assert(write_reg(FS_REG_PROFILE_SELECT,2)==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_TARGET_N,3000)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(1000,5);
 assert(machine.state==FORCE_BUILD && machine.servo.controller.start==2010 && machine.servo.controller.target==3000);
 sample(1200,10); assert(machine.state!=FAULT && machine.servo.diagnostic.force_N==2410);
 sample(1545,10); assert(machine.fault==FAULT_OVERPRESSURE); off(); /* 3100 N before raw trip1600 */
 static_fixture(); machine.servo.profile=calibrated_fixture();
 sample(1600,10); assert(machine.fault==FAULT_OVERPRESSURE); off();
 ForceServo s; ForceServoStep o; ForceServoConfig c=g_force_servo_default_config;
 assert(ForceServo_Init(&s,&c,22,3000));
 assert(fabsf(s.trajectory_s-22.335f)<.001f);
 for (int i=0;i<50;i++) { assert(ForceServo_Prepare(&s,&c,22,.1f,&o)); assert(ForceServo_Commit(&s,&c,&o,(int32_t)o.limited_output)); }
 assert(fabsf(o.reference-402.908f)<.01f);
 assert(ForceServo_PlanAllowed(&g_force_servo_profile,&c,22,250,&t)==FS_PROFILE_OK);
 c.reference_rate=20;
 assert(ForceServo_PlanAllowed(&g_force_servo_profile,&c,22,250,&t)==FS_TRAJECTORY_EXCEEDS_BUDGET);
}
static void TestStaticPlanAndAbsoluteBudget(void)
{
 static_fixture(); machine.servo.config.reference_rate=20;
 assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY);
 assert(machine.servo.diagnostic.rejection==FS_TRAJECTORY_EXCEEDS_BUDGET); off();
 static_fixture(); machine.servo.config.hold_dwell_ms=4900;
 assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
 static_fixture(); start250(22);
 assert(!MotorExecutor_SetContinuousBudget(machine.servo.token,now,5000,720));
 uint32_t began=machine.servo.session_started_ms;
 for (int i=0;i<49;i++) { sample(22+i,100); assert(machine.state==FORCE_BUILD); }
 assert(machine.servo.diagnostic.progress_status==1); /* cap with real synthetic progress */
 advance(99); off(); /* 5000-1, TIM5 only, NO next sample or main tick */
 assert(now-began==4999 && !MotorStopTimer_IsArmed());
 advance(1); Machine_CheckPressureSafety(&machine,now);
 assert(machine.fault_detail==FAULT_DETAIL_SESSION_TIMEOUT); off();
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 sample(100,100); off(); assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED);
 sample(100,100); off(); assert(machine.state==IDLE && !machine.servo.active);
}
static void TestStaticProgressNoiseAndCreep(void)
{
 for (int mode=0;mode<4;mode++) {
     static_fixture(); machine.servo.profile=calibrated_fixture();
     machine.servo.profile.no_response_ms=2000; machine.servo.config.saturation_ms=2000;
     if (mode==1) { machine.servo.profile.scale=.5f; machine.servo.profile.raw_trip=6500;
         machine.servo.profile.offset=10; }
     assert(write_reg(FS_REG_TARGET_N,3000)==COMMAND_ACCEPTED);
     assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(30,5);
     bool progress=false,saturation=false;
     for (int i=0;i<120 && machine.state!=FAULT;i++) {
         /* 0 steady; 1 bounded alternating noise (<2 N); 2 too-slow creep;
          * 3 persistent progress while clipped, beyond old five-second cutoff. */
         int raw=30+(mode==2 ? i/50 : mode==3 ? i : 0);
         if (mode==1) raw=120+i%2;
         sample(raw,100);
         if (machine.servo.diagnostic.progress_status==1) progress=true;
         if (machine.servo.diagnostic.limits&FS_LIMIT_AMPLITUDE) saturation=true;
     }
     if (mode<3) { assert(machine.state==FAULT && machine.fault_detail==FAULT_DETAIL_SATURATION_TIMEOUT);
         assert(machine.servo.diagnostic.progress_status==2); off(); }
     else { assert(machine.state==FORCE_BUILD && progress && saturation);
         assert(now-machine.servo.session_started_ms>5000); }
 }
}
static void TestStaticStopStagesAndReadback(void)
{
 for (int stage_id=0;stage_id<3;stage_id++) {
     static_fixture(); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
     if (stage_id>0) { sample(30,5); for (int i=0;i<20;i++) sample(30,100); }
     if (stage_id==2) { sample(247,100); assert(machine.state==FORCE_HOLD); sample(247,100); assert(machine.servo.diagnostic.hold_ms==100); }
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     for (int i=0;i<5;i++) { sample(30,100); Machine_Tick(&machine,now); off(); }
     assert(machine.state==IDLE && !machine.servo.active && !machine.servo.start_pending);
 }
 static_fixture();
 for (unsigned i=0;i<FORCE_SERVO_PROFILE_WORDS;i++) {
     uint16_t word; uint32_t bits;
     memcpy(&bits,(const unsigned char*)&machine.servo.profile+(i/2)*4,4);
     assert(ForceServoProtocol_Read(&machine,true,(uint16_t)(FS_REG_PROFILE+i),&word));
     assert(word==(uint16_t)(i%2 ? bits : bits>>16));
 }
 ForceServoConfig c=machine.servo.config; c.press_cap=100; c.kp=2;
 stage(&c); assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_ACCEPTED);
 assert(machine.servo.config_digest==ForceServo_ConfigDigest(&c));
 assert(command(CMD_SET_TARGET,60)==COMMAND_ACCEPTED); /* no Target250/exact-default restriction */
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); off();
 assert(write_reg(FS_REG_PROFILE_SELECT,1)==COMMAND_BUSY);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 assert(ForceServo_BoostQualified(&g_force_servo_profile));
}
#if FS_SYNTHETIC_BOOST
static void expire_during_boost_transfer(void)
{ TIM5->CNT=TIM5->CCR1; TIM5->SR|=TIM_SR_CC1IF; }
static void boost_fixture(void)
{
 static_fixture(); machine.servo.profile=calibrated_fixture();
 /* Deliberately synthetic 800 ms / 1600 ms session exposure, NOT actuator duty. */
 machine.servo.profile.peak_press=6000; machine.servo.profile.boost_ms=800;
 machine.servo.profile.boost_total_ms=1600; machine.servo.profile.taper_margin=80;
 machine.servo.config.kp=100; machine.servo.config.output_rate=10000; /* explicit synthetic fast-ramp fixture */
 assert(ForceServo_BoostQualified(&machine.servo.profile));
 assert(write_reg(FS_REG_TARGET_N,1000)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(30,5);
 while (!machine.servo.diagnostic.boost_active) { sample(30,10); assert(machine.state!=FAULT); }
}
static void TestStaticBoostDeadlineAndTransfer(void)
{
 boost_fixture();
 uint32_t deadline=machine.servo.diagnostic.boost_deadline_ms;
 while (deadline-now>100) { sample(30,10); assert(machine.servo.diagnostic.boost_deadline_ms==deadline); }
 assert(machine.servo.diagnostic.current_committed==6000 && TIM3->CCR3==1200);
 advance(deadline-now-1); off(); /* independent compare, no main/sensor/PC */
 assert(!MotorStopTimer_IsArmed()); Machine_CheckPressureSafety(&machine,now);
 assert(machine.state==FAULT && machine.servo.diagnostic.boost_spent_ms==800);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); sample(30,10); off();
 assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED);
 assert(machine.servo.diagnostic.boost_spent_ms==800); off();
 boost_fixture();
 for (int i=0;i<30;i++) sample(30,10);
 assert(machine.servo.diagnostic.current_committed>720);
 deadline=machine.servo.diagnostic.boost_deadline_ms;
 sample(460,10); /* calibrated930: adequate early taper before target1000 */
 assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.current_committed<=720);
 while (now<=deadline+100) { sample(494,10); assert(machine.state!=FAULT); }
 while (machine.state!=FORCE_HOLD) { sample(494,10); assert(machine.state!=FAULT); }
 assert(machine.servo.diagnostic.current_committed>=0 && machine.servo.diagnostic.current_committed<=720);
 assert(machine.servo.diagnostic.boost_spent_ms==800);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 assert(machine.servo.diagnostic.boost_spent_ms==800);
 boost_fixture(); for (int i=0;i<30;i++) sample(30,10);
 dsb_hook=expire_during_boost_transfer; sample(460,10);
 assert(machine.state==FAULT && !MotorStopTimer_IsArmed()); off();
 assert(machine.servo.diagnostic.boost_spent_ms==800);
 boost_fixture(); for (int i=0;i<30;i++) sample(30,10);
 sample_at(30,++seq,now,false); assert(machine.state==FAULT); off();
 assert(machine.servo.diagnostic.boost_spent_ms==800);
}
static void TestStaticBoostBudgetAndStop(void)
{
 boost_fixture();
 for (int cycle=0;cycle<2;cycle++) {
     assert(machine.servo.diagnostic.boost_active);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     uint32_t spent=machine.servo.diagnostic.boost_spent_ms;
     for (int i=0;i<5;i++) { sample(30,10); off(); }
     assert(machine.servo.diagnostic.boost_spent_ms==spent);
     assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(30,5);
     for (int i=0;i<150;i++) { sample(30,10); assert(machine.state!=FAULT);
         if (machine.servo.diagnostic.boost_active) break; }
 }
 assert(machine.servo.diagnostic.boost_spent_ms==1600 && !machine.servo.diagnostic.boost_active);
 assert(machine.servo.diagnostic.current_committed<=720); /* spent budget cannot restart peak */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
}
#endif
