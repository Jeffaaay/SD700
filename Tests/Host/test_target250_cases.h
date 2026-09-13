/* Included by the existing production-path harness; no second controller. */
static void start250(int pressure)
{
 sample_at(pressure,++seq,now,true);
 assert(command(CMD_SET_TARGET,250)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 off(); assert(machine.servo.start_pending && !MotorStopTimer_IsArmed());
 sample(pressure,5); assert(machine.state==FORCE_BUILD && !machine.servo.start_pending);
}
static void TestTarget250StartFrame(void)
{
 fixture(); now=214345; sample_at(23,++seq,214321,true);
 assert(machine.target_pressure_units==250 && !Machine_IsPressureFresh(&machine,now));
 ModbusRtuServer server; ModbusRtuServer_Initialize(&server,&machine);
 request(&server,5,FS_COIL_START,0xFF00);
 assert(ModbusRtuServer_ProcessPending(&server,now));
 size_t length; const uint8_t *reply=ModbusRtuServer_GetResponse(&server,&length);
 assert(length==8 && reply[1]==5); /* exact field request now gets success echo */
 ModbusRtuServer_CompleteResponse(&server);
 off(); assert(machine.servo.start_pending && !machine.servo.active && machine.servo.session==0);
 assert(!MotorStopTimer_IsArmed());
 uint32_t at=machine.servo.start_requested_ms;
 advance(20); assert(command(CMD_FORCE_START,0)==COMMAND_BUSY);
 assert(machine.servo.start_requested_ms==at); off();
 sample_at(23,seq,now,true); assert(machine.servo.start_pending); off();
 sample(23,57); /* next frame at 214422, exactly 101 ms after the preceding RX */
 assert(machine.state==FORCE_BUILD && machine.servo.session==1);
 assert(machine.servo.controller.start==23 && machine.servo.controller.target==250);
 assert(machine.servo.diagnostic.control_sequence==0); off();
 uint32_t token=machine.servo.token;
 for (int i=0;i<20;i++) sample(23,101);
 assert(machine.servo.session==1 && machine.servo.token==token && TIM3->CCR3>0);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 for (int i=0;i<10;i++) { sample(23,101); off(); }
 assert(machine.state==IDLE && machine.servo.session==1);
}
static void TestTarget250PendingCancel(void)
{
 fixture(); advance(24); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 for (int i=0;i<10;i++) { sample(23,101); Machine_Tick(&machine,now); off(); }
 assert(machine.servo.session==0 && !machine.servo.start_pending);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 advance(249); Machine_Tick(&machine,now); off(); assert(machine.servo.start_pending);
 advance(1); sample_at(23,++seq,now,true); off(); /* timeout wins a boundary sample */
 assert(machine.state==FAULT && !machine.servo.start_pending && machine.servo.session==0);
 for (int i=0;i<5;i++) { sample(23,101); off(); }
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED);
 sample(23,101); off(); assert(machine.servo.session==0);
}
static void TestTarget250PendingValidation(void)
{
 fixture(); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 sample_at(23,++seq,now,false); off(); assert(machine.state==FAULT);
 sample(23,10); off(); assert(machine.servo.session==0);
 fixture(); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 advance(30); sample_at(23,++seq,now-21,true); off(); assert(machine.servo.start_pending);
 sample(23,101); assert(machine.servo.session==1); off();
 fixture(); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 sample(325,10); assert(machine.fault==FAULT_OVERPRESSURE); off();
 fixture(); start250(0); /* no old light-contact/approach admission gate */
 for (int i=0;i<15;i++) sample(0,101);
 assert(machine.state==FORCE_BUILD && TIM3->CCR3>0);
 assert(machine.servo.diagnostic.contact_count==0);
 assert(MotorExecutor_GetSnapshot()->last_action==MOTOR_ACTION_CONTINUOUS);
 fixture(); now=UINT32_MAX-10; machine.servo.have_sample=false;
 sample_at(23,seq,now-24,true); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 sample(23,30); assert(machine.servo.session==1); off();
}
static void TestTarget250BuildHold(void)
{
 fixture(); start250(23);
 for (int i=0;i<20;i++) sample(23,101);
 assert(machine.servo.diagnostic.reference==250 && machine.servo.diagnostic.current_committed==100);
 assert(machine.servo.diagnostic.limits&FS_LIMIT_AMPLITUDE);
 sample(210,101); assert(machine.servo.diagnostic.current_committed==40);
 sample(246,101); assert(machine.state==FORCE_HOLD && machine.servo.active);
 assert(machine.servo.diagnostic.current_committed==4 && MotorStopTimer_IsArmed());
 sample(247,101); assert(machine.state==FORCE_HOLD && machine.servo.diagnostic.current_committed==3);
 sample(260,101); off(); assert(machine.servo.diagnostic.limits&FS_LIMIT_INTERLOCK);
 sample(260,101); assert(machine.servo.diagnostic.current_committed==-10 && TIM2->CCR3>0);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 sample(247,101); off(); assert(machine.state==IDLE);
}
static void TestTarget250SmallIntegral(void)
{
 fixture(); machine.servo.config.ki=.2f; start250(249);
 for (int i=0;i<60;i++) sample(249,101);
 assert(machine.state==FORCE_HOLD && machine.servo.active);
 assert(machine.servo.diagnostic.p==1 && machine.servo.diagnostic.i>1);
 assert(machine.servo.diagnostic.current_committed>=2); /* sub-mV I survives rounding */
 assert(!(machine.servo.diagnostic.limits&FS_LIMIT_AMPLITUDE));
 float integral=machine.servo.controller.integral;
 sample(239,101); assert(machine.state==FORCE_BUILD);
 assert(machine.servo.controller.integral>integral); /* BUILD/HOLD does not reset I */
 sample(249,101); assert(machine.state==FORCE_HOLD && machine.servo.controller.integral>integral);
}
static void TestTarget250FieldCadence(void)
{
 fixture(); start250(23);
 for (int i=0;i<25;i++) {
     advance(101); sample_at(23,++seq,now-20,true);
     assert(machine.state==FORCE_BUILD);
 }
 assert(TIM3->CCR3>0);
 advance(106); sample_at(23,++seq,now,true); /* old RX gap126 cannot be hidden by late fresh data */
 assert(machine.state==FAULT); off();
 for (int i=0;i<5;i++) { sample(23,101); off(); }
 fixture(); start250(23); for (int i=0;i<20;i++) sample(23,101);
 advance(128); assert(TIM3->CCR3>0); advance(1); off(); /* independent TIM5: 130 -1 */
 Machine_CheckPressureSafety(&machine,now); assert(machine.state==FAULT);
 sample(23,101); off();
}
static void TestTarget250SyntheticPI(void)
{
 float final[2];
 for (int integral=0;integral<2;integral++) {
     fixture(); machine.servo.config.ki=integral ? .8f : 0; start250(23);
     float pressure=23; unsigned held=0;
     for (int i=0;i<1000;i++) {
         /* Small first-order force plant with load: equilibrium=20+5*u.
          * Synthetic only; no identification or hardware-performance claim. */
         pressure+=.02f/.5f*(20+5*machine.servo.diagnostic.current_committed-pressure);
         sample((int)roundf(pressure),20);
         assert(machine.state!=FAULT && pressure<325);
         assert(fabsf(machine.servo.diagnostic.current_committed)<=100);
         if (machine.state==FORCE_HOLD) held++;
     }
     final[integral]=pressure;
     if (integral) assert(held>100 && fabsf(pressure-250)<3);
     printf("TARGET250_SYNTHETIC ki=%.1f final=%.3f hold=%u; NOT_PHYSICAL\n",(double)machine.servo.config.ki,(double)pressure,held);
 }
 assert(final[0]<245 && final[1]>final[0]+20);
}
