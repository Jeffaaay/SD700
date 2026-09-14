/* Production path, real output gate/executor/HW/TIM5; only MCU registers are shims. */
#define main ForceServoLegacyRegressionMain
#include "Tests/Host/test_force_servo.c"
#undef main

static void plan_stage(float target,float assist,float continuous,uint32_t version,uint32_t digest)
{
 ForceCharacterizationPlan p={target,assist,continuous}; uint32_t u;
 assert(write_reg(FS_REG_PLAN_BEGIN,0xB501)==COMMAND_ACCEPTED);
 for (unsigned i=0;i<3;i++) { memcpy(&u,(unsigned char*)&p+i*4,4);
     assert(write_reg(FS_REG_PLAN_STAGE+2*i,(uint16_t)(u>>16))==COMMAND_ACCEPTED);
     assert(write_reg(FS_REG_PLAN_STAGE+2*i+1,(uint16_t)u)==COMMAND_ACCEPTED); }
 assert(write_reg(FS_REG_PLAN_STAGE+6,(uint16_t)(version>>16))==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_STAGE+7,(uint16_t)version)==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_STAGE+8,(uint16_t)(digest>>16))==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_STAGE+9,(uint16_t)digest)==COMMAND_ACCEPTED);
}
static void acknowledge(bool readback)
{
 uint16_t words[FS_PLAN_READ_WORDS];
 if (readback) for (unsigned i=0;i<FS_PLAN_READ_WORDS;i++)
     assert(ForceServoProtocol_Read(&machine,true,FS_REG_PLAN_ACTIVE+i,&words[i]));
 uint32_t v=machine.servo.plan_version,d=machine.servo.plan_digest;
 assert(write_reg(FS_REG_PLAN_ACK,(uint16_t)(v>>16))==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_ACK+1,(uint16_t)v)==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_ACK+2,(uint16_t)(d>>16))==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_ACK+3,(uint16_t)d)==COMMAND_ACCEPTED);
}
static void plan(float target,float assist,float continuous)
{
 ForceCharacterizationPlan p={target,assist,continuous};
 plan_stage(target,assist,continuous,machine.servo.plan_version,ForceServo_CharacterizationDigest(&p));
 assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)==COMMAND_ACCEPTED);
 acknowledge(true); assert(write_reg(FS_REG_PLAN_ARM,0xA501)==COMMAND_ACCEPTED);
 assert(machine.servo.config.kp==10 && machine.servo.config.ki==0 && machine.servo.config.kd==0);
 assert(machine.servo.config.measurement_filter_s==0);
}
static void run(float target,float assist,float continuous)
{
 fixture(); plan(target,assist,continuous);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_BUSY);
 sample(30,5); assert(machine.state==FORCE_BUILD); assert(machine.servo.session==1);
}
static void assist_begin(float assist,float cap)
{
 run(500,assist,cap); sample(30,10);
 assert(machine.servo.diagnostic.boost_active); assert(machine.servo.diagnostic.boost_spent_ms==4);
 assert(machine.servo.diagnostic.assist_requested_peak==(uint32_t)ForceServo_PercentCommand(assist));
}
static void TestRuntimeMappings(void)
{
 const float a[]={0,20,25,30,35,40},c[]={0,3,5,7.5f,8.5f,10};
 const int ac[]={0,4800,6000,7200,8400,9600},cc[]={0,720,1200,1800,2040,2400};
 const int t[]={1,250,350,500,650,1000,2999,3000};
 for (unsigned i=0;i<6;i++) for (unsigned j=0;j<6;j++) for (unsigned k=0;k<8;k++) {
     fixture(); plan((float)t[k],a[i],c[j]);
     assert(machine.target_pressure_units==t[k] && machine.target_valid);
     assert(machine.servo.profile.peak_press==ac[i] && machine.servo.config.press_cap==cc[j]);
 }
 ForceCharacterizationPlan bad[]={{3001,20,5},{0,0,0},{1.5f,0,0},{250,40.01f,5},
     {250,20,10.01f},{250,-1,5},{250,20,-1},{NAN,20,5},{250,NAN,5},{250,20,INFINITY}};
 for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
     fixture(); plan_stage(bad[i].target_N,bad[i].assist_percent,bad[i].continuous_percent,0,ForceServo_CharacterizationDigest(&bad[i]));
     assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)!=COMMAND_ACCEPTED);
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); off();
 }
}
static void TestAtomicPlanAndGains(void)
{
 fixture(); plan(500,25,8.5f); ForceServoConfig old=machine.servo.config;
 assert(write_reg(FS_REG_PLAN_BEGIN,0xB501)==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_PLAN_STAGE,0x7fc0)==COMMAND_ACCEPTED);
 assert(memcmp(&old,&machine.servo.config,sizeof(old))==0);
 assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)!=COMMAND_ACCEPTED); off();
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 ForceCharacterizationPlan p={500,25,8.5f};
 plan_stage(500,25,8.5f,0,ForceServo_CharacterizationDigest(&p)); /* stale version */
 assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)!=COMMAND_ACCEPTED);
 plan_stage(500,25,8.5f,1,1); assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)!=COMMAND_ACCEPTED);
 plan_stage(500,25,8.5f,1,ForceServo_CharacterizationDigest(&p));
 assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)==COMMAND_ACCEPTED);
 acknowledge(false); assert(write_reg(FS_REG_PLAN_ARM,0xA501)!=COMMAND_ACCEPTED);
 plan(500,25,8.5f);
 assert(write_reg(FS_REG_CONFIG,0)!=COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 assert(memcmp(&old,&machine.servo.config,sizeof(old))==0);
 plan(500,25,8.5f); assert(command(CMD_SET_TARGET,250)!=COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 plan(500,25,8.5f); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 assert(write_reg(FS_REG_PLAN_ARM,0xA501)!=COMMAND_ACCEPTED);
 sample(30,5); assert(machine.state==IDLE && !machine.servo.active);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
}
static void TestCharacterizationAssistTiming(void)
{
 const float a[]={20,25,30,35,40,1};
 const float cap[]={0,3,5,7.5f,8.5f,10};
 for (unsigned i=0;i<6;i++) {
     assist_begin(a[i],cap[i]); uint32_t rx=machine.pressure.received_at_ms;
     uint32_t cs=machine.servo.diagnostic.control_sequence;
     advance(1); Machine_Tick(&machine,now);
     assert(machine.state==FORCE_BUILD);
     assert(TIM3->CCR3==(uint32_t)((ForceServo_PercentCommand(a[i])+4)/5));
     assert(machine.servo.diagnostic.boost_peak_command==(uint32_t)ForceServo_PercentCommand(a[i]));
     advance(1); Machine_Tick(&machine,now);
     assert(!machine.servo.diagnostic.boost_active && machine.servo.diagnostic.assist_response_pending);
     assert(machine.servo.diagnostic.boost_elapsed_ms==2);
     assert(MotorExecutor_GetSnapshot()->command_mv<=machine.servo.config.press_cap);
     assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==rx+130);
     assert(machine.servo.diagnostic.control_sequence==cs);
     assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
     sample(31,99); assert(!machine.servo.diagnostic.assist_response_pending);
     assert(machine.servo.diagnostic.assist_after_result==1);
     for (unsigned j=0;j<20;j++) { sample(31,10); Machine_Tick(&machine,now); }
     assert(machine.servo.diagnostic.boost_spent_ms==4 && !machine.servo.diagnostic.boost_active);
     assert(machine.servo.controller.integral==0);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
     plan(500,a[i],cap[i]); assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY);
     advance(5000); sample_at(30,++seq,now,true);
     assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(30,5); sample(30,10);
     assert(machine.servo.diagnostic.boost_active && machine.servo.diagnostic.boost_spent_ms==4);
 }
 run(500,0,0); sample(30,10); assert(!machine.servo.diagnostic.boost_active); off();
 run(40,40,10); sample(30,10); assert(!machine.servo.diagnostic.boost_active); /* taper */
}
static void TestCharacterizationResponseAndStops(void)
{
 assist_begin(40,10); advance(1); Machine_Tick(&machine,now); advance(1); Machine_Tick(&machine,now);
 uint32_t updates=machine.servo.diagnostic.control_sequence;
 sample(55,99); off(); assert(machine.fault_detail==FAULT_DETAIL_POST_ASSIST_EXCESSIVE_RISE);
 assert(machine.servo.diagnostic.control_sequence==updates);
 assert(machine.servo.diagnostic.assist_after_result==2);
 sample(30,10); Machine_Tick(&machine,now); off(); assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 for (int stage=0;stage<3;stage++) {
     assist_begin(40,10);
     if (stage) { advance(1); Machine_Tick(&machine,now); }
     if (stage==2) { advance(1); Machine_Tick(&machine,now); }
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     sample(30,100); off(); assert(machine.state==IDLE);
 }
 assist_begin(40,10); advance(4); off(); /* No main service or pressure required. */
 assert(MotorExecutor_ContinuousExpired());
 Machine_HandleMotorService(&machine,now); Machine_Tick(&machine,now); off();
 assert(machine.state==FAULT);
 assist_begin(40,10); advance(1); dsb_hook=pending_expiry; Machine_Tick(&machine,now); off();
 assert(machine.state==FAULT);
}
static void TestCharacterizationSessionsAndBoundary(void)
{
 const int targets[]={250,500,1000,2999,3000};
 for (unsigned i=0;i<5;i++) {
     run((float)targets[i],0,10);
     for (unsigned j=0;j<499;j++) { sample(30,10); assert(machine.servo.active); }
     sample(30,10); off(); assert(machine.fault_detail==FAULT_DETAIL_SESSION_TIMEOUT);
     assert(machine.servo.diagnostic.run_reason==FS_RUN_TARGET_NOT_REACHED_WITHIN_SESSION);
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 }
 run(1000,0,10);
 for (unsigned j=0;j<499;j++) sample(30,10);
 advance(9); off(); /* TIM5 absolute compare at4999 ms, runtime safety precedes Service. */
 Machine_CheckPressureSafety(&machine,now);
 assert(machine.fault_detail==FAULT_DETAIL_SESSION_TIMEOUT);
 assert(machine.servo.diagnostic.run_reason==FS_RUN_TARGET_NOT_REACHED_WITHIN_SESSION);
 run(3000,0,10); sample(2999,10); assert(machine.servo.active);
 sample(3000,10); off(); assert(machine.state==IDLE && !machine.servo.active && !machine.servo.ever_held);
 assert(machine.servo.diagnostic.run_reason==FS_RUN_BOUNDARY_TARGET_REACHED);
 assert(machine.servo.diagnostic.target_reached && machine.servo.diagnostic.session_peak_measured==3000);
 fixture(); plan(3000,40,10); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 advance(10); sample_at(3000,++seq,now-5,true); off(); assert(machine.state==IDLE && !machine.servo.start_pending);
 assert(machine.servo.diagnostic.target_reached_ms-machine.servo.session_started_ms==5);
 assert(machine.servo.diagnostic.run_reason==FS_RUN_BOUNDARY_TARGET_REACHED && machine.servo.session==1);
 assert(machine.servo.diagnostic.boost_spent_ms==0 && !MotorExecutor_GetSnapshot()->logical_active);
 assert(machine.servo.cooling_active);
 run(2999,0,10); sample(3000,10); off(); assert(machine.fault==FAULT_OVERPRESSURE);
 run(30,0,10); sample(28,10); assert(machine.state==FORCE_HOLD && TIM3->CCR3>0);
 assert(MotorExecutor_GetSnapshot()->command_mv<2400); /* ceiling, not fixed output */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); sample(28,10); off();
 run(1000,0,10); sample(30,10); advance(126); Machine_Tick(&machine,now); off();
 assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_TIMEOUT);
 run(1000,0,10); sample(30,10); sample_at(30,++seq,now,false); off();
 assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_INVALID);
 run(1000,0,10); sample_at(65536,++seq,now,true); off(); assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_INVALID);
 run(3000,0,10); advance(21); sample_at(3000,++seq,now-21,true); off();
 assert(!machine.servo.diagnostic.target_reached); /* stale is never successful boundary evidence */
}
static void TestCharacterizationWrap(void)
{
 fixture(); now=UINT32_MAX-20; seq=UINT64_MAX-1; machine.servo.have_sample=false;
 sample_at(30,seq,now,true); plan(500,40,10); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 sample(30,5); sample(30,10); advance(1); Machine_Tick(&machine,now);
 advance(1); Machine_Tick(&machine,now); sample(31,99);
 assert(machine.state==FORCE_BUILD && machine.servo.diagnostic.assist_after_result==1);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 plan(500,40,10); assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 advance(5000); sample_at(30,++seq,now,true); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
}
static void TestCharacterizationRtuStopPriority(void)
{
 fixture(); ModbusRtuServer server; ModbusRtuServer_Initialize(&server,&machine);
 request(&server,6,FS_REG_PLAN_BEGIN,0xB501); assert(ModbusRtuServer_ProcessPending(&server,now));
 size_t length; const uint8_t *r=ModbusRtuServer_GetResponse(&server,&length);
 assert(length==8 && r[1]==6); ModbusRtuServer_CompleteResponse(&server);
 plan(500,40,10); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); sample(30,5); sample(30,10);
 advance(1); Machine_Tick(&machine,now); assert(TIM3->CCR3==1920);
 for (int j=0;j<10;j++) request(&server,6,FS_REG_SNAPSHOT,0xD101);
 request(&server,5,1,0); assert(ModbusRtuServer_TakeStop(&server,now)); off(); assert(machine.state==IDLE);
 assert(ModbusRtuServer_GetSnapshot(&server)->normal_queue_full_drop_count>0);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
}
static void TestRuntimeContinuousCeilings(void)
{
 const float percents[]={0,3,5,7.5f,8.5f,10};
 for (unsigned i=0;i<6;i++) {
     run(1000,0,percents[i]); uint32_t peak=0;
     for (unsigned j=0;j<400;j++) {
         sample(30,10); assert(machine.state==FORCE_BUILD);
         const MotorExecutorSnapshot *e=MotorExecutor_GetSnapshot();
         assert(e->command_mv<=(uint32_t)ForceServo_PercentCommand(percents[i]));
         if (e->command_mv>peak) peak=e->command_mv;
         assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
     }
     assert(peak==(uint32_t)ForceServo_PercentCommand(percents[i]));
     assert(TIM3->CCR3==(peak+4)/5); /* No downstream720 clamp. */
     assert(!machine.servo.diagnostic.boost_spent_ms);
 }
 run(250,0,10); for (unsigned j=0;j<300;j++) sample(30,10);
 assert(MotorExecutor_GetSnapshot()->command_mv==2200); /* cap2400 is not fixed output. */
 sample(240,10); assert(MotorExecutor_GetSnapshot()->command_mv==100);
 sample(250,10); off(); assert(machine.state==FORCE_HOLD);
 sample(249,10); assert(MotorExecutor_GetSnapshot()->command_mv==10);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
}
int main(void)
{
 setvbuf(stdout,NULL,_IONBF,0);
#define RUN_TEST(f) f(); puts(#f " PASS")
 RUN_TEST(TestRuntimeContinuousCeilings); RUN_TEST(TestCharacterizationRtuStopPriority); RUN_TEST(TestRuntimeMappings); RUN_TEST(TestAtomicPlanAndGains);
 RUN_TEST(TestCharacterizationAssistTiming); RUN_TEST(TestCharacterizationResponseAndStops);
 RUN_TEST(TestCharacterizationSessionsAndBoundary);
 RUN_TEST(TestExecutorUpdates); RUN_TEST(TestLeaseRaces); RUN_TEST(TestCharacterizationWrap);
 RUN_TEST(TestTrajectory); RUN_TEST(TestNumericAndD); RUN_TEST(TestAntiWindup);
 puts("CHARACTERIZATION_PRODUCTION_GROUPS=13 PASS; PHYSICAL_NOT_RUN"); return 0;
}
