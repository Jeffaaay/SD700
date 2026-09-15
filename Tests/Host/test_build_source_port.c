/* Deterministic sensor/register shims. No claim of physical force attainment. */
#define main LegacyForceServoTests
#include "Tests/Host/test_force_servo.c"
#undef main
#undef RUN
#include "Tests/Host/field182402_oracle.h"
#include "Application/force_build_machine.h"
static void build_plan(float target)
{
 ForceCharacterizationPlan p={target,0,0}; uint32_t u;
 assert(write_reg(FS_REG_PLAN_BEGIN,0xB501)==COMMAND_ACCEPTED);
 for (unsigned i=0;i<3;i++) {
     memcpy(&u,(const unsigned char*)&p+i*4,4);
     assert(write_reg(FS_REG_PLAN_STAGE+2*i,(uint16_t)(u>>16))==COMMAND_ACCEPTED);
     assert(write_reg(FS_REG_PLAN_STAGE+2*i+1,(uint16_t)u)==COMMAND_ACCEPTED);
 }
 uint32_t values[]={machine.servo.plan_version,ForceServo_CharacterizationDigest(&p)};
 for (unsigned i=0;i<2;i++) {
     assert(write_reg(FS_REG_PLAN_STAGE+6+i*2,(uint16_t)(values[i]>>16))==COMMAND_ACCEPTED);
     assert(write_reg(FS_REG_PLAN_STAGE+7+i*2,(uint16_t)values[i])==COMMAND_ACCEPTED);
 }
 assert(write_reg(FS_REG_PLAN_COMMIT,0xC501)==COMMAND_ACCEPTED);
 uint16_t w;
 for (unsigned i=0;i<FS_PLAN_READ_WORDS;i++) assert(ForceServoProtocol_Read(&machine,true,FS_REG_PLAN_ACTIVE+i,&w));
 values[0]=machine.servo.plan_version; values[1]=machine.servo.plan_digest;
 for (unsigned i=0;i<2;i++) {
     assert(write_reg(FS_REG_PLAN_ACK+i*2,(uint16_t)(values[i]>>16))==COMMAND_ACCEPTED);
     assert(write_reg(FS_REG_PLAN_ACK+1+i*2,(uint16_t)values[i])==COMMAND_ACCEPTED);
 }
 assert(write_reg(FS_REG_PLAN_ARM,0xA501)==COMMAND_ACCEPTED);
}
static void build_fixture(int initial)
{
 fixture(); advance(g_force_build_config.full_rest_ms);
 Machine_Tick(&machine,now); sample_at(initial,++seq,now,true);
 assert(!MotorExecutor_GetBuildSnapshot(now).inhibited);
}
static void build_advance(uint32_t ms)
{
 for (uint32_t i=0;i<ms;i++) {
     advance(1);
     MotorResult r=MotorExecutor_Service(now);
     if (r!=MOTOR_RESULT_OK) Machine_ReportFault(&machine,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     Machine_HandleMotorService(&machine,now); Machine_Tick(&machine,now);
 }
}
static void build_sample(int force,uint32_t ms)
{ build_advance(ms); sample_at(force,++seq,now,true); }
static void build_start(int target,int initial)
{
 build_fixture(initial); build_plan((float)target);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 build_sample(initial,5); assert(machine.servo.active);
}
static void brake(void)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 assert(e.preload_active && !e.segment_active && e.preload_command==0);
 assert(MotorHwReal_IsBraking() && !MotorExecutor_OutputIsDisabled());
 assert(TIM2->CCR3==0 && TIM3->CCR3==0);
 assert(MotorExecutor_GetSnapshot()->last_action==MOTOR_ACTION_BUILD_BRAKE);
 assert(MotorExecutor_ActiveRequestIsValid());
}
static void compare_oracle(void)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 int32_t actual=MotorExecutor_OutputIsDisabled() ? 0 : (int32_t)MotorExecutor_GetSnapshot()->command_mv;
 if (actual!=FieldOracle_Command() || (e.preload_active!=0)!=FieldOracle_Braking())
     printf("MISMATCH now=%u actual=%d expected=%d brake=%d/%d state=%u fault=%u detail=%u\n",now,actual,FieldOracle_Command(),e.preload_active,FieldOracle_Braking(),machine.state,machine.fault,machine.fault_detail);
 assert(actual==FieldOracle_Command());
 assert((e.preload_active!=0)==FieldOracle_Braking());
 if (e.segment_active && e.mode!=BUILD_MODE_COARSE) {
     assert(e.normal_ms==FieldOracle_Duration() && e.hard_ms==e.normal_ms+1);
     assert(TIM2->CCR3==0 && TIM3->CCR3==(e.command*4799U+23999U)/24000U);
 }
 assert(machine.servo.diagnostic.source_precision_level==FieldOracle_Level());
 assert(machine.servo.diagnostic.source_precision_escape==FieldOracle_Escape());
 assert(machine.servo.build.boost==FieldOracle_Boost());
}
static void oracle_start(int initial)
{ build_start(250,initial); FieldOracle_Reset(250,initial,now); compare_oracle(); }
static void oracle_run(int force,uint32_t ms)
{
 for (uint32_t i=0;i<ms;i++) {
     build_advance(1); FieldOracle_Time(now);
     if (i%10==9) { sample_at(force,++seq,now,true);FieldOracle_Feed(force,now); }
     compare_oracle(); /* Compare actual high/BRAKE waveform at every1 ms. */
     assert(machine.state!=FAULT);
 }
}
static void TestFullSourceOutputOracle250(void)
{
 oracle_start(0); oracle_run(0,100); oracle_run(10,500);
 assert(machine.state==FORCE_BUILD); /* continuous5 V approach, then bounded pulses */
 oracle_start(10); oracle_run(10,2000);
 assert(machine.servo.build.boost==9000);
 oracle_run(12,200); assert(machine.servo.build.boost>=8500); /* effective rise retains authority */
 oracle_run(14,200); oracle_run(10,200); /* directional fall is not positive response */
 printf("SOURCE_ORACLE FAR target=250 cap=10000 width=12 BRAKE=PASS\n");
 const int forces[]={205,215,235,245,249};
 for (unsigned j=0;j<sizeof(forces)/sizeof(forces[0]);j++) {
     oracle_start(forces[j]); oracle_run(forces[j],32000);
     MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
     if (forces[j]>=210) {
         assert(machine.servo.diagnostic.source_precision_level==32);
         assert(e.command==7000 && e.normal_ms==8);
     } else assert(e.command<=7000 && e.normal_ms<=8);
     assert(machine.servo.build.no_response_ms>5000);
     assert(e.energized_upper_ms>12000 && !e.inhibited); /* no restored12-second gate */
     printf("SOURCE_ORACLE force=%d command=%u normal_ms=%u level=%u escape=%u elapsed=32000 PASS\n",
         forces[j],e.command,e.normal_ms,machine.servo.diagnostic.source_precision_level,machine.servo.diagnostic.source_precision_escape);
 }
}
static void TestSourceFreshCooldownAndNoResponse(void)
{
 build_start(250,245); build_advance(2); brake();
 uint32_t deadline=MotorExecutor_GetBuildSnapshot(now).preload_deadline_ms;
 uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
 for (unsigned i=0;i<5;i++) { build_advance(10);sample_at(245,seq,now,true); }
 assert(machine.servo.diagnostic.source_precision_level==0);
 assert(MotorExecutor_GetBuildSnapshot(now).request==request && MotorExecutor_GetBuildSnapshot(now).preload_deadline_ms==deadline);
 for (unsigned i=0;i<34;i++) build_sample(245,10);
 assert(MotorExecutor_GetBuildSnapshot(now).request==request); /* 392ms after start: final400ms not elapsed */
 build_sample(245,10); assert(MotorExecutor_GetBuildSnapshot(now).request==request+1);
 build_start(250,245);
 for (unsigned i=0;i<4400;i++) { build_sample(245,10); assert(machine.state!=FAULT); }
 assert(machine.servo.diagnostic.source_precision_level==32);
 for (unsigned i=0;i<101 && machine.state!=FAULT;i++) build_sample(245,10);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE);
 uint32_t spent=machine.servo.build.no_response_ms;
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED); off();
 build_sample(245,50); assert(machine.servo.build.no_response_ms==spent);
 printf("FINAL_PRECISION_SEQUENCE_COMPLETES_BEFORE_PERSISTENT_45000_MS_NO_RESPONSE_OFF=PASS\n");
}
static void TestSourceTerminalAndLease(void)
{
 for (unsigned during=0;during<2;during++) for (unsigned cause=0;cause<5;cause++) {
     build_start(250,10); if (during) { build_advance(9);brake(); }
     uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
     if (cause==0) assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);
     else if (cause==1) sample_at(10,++seq,now,false);
     else if (cause==2) sample_at(3000,++seq,now,true);
     else if (cause==3) sample_at(250,++seq,now,true);
     else sample_at(10,++seq,now-21,true);
     off();
     for (unsigned i=0;i<5;i++) { build_sample(10,20);off(); }
     assert(MotorExecutor_GetBuildSnapshot(now).request==request);
     if (cause==3) assert(machine.state==FORCE_TARGET_REACHED_OFF && !machine.servo.ever_held);
     else assert(!machine.servo.active);
 }
 build_start(250,10); build_advance(9);brake();
 uint32_t lease=MotorExecutor_GetBuildSnapshot(now).receive_deadline_ms;
 while (now<lease) advance(1); off(); /* no main/task or feedback processing */
 assert(!MotorExecutor_BuildOwnerValid(machine.servo.token));
 build_start(250,10); uint32_t hard=MotorExecutor_GetBuildSnapshot(now).deadline_ms;
 now=hard;TIM5->SR=TIM_SR_UIF;MotorStopTimer_IrqHandler();off();
 assert(!MotorExecutor_BuildOwnerValid(machine.servo.token));
 build_start(250,10); dsb_hook=stop_before_commit;advance(9);off();
 build_start(250,0);
 for (unsigned i=0;i<801 && machine.state!=FAULT;i++) build_sample(0,10);
 off(); assert(!machine.servo.active); /* independent8 s approach limit remains */
}
static void TestSourceIndependentContractAndPersistence(void)
{
 build_start(250,10);
 const uint32_t *cfg=(const uint32_t *)&g_force_build_config;
 for (unsigned i=0;i<FORCE_BUILD_CONFIG_WORDS;i++) {
     uint16_t word=0;assert(ForceServoProtocol_Read(&machine,true,0x600+i,&word));
     assert(word==(uint16_t)(cfg[i/2]>>((i&1) ? 0 : 16)));
 }
 assert(machine.servo.diagnostic.build_config_digest==ForceBuild_ConfigDigest());
 ForceBuildRequest good={.phase=BUILD_PHASE_BUILD,.command=10000,.hard_ms=13,.mode=BUILD_MODE_MICRO,
     .normal_ms=12,.settle_ms=30,.target=250};
 for (unsigned cause=0;cause<8;cause++) {
     build_fixture(10); uint32_t token;assert(MotorExecutor_BeginBuild(now,&token)==MOTOR_RESULT_OK);
     ForceBuildRequest bad=good;
     if (cause==0) bad.command=10001;
     else if (cause==1) {bad.normal_ms=13;bad.hard_ms=14;}
     else if (cause==2) bad.hard_ms=12;
     else if (cause==3) bad.preload_command=300;
     else if (cause==4) bad.settle_ms=29;
     else if (cause==5) {bad.precision=1;bad.command=7001;bad.normal_ms=8;bad.hard_ms=9;}
     else if (cause==6) {bad.precision=1;bad.command=7000;bad.normal_ms=9;bad.hard_ms=10;}
     else bad.mode=99; /* No reverse or untyped direction contract. */
     assert(MotorExecutor_StartBuildSegment(token,&bad,++seq,now,now)!=MOTOR_RESULT_OK);off();
 }
 build_start(250,245);
 for (unsigned i=0;i<200;i++) build_sample(245,10);
 uint32_t charged=machine.servo.build.no_response_ms;
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);off();build_sample(245,50);
 build_advance(5100);sample_at(245,++seq,now,true);
 build_plan(250);assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);build_sample(245,5);
 assert(machine.servo.build.no_response_ms>=charged); /* Resetting source adaptation cannot erase safety time. */
 uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
 build_advance(2);brake();
 sample_at(245,seq-1,now,true);off();assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_ORDER_LOST);
 build_sample(245,20);off();assert(MotorExecutor_GetBuildSnapshot(now).request==request);
}
int main(void)
{
 setvbuf(stdout,NULL,_IONBF,0);
 TestFullSourceOutputOracle250();puts("TestFullSourceOutputOracle250 PASS");
 TestSourceFreshCooldownAndNoResponse();puts("TestSourceFreshCooldownAndNoResponse PASS");
 TestSourceTerminalAndLease();puts("TestSourceTerminalAndLease PASS");
 TestSourceIndependentContractAndPersistence();puts("TestSourceIndependentContractAndPersistence PASS");
 puts("SOURCE_WORKTREE_ORACLE_AND_AFFECTED_250_PATH=PASS; PHYSICAL_NOT_RUN");return 0;
}
