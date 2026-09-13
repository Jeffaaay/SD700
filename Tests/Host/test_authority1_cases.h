/* User-selected experimental defaults, full production path with register shims. */
static void TestAuthority1Sweep(void)
{
 fixture();
 assert(machine.target_pressure_units==250 && machine.servo.config.kp==10);
 assert(machine.servo.config.ki==0 && machine.servo.config.kd==0);
 assert(machine.servo.config.press_cap==720 && machine.servo.config.release_cap==100);
 assert(machine.servo.config.reference_rate==200 && machine.servo.config.reference_acceleration==1000);
 assert(machine.servo.config.output_rate==1000 && machine.servo.config.saturation_ms==5000);
 continuous2_config(720,100,10); /* complete production parameter commit + FC03 readback */
 start250(22);
 for (int n=0;n<20;n++) sample(22,101);
 const int pressures[]={22,100,178,200,220,240,245,249,250,260,270};
 const int commands[]={720,720,720,500,300,100,50,10,0,-100,-100};
 for (unsigned n=0;n<sizeof(pressures)/sizeof(pressures[0]);n++) {
     sample(pressures[n],101); sample(pressures[n],101);
     const ForceServoDiagnostic *d=&machine.servo.diagnostic;
     int raw=10*(250-pressures[n]);
     assert(machine.state!=FAULT && d->reference==250 && d->p==raw && d->raw_output==raw);
     assert(d->i==0 && d->d==0 && d->post_limit_output==commands[n]);
     assert(d->requested_output==commands[n] && d->control_committed==commands[n] && d->current_committed==commands[n]);
     assert(((d->limits&FS_LIMIT_AMPLITUDE)!=0)==(raw!=commands[n]));
     assert(!(d->limits&(FS_LIMIT_RATE|FS_LIMIT_INTERLOCK)));
     assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
     if (commands[n]==720) assert(d->tim3==144 && TIM3->CCR3==144);
     if (pressures[n]>=245 && pressures[n]<=250) assert(machine.state==FORCE_HOLD && machine.servo.active);
     printf("AUTHORITY1_CALCULATED pressure=%d raw=%d committed=%d; SYNTHETIC_NOT_PHYSICAL\n",pressures[n],raw,commands[n]);
 }
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
}
static void TestAuthority1Ramp(void)
{
 fixture(); start250(250); sample(250,5);
 uint32_t ramp_at=now;
 for (int n=1;n<=144;n++) {
     sample(22,5); assert(machine.servo.diagnostic.current_committed==5*n);
 }
 assert(now-ramp_at==720 && TIM3->CCR3==144);
 sample(249,5); assert(machine.servo.diagnostic.current_committed==10); /* reduction bypasses rise slew */
 sample(22,5); assert(machine.servo.diagnostic.current_committed==15);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 for (int n=0;n<5;n++) { sample(22,101); off(); }
 uint32_t token=continuous(); int32_t actual; bool interlock;
 assert(update(token,720,&actual,&interlock)==MOTOR_RESULT_OK && actual==720);
 advance(10); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK && actual==0 && interlock); off();
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK && actual==0 && interlock); off();
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK && actual==-100);
 assert(TIM2->CCR3==20 && TIM3->CCR3==0);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
}
static void TestAuthority1Safety(void)
{
 for (int cause=0;cause<4;cause++) {
     fixture(); start250(22);
     for (int n=0;n<20;n++) sample(22,101);
     assert(machine.servo.diagnostic.current_committed==720 && TIM3->CCR3==144);
     if (cause==0) { sample(325,5); assert(machine.fault==FAULT_OVERPRESSURE); }
     if (cause==1) { advance(126); Machine_CheckPressureSafety(&machine,now); assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_TIMEOUT); }
     if (cause==2) { advance(129); off(); Machine_CheckPressureSafety(&machine,now); }
     if (cause==3) {
         while (machine.state!=FAULT && now<10000) sample(22,5);
         assert(machine.fault==FAULT_MOTION_TIMEOUT && machine.fault_detail==FAULT_DETAIL_SATURATION_TIMEOUT);
         assert(machine.servo.diagnostic.saturated_ms>=5000 && machine.servo.diagnostic.saturated_ms<5005);
     }
     off(); assert(machine.state==FAULT && !MotorStopTimer_IsArmed());
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
     sample(22,101); off(); assert(machine.state==FAULT);
     assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED);
     sample(22,101); off(); assert(machine.state==IDLE && !machine.servo.start_pending);
 }
}
