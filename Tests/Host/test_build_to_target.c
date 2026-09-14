/* Deterministic sensor/register shims. No claim of physical force attainment. */
#define main LegacyForceServoTests
#include "Tests/Host/test_force_servo.c"
#undef main
#undef RUN
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
static void TestBuildApproachAndLowTargets(void)
{
 fixture(); build_plan(250); assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
 build_start(250,0); assert(machine.state==FORCE_APPROACH);
 assert(MotorExecutor_GetSnapshot()->command_mv==5000 && TIM3->CCR3==1000);
 assert(machine.servo.config.press_cap==0 && machine.servo.diagnostic.boost_spent_ms==0);
 build_sample(10,20); off(); assert(machine.state==FORCE_BUILD && machine.servo.build.post_pending);
 uint32_t issued=MotorExecutor_GetBuildSnapshot(now).request;
 build_sample(10,29); assert(MotorExecutor_GetBuildSnapshot(now).request==issued); off();
 build_sample(10,1); assert(MotorExecutor_GetBuildSnapshot(now).request==issued+1);
 assert(MotorExecutor_GetSnapshot()->command_mv==3000);
 const int low[]={1,3,5,10,20};
 for (unsigned i=0;i<5;i++) {
     build_start(low[i],0); assert(machine.state==FORCE_TAPER);
     assert(MotorExecutor_GetSnapshot()->command_mv<=3000);
     build_sample(low[i],1); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 }
 build_start(100,100); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==0);
 build_start(100,150); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
}
static void TestBuildSyntheticTargetsOffAndDecay(void)
{
 const int targets[]={250,500,1000,2000,3000};
 for (unsigned i=0;i<5;i++) {
     printf("SYNTHETIC_TARGET=%d\n",targets[i]); build_start(targets[i],0);
     build_sample(10,100); build_sample(10,100);
     for (int f=20;f<targets[i];f+=10) {
         build_sample(f,100);
         assert(machine.state!=FAULT && machine.servo.active);
     }
     build_sample(targets[i],100); off();
     assert(machine.state==FORCE_TARGET_REACHED_OFF && machine.servo.diagnostic.target_reached);
     uint32_t requests=MotorExecutor_GetBuildSnapshot(now).request;
     for (unsigned j=0;j<100;j++) { build_sample(targets[i]-20,100); off(); }
     assert(MotorExecutor_GetBuildSnapshot(now).request==requests);
     assert(machine.state==FORCE_TARGET_REACHED_OFF && !machine.servo.ever_held);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 }
 build_fixture(2999); build_plan(3000); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 build_sample(3000,5); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==0);
 build_start(250,10); build_sample(3000,1); off(); assert(machine.fault==FAULT_OVERPRESSURE);
}
static void TestBuildSlowProgressAndShortPlateau(void)
{
 build_start(250,0); build_sample(10,100); build_sample(10,100);
 for (int f=11;f<=100;f++) { build_sample(f,100); assert(machine.state!=FAULT); }
 for (unsigned i=0;i<6;i++) { build_sample(100,100); assert(machine.state!=FAULT); }
 assert(machine.servo.diagnostic.tracking_ms>5000);
 assert(machine.servo.build.no_response_ms<1000);
 for (unsigned i=0;i<3;i++) for (unsigned j=0;j<20;j++) {
     build_sample(101+i,100); assert(machine.state!=FAULT); /*1 N per2 s accumulates to2 N. */
 }
 for (unsigned i=0;i<60 && machine.state!=FAULT;i++) build_sample(103,100);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 /* A deliberate fault reset/new plan must not buy even one extra pulse. */
 build_sample(103,100); advance(5000); sample_at(103,++seq,now,true);
 assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED); build_plan(250);
 uint32_t spent=MotorExecutor_GetBuildSnapshot(now).reserved_ms;
 assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==spent);
}
static void TestBuildNoiseAndPersistentBudgets(void)
{
 build_start(250,10);
 for (unsigned i=0;i<60 && machine.state!=FAULT;i++) build_sample(10+i%2,100);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE);
 build_start(250,10); for (unsigned i=0;i<20;i++) build_sample(10,100);
 uint32_t spent=MotorExecutor_GetBuildSnapshot(now).reserved_ms, no_response=machine.servo.build.no_response_ms;
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 build_sample(10,100); advance(5000); sample_at(10,++seq,now,true);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==spent && machine.servo.build.no_response_ms>=no_response);
 build_plan(250); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); build_sample(10,5);
 for (unsigned i=0;i<40 && machine.state!=FAULT;i++) build_sample(10,100);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE);
}
static uint32_t build_owner(void)
{
 build_fixture(10); uint32_t token=0;
 assert(MotorExecutor_BeginBuild(now,&token)==MOTOR_RESULT_OK); return token;
}
static void executor_wait(uint32_t ms)
{
 for (uint32_t i=0;i<ms;i++) { advance(1); assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK); }
}
static void TestBuildContractAndPostFeedback(void)
{
 uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_BUILD,3000,10};
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
 uint32_t deadline=MotorExecutor_GetBuildSnapshot(now).deadline_ms;
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK);
 assert(MotorExecutor_GetBuildSnapshot(now).deadline_ms==deadline);
 uint32_t during=now+5; executor_wait(9); off();
 assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_NORMAL);
 assert(!MotorExecutor_AcceptBuildPost(token,request,seq,during,now));
 executor_wait(30);
 assert(!MotorExecutor_AcceptBuildPost(token,request+1,++seq,now,now));
 assert(!MotorExecutor_AcceptBuildPost(token,request,1,now,now));
 assert(!MotorExecutor_AcceptBuildPost(token,request,seq,now-21,now));
 assert(!MotorExecutor_AcceptBuildPost(token+1,request,seq,now,now));
 assert(MotorExecutor_AcceptBuildPost(token,request,seq,now,now));
 assert(!MotorExecutor_AcceptBuildPost(token,request,seq,now,now));
 assert(MotorExecutor_StartBuildSegment(token,&r,seq,now,now)==MOTOR_RESULT_OK);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==20);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); off();
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK);
 token=build_owner(); assert(MotorExecutor_BeginContinuous(&request)==MOTOR_RESULT_INVALID);
 const ForceBuildRequest invalid[]={ {1,5001,100},{1,5000,101},{2,7001,4},{2,7000,10},
     {2,2999,10},{2,3000,11},{2,3000,1},{3,3001,5},{3,399,5},{3,1000,11},{0,1000,5} };
 for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
     token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&invalid[i],++seq,now,now)!=MOTOR_RESULT_OK); off();
 }
 token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now-21,now)!=MOTOR_RESULT_OK); off();
}
static void build_pending_when_armed(void)
{ if (MotorStopTimer_IsArmed()) pending_expiry(); else dsb_hook=build_pending_when_armed; }
static void TestBuildCutoffsAndStopRaces(void)
{
 ForceBuildRequest r={BUILD_PHASE_APPROACH,5000,100}; uint32_t token=build_owner();
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now-20,now)==MOTOR_RESULT_OK);
 assert(MotorExecutor_GetBuildSnapshot(now).deadline_ms<MotorExecutor_GetBuildSnapshot(now).receive_deadline_ms);
 /* No machine tick/service required for normal independent OFF. */
 for (unsigned i=0;i<99;i++) advance(1);
 off(); assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_NORMAL);
 assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK);
 token=build_owner(); r=(ForceBuildRequest){BUILD_PHASE_BUILD,3000,10};
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 now+=10; TIM5->SR=TIM_SR_UIF; MotorStopTimer_IrqHandler(); off();
 assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_DEADLINE);
 assert(!MotorExecutor_BuildOwnerValid(token));
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
 token=build_owner(); enter_hook=stop_before_commit;
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
 token=build_owner(); dsb_hook=stop_before_commit;
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
 token=build_owner(); dsb_hook=build_pending_when_armed;
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
 token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 dsb_hook=pending_expiry; assert(MotorExecutor_EndBuildSegment(token,now)!=MOTOR_RESULT_OK); off();
 assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_DEADLINE);
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK);
 token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 uint32_t stale_now=now; now+=10; /* Main clock advanced, caller timestamp old; no serviced IRQ. */
 assert(MotorExecutor_EndBuildSegment(token,stale_now)!=MOTOR_RESULT_OK); off();
 assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_DEADLINE);
 build_start(250,0); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 uint32_t spent=MotorExecutor_GetBuildSnapshot(now).reserved_ms;
 build_sample(15,100); off(); assert(!machine.servo.active);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==spent);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
}
static void TestBuildMissingBadFeedbackAndTargetPriority(void)
{
 build_start(250,0); build_advance(126); off();
 assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 build_sample(10,100); off(); assert(machine.state==FAULT);
 build_start(250,0); sample_at(0,++seq,now,false); off(); assert(machine.state==FAULT);
 build_start(250,10); uint64_t old=seq; build_advance(39);
 uint32_t issued=MotorExecutor_GetBuildSnapshot(now).request;
 sample_at(100,old,now,true); off(); assert(MotorExecutor_GetBuildSnapshot(now).request==issued);
 sample_at(10,++seq,now-21,true); off(); assert(MotorExecutor_GetBuildSnapshot(now).request==issued);
 build_advance(100); off(); assert(machine.state==FAULT);
 build_start(250,10); build_sample(240,1); off();
 assert(machine.state==FORCE_TAPER && machine.servo.build.post_pending);
 build_sample(250,1); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 build_sample(200,100); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 build_start(250,10); build_sample(35,40); off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_RESPONSE);
}
static void TestBuildBoostAndTaperEnergy(void)
{
 build_start(250,10);
 for (unsigned i=0;i<30;i++) { build_sample(10,50); assert(machine.state!=FAULT); }
 assert(machine.servo.build.boost==4000);
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 assert(e.command==7000 && e.hard_ms==4 && TIM3->CCR3==1400);
 build_sample(12,50); assert(machine.servo.build.boost==4000 && machine.servo.build.low_count==0);
 assert(machine.servo.diagnostic.post_pulse_valid && machine.servo.build.post_pending);
 assert(machine.servo.diagnostic.post_pulse_request+1==MotorExecutor_GetBuildSnapshot(now).request);
 assert(machine.servo.diagnostic.pulse_force_before==10 && machine.servo.diagnostic.pulse_force_after==12);
 ForceBuildRequest r; uint32_t previous_energy=30000;
 for (int error=50;error>0;error--) {
     assert(ForceBuild_Select(250-error,250,true,4000,&r));
     assert(r.phase==BUILD_PHASE_TAPER && r.command<=3000 && r.command*r.hard_ms<=previous_energy);
     previous_energy=r.command*r.hard_ms;
 }
 assert(ForceBuild_Select(251,250,true,4000,&r) && r.command==0 && r.hard_ms==0);
 assert(!ForceBuild_Select(NAN,250,true,0,&r));
 build_start(250,190); build_sample(201,1); off(); assert(machine.servo.build.boost==0);
 build_sample(201,30); assert(MotorExecutor_GetBuildSnapshot(now).command<3000);
}
static void TestBuildExposureAndUninterruptedCooling(void)
{
 /* Exercise the production executor ledger without a simulated plant timeout. */
 uint32_t token=build_owner(); ForceBuildRequest approach={BUILD_PHASE_APPROACH,5000,100};
 ForceBuildRequest pulse={BUILD_PHASE_BUILD,3000,10};
 for (unsigned i=0;i<80;i++) {
     assert(MotorExecutor_StartBuildSegment(token,&approach,++seq,now,now)==MOTOR_RESULT_OK);
     executor_wait(129); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
     assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
 }
 assert(MotorExecutor_GetBuildSnapshot(now).approach_reserved_ms==8000);
 for (unsigned i=0;i<400;i++) {
     assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK);
     executor_wait(39); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
     assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
 }
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==12000);
 assert(MotorExecutor_GetBuildSnapshot(now).energized_upper_ms==12000);
 assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)!=MOTOR_RESULT_OK); off();
 assert(MotorExecutor_GetBuildSnapshot(now).inhibited);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); MotorExecutor_Service(now);
 uint32_t rest=MotorExecutor_GetBuildSnapshot(now).rest_remaining_ms;
 advance(rest-1); assert(MotorExecutor_GetBuildSnapshot(now).inhibited);
 assert(MotorExecutor_BeginBuild(now,&token)!=MOTOR_RESULT_OK);
 advance(1); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==0);
 assert(MotorExecutor_BeginBuild(now,&token)==MOTOR_RESULT_OK); off(); /* no automatic output */
 assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK);
 executor_wait(39); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
 assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
 executor_wait(100000); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==10);
 assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK);
 executor_wait(39); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==20);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK);
 assert(MotorExecutor_Initialize()==MOTOR_RESULT_OK); /* reboot must impose full OFF qualification */
 assert(MotorExecutor_GetBuildSnapshot(now).inhibited && MotorExecutor_BeginBuild(now,&token)!=MOTOR_RESULT_OK); off();
 build_start(250,0);
 for (unsigned i=0;i<170 && machine.state!=FAULT;i++) build_sample(0,100);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_EXPOSURE);
 assert(MotorExecutor_GetBuildSnapshot(now).approach_reserved_ms==8000);
}
static void TestBuildPlanAndReadbackContract(void)
{
 build_fixture(0); ForceCharacterizationPlan p={250,1,0};
 assert(!ForceServo_CharacterizationPlanValid(&p)); p.assist_percent=0; p.continuous_percent=1;
 assert(!ForceServo_CharacterizationPlanValid(&p));
 uint32_t expected[sizeof(ForceBuildConfig)/4]; memcpy(expected,&g_force_build_config,sizeof(expected));
 for (unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++) {
     uint16_t hi,lo; assert(ForceServoProtocol_Read(&machine,true,0x600+2*i,&hi));
     assert(ForceServoProtocol_Read(&machine,true,0x601+2*i,&lo));
     assert((((uint32_t)hi<<16)|lo)==expected[i]);
     assert(write_reg(0x600+2*i,0)!=COMMAND_ACCEPTED);
 }
 assert(machine.servo.diagnostic.build_config_digest==ForceBuild_ConfigDigest());
}
int main(void)
{
 setvbuf(stdout,NULL,_IONBF,0);
#define RUN(f) f(); puts(#f " PASS")
 RUN(TestBuildApproachAndLowTargets); RUN(TestBuildSyntheticTargetsOffAndDecay);
 RUN(TestBuildSlowProgressAndShortPlateau); RUN(TestBuildNoiseAndPersistentBudgets);
 RUN(TestBuildContractAndPostFeedback); RUN(TestBuildCutoffsAndStopRaces);
 RUN(TestBuildMissingBadFeedbackAndTargetPriority); RUN(TestBuildBoostAndTaperEnergy);
 RUN(TestBuildExposureAndUninterruptedCooling); RUN(TestBuildPlanAndReadbackContract);
 RUN(TestTrajectory); RUN(TestNumericAndD); RUN(TestAntiWindup);
 puts("BUILD_TO_TARGET_GROUPS=13 PASS; SYNTHETIC_ONLY; PHYSICAL_NOT_RUN"); return 0;
}
