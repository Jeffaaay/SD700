/* Production machine -> controller -> executor -> PWM register shim.
 * Larger host ceilings are arbitrary boundary fixtures, NEVER approved output. */
static void continuous2_config(float cap,float release,float kp)
{
 ForceServoConfig c=machine.servo.config;
 c.press_cap=cap; c.release_cap=release; c.kp=kp; c.ki=0; c.kd=0;
 stage(&c); assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_ACCEPTED);
 for (unsigned j=0;j<sizeof(c)/4;j++) {
     uint16_t hi,lo; uint32_t bits;
     assert(ForceServoProtocol_Read(&machine,true,(uint16_t)(FS_REG_CONFIG+2*j),&hi));
     assert(ForceServoProtocol_Read(&machine,true,(uint16_t)(FS_REG_CONFIG+2*j+1),&lo));
     memcpy(&bits,(const unsigned char*)&c+4*j,4);
     assert(bits==((uint32_t)hi<<16|lo));
 }
}
static void TestContinuous2Range(void)
{
 fixture(); continuous2_config(FS_PRESS_PROFILE_CEILING,FS_RELEASE_PROFILE_CEILING,1);
 ForceServoConfig old=machine.servo.config,c=old;
 c.press_cap=FS_PRESS_PROFILE_CEILING+1; stage(&c);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE);
 assert(memcmp(&old,&machine.servo.config,sizeof(c))==0);
 c=old; c.release_cap=FS_RELEASE_PROFILE_CEILING+1; stage(&c);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE);
 assert(memcmp(&old,&machine.servo.config,sizeof(c))==0);
 c=old; c.press_cap=0; assert(!ForceServo_ConfigValid(&c));
 c=old; c.release_cap=0; assert(!ForceServo_ConfigValid(&c));
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,FS_PRESS_PROFILE_CEILING,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==FS_PRESS_PROFILE_CEILING && TIM3->CCR3>0);
 advance(5); assert(update(token,FS_PRESS_PROFILE_CEILING-1,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==FS_PRESS_PROFILE_CEILING-1); /* no hidden 100 clamp */
 advance(5); assert(update(token,FS_PRESS_PROFILE_CEILING+1,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 assert(update(token,1,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 token=continuous(); assert(update(token,-FS_RELEASE_PROFILE_CEILING,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==-FS_RELEASE_PROFILE_CEILING && TIM2->CCR3>0);
 advance(5); assert(update(token,-FS_RELEASE_PROFILE_CEILING-1,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void TestContinuous2Sweep(void)
{
 const int pressure[]={22,100,180,220,240,245,249,250,260,324};
 float cap=fminf(FS_PRESS_PROFILE_CEILING,400), release=100, kp=cap/100;
 fixture(); continuous2_config(cap,release,kp); start250(22);
 for (int j=0;j<20;j++) sample(22,101);
 for (unsigned j=0;j<sizeof(pressure)/sizeof(pressure[0]);j++) {
     for (int n=0;n<5;n++) sample(pressure[j],101); /* settle slew and reversal */
     const ForceServoDiagnostic *d=&machine.servo.diagnostic;
     float p=kp*(250-pressure[j]), limited=fminf(cap,fmaxf(-release,p));
     assert(machine.state!=FAULT && d->reference==250 && d->p==p && d->i==0 && d->d==0);
     assert(d->raw_output==p && d->post_limit_output==limited);
     assert(d->requested_output==(int32_t)limited && d->current_committed==(int32_t)limited);
     assert(((d->limits&FS_LIMIT_AMPLITUDE)!=0)==(limited!=p));
     assert(!(d->limits&(FS_LIMIT_RATE|FS_LIMIT_INTERLOCK)));
     assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
     if (pressure[j]>=245 && pressure[j]<=250) assert(machine.state==FORCE_HOLD);
     printf("CONTINUOUS2_CALCULATED_HOST pressure=%d p=%.1f submitted=%.1f; SYNTHETIC\n",
            pressure[j],(double)p,(double)d->current_committed);
 }
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
}
static void TestContinuous2Ramp(void)
{
 fixture(); float cap=fminf(FS_PRESS_PROFILE_CEILING,400), kp=cap/100;
 continuous2_config(cap,100,kp); start250(22);
 for (int j=0;j<20;j++) sample(22,101);
 assert(machine.servo.diagnostic.current_committed==cap);
 sample(249,5); /* old symmetric limiter would retain cap-5; now taper now */
 assert(machine.servo.diagnostic.current_committed==(int32_t)kp);
 assert(!(machine.servo.diagnostic.limits&FS_LIMIT_RATE));
 sample(22,5); assert(machine.servo.diagnostic.current_committed==(int32_t)(kp+5));
 assert(machine.servo.diagnostic.limits&FS_LIMIT_RATE);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 for (int j=0;j<4;j++) { sample(22,101); off(); }
 fixture(); continuous2_config(cap,100,kp); start250(250); sample(250,5);
 for (int n=1;n<=(int)cap/5;n++) {
     sample(22,5); assert(machine.servo.diagnostic.current_committed==n*5);
 }
 assert(machine.servo.diagnostic.current_committed==cap); /* cap/rate: 100 or 400 ms */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 fixture(); continuous2_config(100,100,2); start250(300);
 for (int j=0;j<20;j++) sample(300,101);
 assert(machine.servo.diagnostic.current_committed==-100);
 sample(251,5); assert(machine.servo.diagnostic.current_committed==-2);
 sample(300,5); assert(machine.servo.diagnostic.current_committed==-7);
 sample(249,5); off(); assert(machine.servo.diagnostic.limits&FS_LIMIT_INTERLOCK);
 sample(249,5); assert(machine.servo.diagnostic.current_committed==2);
 sample(325,5); off(); assert(machine.fault==FAULT_OVERPRESSURE);
 /* Fractional P still truncates toward zero, without a minimum forced drive. */
 fixture(); continuous2_config(100,100,.5f); start250(249); sample(249,101);
 assert(machine.servo.diagnostic.post_limit_output==.5f);
 assert(machine.servo.diagnostic.requested_output==0 && machine.servo.diagnostic.current_committed==0);
 assert(machine.servo.diagnostic.limits&FS_LIMIT_QUANTIZATION); off();
}
static void TestContinuous2Pwm(void)
{
 int32_t actual; bool interlock; uint16_t t2,t3;
 assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_PRESS,0,&t2,&t3)==MOTOR_RESULT_INVALID);
 for (unsigned dir=0;dir<2;dir++) {
     unsigned ceiling=dir ? FS_RELEASE_PROFILE_CEILING : FS_PRESS_PROFILE_CEILING;
     unsigned commands[]={1,4,5,6,99,100,ceiling-1,ceiling};
     for (unsigned j=0;j<sizeof(commands)/sizeof(commands[0]);j++) {
         unsigned v=commands[j]; uint32_t token=continuous();
         assert(update(token,dir ? -(int32_t)v : (int32_t)v,&actual,&interlock)==MOTOR_RESULT_OK);
         unsigned count=(v*4799+23999)/24000;
         assert(actual==(dir ? -(int32_t)v : (int32_t)v));
         assert(TIM2->CCR3==(dir ? count : 0) && TIM3->CCR3==(dir ? 0 : count));
         assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
         advance(5); assert(update(token,0,&actual,&interlock)==MOTOR_RESULT_OK); off();
     }
     assert(MotorExecutor_PlanCommand(dir ? MOTOR_DIRECTION_RELEASE : MOTOR_DIRECTION_PRESS,
                                     ceiling+1,&t2,&t3)==MOTOR_RESULT_INVALID);
 }
 uint32_t token=continuous();
 assert(update(token,INT32_MIN,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void TestContinuous2Peak(void)
{
 fixture(); start250(22); sample(30,101);
 uint32_t controls=machine.servo.diagnostic.control_sequence;
 sample(260,1); /* valid sample between control-grid updates, no PC polling */
 assert(machine.servo.diagnostic.session_peak_raw==260);
 assert(machine.servo.diagnostic.control_sequence==controls);
 sample_at(320,seq,now,true); assert(machine.servo.diagnostic.session_peak_raw==260); /* duplicate */
 uint32_t peak_at=machine.servo.diagnostic.session_peak_received_ms;
 sample(40,5); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); sample(300,101);
 assert(machine.servo.diagnostic.session_peak_raw==260 && machine.servo.diagnostic.session_peak_received_ms==peak_at);
 start250(22); assert(machine.servo.diagnostic.session_peak_raw==22);
 sample(325,5); off(); assert(machine.servo.diagnostic.session_peak_raw==325);
 sample(324,5); assert(machine.servo.diagnostic.session_peak_raw==325);
 fixture(); start250(22); advance(30); sample_at(300,++seq,now-21,true); off();
 assert(machine.state==FAULT && machine.servo.diagnostic.session_peak_raw==22);
}
static void TestContinuous2OldSaturation(void)
{
 fixture(); continuous2_config(100,100,1); start250(22);
 for (int j=0;j<2000 && machine.state!=FAULT;j++) sample(22,5);
 assert(machine.fault==FAULT_MOTION_TIMEOUT && machine.fault_detail==FAULT_DETAIL_SATURATION_TIMEOUT);
 assert(machine.servo.diagnostic.reference==250 && machine.servo.diagnostic.p==228);
 assert(machine.servo.diagnostic.raw_output==228 && machine.servo.diagnostic.requested_output==100);
 assert(machine.servo.diagnostic.saturated_ms==5000); off();
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); sample(22,5); off();
}
