/* ReviewFix: scripted pressure is synthetic, not a physical response model. */
#if FS_SYNTHETIC_BOOST
static uint32_t post_assist_fixture(void)
{
 authority2_admit(0,0); uint32_t began=now,compare=TIM5->CCR1;
 advance(3); Machine_Tick(&machine,now);
 assert(TIM3->CCR3==960 && MotorExecutor_GetSnapshot()->command_mv==4800);
 assert(TIM5->CCR1==compare);
 advance(6); Machine_Tick(&machine,now);
 assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.assist_response_pending);
 assert(MotorExecutor_GetSnapshot()->command_mv==89 && TIM3->CCR3==18);
 assert(machine.servo.diagnostic.boost_handoff_command==89);
 assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==began+130);
 assert(machine.servo.last_control_rx_ms==began && machine.pressure.received_at_ms==began);
 assert(machine.servo.diagnostic.boost_deadline_ms==began+12);
 return began;
}
static unsigned post_dsb_checks;
static void post_no_increase(void)
{
 assert(TIM3->CCR3<=18 && TIM2->CCR3==0);
 post_dsb_checks++; dsb_hook=post_no_increase;
}
static void TestPostAssistExcessive(void)
{
 for (unsigned raw=37;raw<=38;raw++) {
     uint32_t began=post_assist_fixture();
     uint32_t requests=MotorExecutor_GetSnapshot()->request_sequence,controls=machine.servo.diagnostic.control_sequence;
     uint64_t previous=machine.servo.last_control_sequence;
     post_dsb_checks=0; dsb_hook=post_no_increase;
     sample((int)raw,92); dsb_hook=NULL; off();
     assert(post_dsb_checks && machine.fault==FAULT_MOTION_TIMEOUT && machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE);
     assert(MotorExecutor_GetSnapshot()->request_sequence==requests && machine.servo.diagnostic.control_sequence==controls);
     assert(machine.servo.last_control_sequence==previous && machine.servo.last_control_rx_ms==began);
     assert(machine.servo.diagnostic.boost_deadline_ms==began+12 && !MotorStopTimer_IsArmed());
     assert(!machine.servo.diagnostic.assist_response_pending && machine.servo.diagnostic.assist_after_result==2);
     assert(machine.servo.diagnostic.boost_after_valid && machine.servo.diagnostic.boost_pressure_after==raw);
     assert(machine.servo.diagnostic.boost_after_received_ms==began+101);
     assert(machine.servo.diagnostic.boost_after_sample_lo==(uint32_t)seq);
     assert(machine.servo.diagnostic.assist_pressure_peak==27 && machine.servo.diagnostic.assist_response_peak==raw);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); sample(40,10); off();
     assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE); /* first fault survives STOP/late telemetry */
 }
 authority2_admit(0,0); advance(3); Machine_Tick(&machine,now); sample(37,1);
 assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE); off();
 assert(machine.servo.diagnostic.assist_pressure_peak==37 && !machine.servo.diagnostic.assist_response_pending);
}
static void TestPostAssistNormalOnce(void)
{
 uint32_t began=post_assist_fixture(); sample(36,92);
 assert(machine.state==FORCE_BUILD && machine.fault==FAULT_NONE);
 assert(!machine.servo.diagnostic.assist_response_pending && machine.servo.diagnostic.assist_after_result==1);
 assert(machine.servo.diagnostic.assist_response_peak==36 && machine.servo.diagnostic.assist_pressure_peak==27);
 assert(machine.servo.last_control_rx_ms==began+101);
 assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==began+101+130); /* only accepted fresh control renews */
 uint32_t after=machine.servo.diagnostic.boost_after_received_ms;
 sample(37,10); sample(70,10);
 assert(machine.fault==FAULT_NONE && !machine.servo.diagnostic.boost_active);
 assert(machine.servo.diagnostic.assist_response_peak==36 && machine.servo.diagnostic.boost_pressure_after==36);
 assert(machine.servo.diagnostic.boost_after_received_ms==after && machine.servo.diagnostic.boost_spent_ms==12);
}
static void TestPostAssistRejectedFrames(void)
{
 for (int mode=0;mode<5;mode++) {
     uint32_t began=post_assist_fixture(),compare=TIM5->CCR1;
     uint32_t requests=MotorExecutor_GetSnapshot()->request_sequence;
     uint64_t control=machine.servo.last_control_sequence;
     advance(92);
     if (mode==0) sample_at(37,seq,now,true); /* duplicate */
     if (mode==1) sample_at(37,seq-1,now,true);
     if (mode==2) sample_at(37,++seq,now-21,true);
     if (mode==3) sample_at(37,++seq,now,false);
     if (mode==4) sample_at(37,++seq,began-1,true);
     assert(machine.servo.diagnostic.assist_response_pending && !machine.servo.diagnostic.assist_after_result);
     assert(!machine.servo.diagnostic.boost_after_valid && machine.servo.last_control_sequence==control);
     assert(MotorExecutor_GetSnapshot()->request_sequence==requests);
     if (mode==0) {
         assert(machine.state==FORCE_BUILD && TIM5->CCR1==compare);
         assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==began+130);
         sample_at(37,++seq,now,true); assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE); off();
     } else {
         assert(machine.state==FAULT); off(); FaultDetail first=machine.fault_detail;
         sample(37,1); off();
         assert(machine.fault_detail==first && machine.servo.diagnostic.assist_response_pending);
     }
 }
}
static void TestPostAssistFrameBoundaries(void)
{
 uint32_t began=post_assist_fixture(),compare=TIM5->CCR1;
 uint32_t controls=machine.servo.diagnostic.control_sequence,requests=MotorExecutor_GetSnapshot()->request_sequence;
 advance(1); sample_at(37,++seq,began+8,true); /* fresh delivery, but received before handoff */
 assert(machine.servo.diagnostic.assist_response_pending && !machine.servo.diagnostic.boost_after_valid);
 assert(TIM5->CCR1==compare && machine.servo.diagnostic.control_sequence==controls);
 assert(MotorExecutor_GetSnapshot()->request_sequence==requests);
 sample(37,91); off(); assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE);
 /* Early handoff frame is checked while active, not counted twice as after. */
 authority2_admit(0,0); advance(3); Machine_Tick(&machine,now); sample(29,3);
 assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.assist_response_pending);
 assert(!machine.servo.diagnostic.boost_after_valid && machine.servo.diagnostic.assist_pressure_peak==29);
 controls=machine.servo.diagnostic.control_sequence; compare=TIM5->CCR1;
 sample_at(37,seq,now,true); assert(machine.servo.diagnostic.assist_response_pending && TIM5->CCR1==compare);
 sample_at(37,++seq,now,true); /* new sequence, same millisecond: below control grid */
 off(); assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE);
 assert(machine.servo.diagnostic.control_sequence==controls && machine.servo.diagnostic.assist_after_result==2);
 assert(machine.servo.diagnostic.boost_after_received_ms==machine.servo.diagnostic.boost_end_ms);
}
static void TestPostAssistStopAndDeadlines(void)
{
 for (int mode=0;mode<6;mode++) {
     uint32_t began=post_assist_fixture();
     uint64_t control=machine.servo.last_control_sequence;
     if (mode==0) { assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); sample(37,92); assert(machine.state==IDLE); }
     if (mode==1) { Machine_ReportFault(&machine,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); sample(37,92); assert(machine.fault_detail==FAULT_DETAIL_NUMERIC); }
     if (mode==2) { sample(325,92); assert(machine.fault==FAULT_OVERPRESSURE); }
     if (mode==3) { sample(19,92); assert(machine.fault_detail==FAULT_DETAIL_CONTACT_LOST); }
     if (mode==4) { advance(117); Machine_Tick(&machine,now); assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_TIMEOUT); }
     if (mode==5) { advance(120); off(); Machine_Tick(&machine,now); assert(machine.state==FAULT); }
     off(); assert(machine.servo.last_control_sequence==control && machine.servo.diagnostic.boost_deadline_ms==began+12);
     assert(machine.servo.diagnostic.assist_response_pending && !machine.servo.diagnostic.assist_after_result);
     assert(machine.servo.diagnostic.boost_spent_ms==12);
     MachineState state=machine.state; FaultDetail first=machine.fault_detail;
     for (int i=0;i<3;i++) { sample(37,10); Machine_Tick(&machine,now); off(); }
     assert(machine.state==state && machine.fault_detail==first && !machine.servo.active);
 }
}
static void TestPostAssistWrap(void)
{
 authority2_profile(0,0); now=UINT32_MAX-210; machine.pressure.received_at_ms=now;
 start250(27);
 while (!machine.servo.diagnostic.boost_active) { sample(27,101); assert(machine.state==FORCE_BUILD); }
 uint32_t began=now;
 advance(3); Machine_Tick(&machine,now); advance(6); Machine_Tick(&machine,now);
 assert(now<began && machine.servo.diagnostic.assist_response_pending);
 /* Rebase only the synthetic sequence counters to exercise serial-number wrap. */
 seq=UINT64_MAX; machine.pressure.sequence=seq; machine.servo.assist_end_sequence=seq;
 sample(37,92); off();
 assert(machine.fault_detail==FAULT_DETAIL_ASSIST_EXCESSIVE_RISE);
 assert(machine.servo.diagnostic.boost_after_valid && machine.servo.diagnostic.boost_after_sample_hi==0 && machine.servo.diagnostic.boost_after_sample_lo==0);
 assert(machine.servo.diagnostic.boost_after_received_ms-began==101 && machine.servo.diagnostic.boost_deadline_ms-began==12);
}
#endif
