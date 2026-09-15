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
static void preload(uint32_t command)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 assert(e.preload_active && !e.segment_active && e.preload_command==command);
 assert(!MotorExecutor_OutputIsDisabled() && MotorExecutor_ActiveRequestIsValid());
 assert(MotorExecutor_GetSnapshot()->last_action==MOTOR_ACTION_BUILD_PRELOAD);
 assert(MotorExecutor_GetSnapshot()->command_mv==command && TIM2->CCR3==0 && TIM3->CCR3==(command+4)/5);
 assert(MotorStopTimer_IsArmed() && MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
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
 const int low[]={1,3,5,10,20,300};
 for (unsigned i=0;i<5;i++) {
     build_start(low[i],0); assert(machine.state==(low[i]<=3 ? FORCE_TAPER : FORCE_APPROACH));
     assert(MotorExecutor_GetSnapshot()->command_mv==(low[i]==1 ? 400U : low[i]==3 ? 1000U : 5000U));
     build_sample(low[i],1); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 }
 build_start(100,100); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==0);
 build_start(100,150); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
}
static void TestBuildSyntheticTargetsOffAndDecay(void)
{
 const int targets[]={250,500,1000,2000,3000,300};
 for (unsigned i=0;i<5;i++) {
     printf("SYNTHETIC_TARGET=%d\n",targets[i]); build_start(targets[i],0);
     build_sample(10,100); build_sample(10,100);
     for (int f=20;f<targets[i];f+=20) {
         build_sample(f,50);
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
 for (int f=12;f<=100;f+=2) { build_sample(f,100); assert(machine.state!=FAULT); }
 for (unsigned i=0;i<6;i++) { build_sample(100,100); assert(machine.state!=FAULT); }
 assert(machine.servo.diagnostic.tracking_ms>3000);
 assert(machine.servo.build.no_response_ms<1000);
 /* Independent credited-progress scenario: physical ON budget remains12 s. */
 build_start(250,100);
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
/* Regression from review7531723: a previous session's150 N high must not
 * become the next session's progress threshold after unloading to0 N. */
static void restart_after_150(bool approach)
{
 build_start(500,approach ? 0 : 10);
 if (approach) { build_sample(10,20); build_sample(10,30); }
 for (int f=20;f<=150;f+=10) { build_sample(f,100); assert(machine.state!=FAULT); }
 assert(machine.servo.build.progress_anchor==150);
 build_sample(150,100); /* Retain a nonzero no-response charge across STOP. */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 build_sample(0,100); build_advance(5100); sample_at(0,++seq,now,true); off();
 assert(machine.servo.build.progress_anchor==150);
}
static void TestBuildSecondStartProgressAndPlatform(void)
{
 restart_after_150(false);
 uint32_t carried=machine.servo.build.no_response_ms;
 assert(carried>0);
 build_plan(500); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 build_sample(10,5);
 assert(machine.servo.build.no_response_ms==carried); /* START cannot buy time. */
 for (int f=11;f<=61;f++) {
     build_sample(f,100);
     if (machine.state==FAULT) printf("CROSS_START_REPRO force=%d anchor=%.0f detail=%u no_response_ms=%lu\n",
         f,(double)machine.servo.build.progress_anchor,(unsigned)machine.fault_detail,
         (unsigned long)machine.servo.build.no_response_ms);
     assert(machine.state!=FAULT);
 }
 assert(machine.servo.build.progress_anchor==60 && machine.servo.build.no_response_ms==100);
 uint32_t plateau=now;
 for (unsigned i=0;i<49;i++) {
     build_sample(61,100);
     if (i<48) assert(machine.state!=FAULT);
 }
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE && now-plateau==4900);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 puts("CROSS_START_REPRO second10_to61=PASS; platform4900ms_PLUS_carried100ms=OFF_DETAIL22");
}
static void assert_restart_budget(MotorBuildSnapshot prior,uint32_t extra,uint32_t response)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 assert(e.epoch==prior.epoch && e.reserved_ms==prior.reserved_ms+extra);
 assert(e.approach_reserved_ms==prior.approach_reserved_ms && e.approach_command_ms==prior.approach_command_ms);
 assert(machine.servo.build.no_response_ms==response);
}
static void TestBuildStartAnchorRequiresFreshFrame(void)
{
 restart_after_150(true);
 MotorBuildSnapshot prior=MotorExecutor_GetBuildSnapshot(now);
 uint32_t response=machine.servo.build.no_response_ms;
 assert(prior.approach_reserved_ms>0 && prior.approach_command_ms>0 && response>0);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); assert_restart_budget(prior,0,response);
 build_advance(5100); sample_at(0,++seq,now,true);
 build_plan(500); assert(machine.servo.build.progress_anchor==150); assert_restart_budget(prior,0,response);
 build_advance(5);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 assert(machine.servo.start_pending && machine.servo.build.progress_anchor==150);
 assert(command(CMD_FORCE_START,0)==COMMAND_BUSY); assert_restart_budget(prior,0,response);
 sample_at(10,++seq,now-1,true); /* fresh but received before this START */
 assert(machine.servo.start_pending && machine.servo.build.progress_anchor==150); off();
 sample_at(10,seq,now,true); /* duplicate cannot initialize the session */
 assert(machine.servo.start_pending && machine.servo.build.progress_anchor==150); off();
 build_advance(25); sample_at(10,++seq,now-21,true); /* ordered but too old */
 assert(machine.servo.start_pending && machine.servo.build.progress_anchor==150); off();
 assert_restart_budget(prior,0,response);
 sample_at(10,++seq,now,true);
 assert(machine.servo.active && !machine.servo.start_pending && machine.servo.build.progress_anchor==10);
 assert_restart_budget(prior,11,response); /* one existing3000/11-ms hard reservation */
 assert(command(CMD_FORCE_START,0)==COMMAND_BUSY); assert_restart_budget(prior,11,response);
 build_sample(11,100); assert(machine.servo.build.progress_anchor==10 && machine.servo.build.no_response_ms==response+100);
 build_sample(12,100); assert(machine.servo.build.progress_anchor==12 && machine.servo.build.no_response_ms==0);
 /* A repeated START during output cannot lower an established progress anchor. */
 assert(command(CMD_FORCE_START,0)==COMMAND_BUSY); build_sample(10,100);
 assert(machine.servo.build.progress_anchor==12 && machine.servo.build.no_response_ms==100);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 prior=MotorExecutor_GetBuildSnapshot(now); response=machine.servo.build.no_response_ms;
 build_sample(14,100); off(); assert(machine.servo.build.progress_anchor==12);
 assert_restart_budget(prior,0,response);
}
static void TestBuildRestartNoiseBadFramesAndStop(void)
{
 /* Repeated explicit new sessions without net post-pulse progress must still
  * exhaust the SAME response allowance. Off cooling here is less than108 s. */
 build_start(500,0); build_sample(10,20); build_sample(10,30);
 uint32_t epoch=MotorExecutor_GetBuildSnapshot(now).epoch;
 for (unsigned session=0;session<6 && machine.state!=FAULT;session++) {
     for (unsigned i=0;i<10 && machine.state!=FAULT;i++) {
         uint32_t before=machine.servo.build.no_response_ms;
         assert(command(CMD_FORCE_START,0)==COMMAND_BUSY);
         build_sample(10+i%2,100);
         assert(machine.servo.build.no_response_ms>=before);
     }
     if (machine.state==FAULT) break;
     uint32_t response=machine.servo.build.no_response_ms;
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     MotorBuildSnapshot prior=MotorExecutor_GetBuildSnapshot(now);
     build_sample(0,100); build_advance(5100); sample_at(0,++seq,now,true);
     build_plan(500); assert_restart_budget(prior,0,response);
     assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); build_sample(10,5);
     assert_restart_budget(prior,11,response);
 }
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_NO_RESPONSE);
 assert(machine.servo.build.no_response_ms==5000 && MotorExecutor_GetBuildSnapshot(now).epoch==epoch);
 assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 /* Invalid/old/duplicate feedback and STOP while pending cannot establish a
  * new baseline or erase budgets. Each bad-frame path is an independent run. */
 for (unsigned kind=0;kind<4;kind++) {
     restart_after_150(true); build_plan(500);
     MotorBuildSnapshot prior=MotorExecutor_GetBuildSnapshot(now);
     uint32_t response=machine.servo.build.no_response_ms;
     assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
     if (kind==0) sample_at(10,++seq,now,false);
     else if (kind==1) { sample_at(10,seq,now,true); build_advance(FORCE_SERVO_START_WAIT_MS); }
     else if (kind==2) { build_advance(25); sample_at(10,++seq,now-21,true); build_advance(FORCE_SERVO_START_WAIT_MS); }
     else { assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); build_sample(10,100); }
     off(); assert(!machine.servo.active && !machine.servo.start_pending);
     assert(machine.servo.build.progress_anchor==150); assert_restart_budget(prior,0,response);
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 }
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
 uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_BUILD,3000,11,3000,BUILD_MODE_MICRO,300};
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
 uint32_t deadline=MotorExecutor_GetBuildSnapshot(now).deadline_ms;
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK);
 assert(MotorExecutor_GetBuildSnapshot(now).deadline_ms==deadline);
 uint32_t during=now+5; executor_wait(10); preload(300);
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
 assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==142);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); off();
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK);
 token=build_owner(); assert(MotorExecutor_BeginContinuous(&request)==MOTOR_RESULT_INVALID);
 const ForceBuildRequest invalid[]={ {1,5001,100,5000,1,0},{1,8501,100,5000,1,0},{1,8500,101,5000,1,0},
     {2,7001,5,3000,2,300},{2,7000,11,3000,2,300},{2,2999,11,3000,2,300},{2,3000,12,3000,2,300},
     {2,3000,2,3000,2,300},{3,2001,5,1000,3,300},{3,399,11,400,3,300},{3,1000,12,1000,3,300},
     {0,1000,5,1000,3,300},{3,7000,5,1000,3,300},{1,8500,100,8500,1,0},{2,7000,5,3001,2,300} };
 for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
     token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&invalid[i],++seq,now,now)!=MOTOR_RESULT_OK); off();
 }
 token=build_owner(); assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now-21,now)!=MOTOR_RESULT_OK); off();
}
static void build_pending_when_armed(void)
{ if (MotorStopTimer_IsArmed()) pending_expiry(); else dsb_hook=build_pending_when_armed; }
static void TestBuildCutoffsAndStopRaces(void)
{
 ForceBuildRequest r={BUILD_PHASE_APPROACH,5000,100,5000,BUILD_MODE_COARSE,0}; uint32_t token=build_owner();
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now-20,now)==MOTOR_RESULT_OK);
 assert(MotorExecutor_GetBuildSnapshot(now).deadline_ms<MotorExecutor_GetBuildSnapshot(now).receive_deadline_ms);
 /* No machine tick/service required for normal independent OFF. */
 for (unsigned i=0;i<99;i++) advance(1);
 off(); assert(MotorExecutor_GetBuildSnapshot(now).end_reason==BUILD_END_NORMAL);
 assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK);
 token=build_owner(); r=(ForceBuildRequest){BUILD_PHASE_BUILD,3000,11,3000,BUILD_MODE_MICRO,300};
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
 now+=11; TIM5->SR=TIM_SR_UIF; MotorStopTimer_IrqHandler(); off();
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
 uint32_t stale_now=now; now+=11; /* Main clock advanced, caller timestamp old; no serviced IRQ. */
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
 sample_at(100,old,now,true); preload(300); assert(MotorExecutor_GetBuildSnapshot(now).request==issued);
 sample_at(10,++seq,now-21,true); off(); assert(MotorExecutor_GetBuildSnapshot(now).request==issued);
 build_advance(100); off(); assert(machine.state==FAULT);
 build_start(250,10); build_sample(240,1); preload(300);
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
 assert(e.command==7000 && e.hard_ms==5 && TIM3->CCR3==1400);
 build_sample(12,50); assert(machine.servo.build.boost==0 && machine.servo.build.low_count==0);
 assert(machine.servo.diagnostic.post_pulse_valid && machine.servo.build.post_pending);
 assert(machine.servo.diagnostic.post_pulse_request+1==MotorExecutor_GetBuildSnapshot(now).request);
 assert(machine.servo.diagnostic.pulse_force_before==10 && machine.servo.diagnostic.pulse_force_after==12);
 /* Values independently calculated from actual old source formulas.
  * MICRO/FINE switch is discontinuous in the source: error4 base846,
  * error3 base1000. Do not falsely assert strict monotonicity at that boundary. */
 const struct { int error; unsigned boost,base,command,normal,mode; } cases[]={
     {240,0,3000,3000,10,2},{240,300,3000,3300,9,2},{240,4000,3000,7000,4,2},
     {50,4000,3000,7000,4,2},{26,300,1876,2176,8,2},
     {5,0,893,893,10,2},{4,4000,846,4846,2,2},{3,4000,1000,2000,5,3},
     {2,300,700,1000,7,3},{1,1000,400,1400,2,3}};
 ForceBuildRequest r;
 for (unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
     assert(ForceBuild_Select(250-cases[i].error,250,true,cases[i].boost,0,&r));
     assert(r.base_command==cases[i].base && r.command==cases[i].command &&
            r.hard_ms==cases[i].normal+1 && r.mode==cases[i].mode);
 }
 /* Representative decreasing-error MICRO -> FINE: less nominal command-time. */
 ForceBuildRequest micro,fine;
 assert(ForceBuild_Select(245,250,true,300,0,&micro));
 assert(ForceBuild_Select(248,250,true,300,0,&fine));
 assert(fine.command*(fine.hard_ms-1)<micro.command*(micro.hard_ms-1));
 assert(ForceBuild_Select(251,250,true,4000,0,&r) && r.command==0 && r.hard_ms==0);
 assert(!ForceBuild_Select(NAN,250,true,0,0,&r));
 build_start(250,190); build_sample(201,1); preload(300); assert(machine.servo.build.boost==0);
 build_sample(201,30); assert(MotorExecutor_GetBuildSnapshot(now).command<3000);
}
static void TestBuildExposureAndUninterruptedCooling(void)
{
 /* Exercise the production executor ledger without a simulated plant timeout. */
 uint32_t token=build_owner(); ForceBuildRequest approach={BUILD_PHASE_APPROACH,5000,100,5000,BUILD_MODE_COARSE,0};
 ForceBuildRequest pulse={BUILD_PHASE_BUILD,3300,10,3000,BUILD_MODE_MICRO,300};
 for (unsigned i=0;i<80;i++) {
     assert(MotorExecutor_StartBuildSegment(token,&approach,++seq,now,now)==MOTOR_RESULT_OK);
     executor_wait(129); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
     assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
 }
 assert(MotorExecutor_GetBuildSnapshot(now).approach_reserved_ms==8000);
 unsigned pulses=0;
 while (MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK) {
     executor_wait(39); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
     assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
     assert(++pulses<400); /* Preload now consumes the SAME12 s budget. */
 }
 off(); MotorBuildSnapshot spent=MotorExecutor_GetBuildSnapshot(now);
 assert(spent.reserved_ms<=12000 && spent.reserved_ms+10>12000 && spent.inhibited);
 assert(spent.energized_upper_ms<=spent.reserved_ms && spent.preload_spent_ms>0);
 assert(spent.approach_reserved_ms==8000 && spent.approach_command_ms==40000000);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); MotorExecutor_Service(now);
 uint32_t rest=MotorExecutor_GetBuildSnapshot(now).rest_remaining_ms;
 advance(rest-1); assert(MotorExecutor_GetBuildSnapshot(now).inhibited);
 assert(MotorExecutor_BeginBuild(now,&token)!=MOTOR_RESULT_OK);
 advance(1); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==0);
 assert(MotorExecutor_BeginBuild(now,&token)==MOTOR_RESULT_OK); off(); /* no automatic output */
 assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK);
 executor_wait(39); uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
 assert(MotorExecutor_AcceptBuildPost(token,req,++seq,now,now));
 preload(300); assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); off();
 uint32_t reserved=MotorExecutor_GetBuildSnapshot(now).reserved_ms;
 executor_wait(100000); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==reserved);
 assert(MotorExecutor_BeginBuild(now,&token)==MOTOR_RESULT_OK);
 assert(MotorExecutor_StartBuildSegment(token,&pulse,++seq,now,now)==MOTOR_RESULT_OK);
 executor_wait(39); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms>=reserved+10);
 assert(MotorExecutor_Disable()==MOTOR_RESULT_OK);
 uint32_t reserved2=MotorExecutor_GetBuildSnapshot(now).reserved_ms;
 executor_wait(8001); assert(MotorExecutor_GetBuildSnapshot(now).reserved_ms==reserved2); /* not cooled since first STOP */
 assert(MotorExecutor_Initialize()==MOTOR_RESULT_OK); /* reboot must impose full OFF qualification */
 assert(MotorExecutor_GetBuildSnapshot(now).inhibited && MotorExecutor_BeginBuild(now,&token)!=MOTOR_RESULT_OK); off();
 build_start(250,0);
 for (unsigned i=0;i<170 && machine.state!=FAULT;i++) build_sample(0,100);
 off(); assert(machine.fault_detail==FAULT_DETAIL_BUILD_EXPOSURE);
 assert(MotorExecutor_GetBuildSnapshot(now).approach_reserved_ms<8000);
 assert(MotorExecutor_GetBuildSnapshot(now).approach_command_ms<=40000000);
 assert(MotorExecutor_GetBuildSnapshot(now).approach_command_ms+850000>40000000);
}
static void TestOldPulseBoostResetAndFreshGate(void)
{
 build_start(250,10); build_sample(10,50);
 assert(machine.servo.build.low_count==1 && machine.servo.build.boost==0);
 build_sample(10,50); assert(machine.servo.build.low_count==0 && machine.servo.build.boost==300);
 build_sample(10,50); assert(machine.servo.build.low_count==1 && machine.servo.build.boost==300);
 uint64_t duplicate=seq; build_advance(40); sample_at(12,duplicate,now,true);
 assert(machine.servo.build.low_count==1 && machine.servo.build.boost==300); preload(300);
 sample_at(12,++seq,now-21,true);
 assert(machine.servo.build.low_count==1 && machine.servo.build.boost==300); off();
 assert(machine.state==FAULT); sample_at(12,++seq,now,true); off();
 assert(machine.servo.build.boost==300); /* invalid feedback fault cannot earn/reset compensation */
 build_start(250,10); build_sample(10,50); build_sample(10,50); build_sample(10,50);
 build_sample(12,50);
 assert(machine.servo.build.low_count==0 && machine.servo.build.boost==0);
 build_sample(12,50); build_sample(12,50); assert(machine.servo.build.boost==300);
 build_sample(11,50); assert(machine.servo.build.boost==0); /* absolute movement, not net progress */
 assert(machine.servo.build.progress_anchor==12);
 build_start(250,248);
 for (unsigned i=0;i<8;i++) build_sample(248,50);
 assert(machine.servo.build.boost==1000 && MotorExecutor_GetBuildSnapshot(now).command==1700);
 build_sample(249,50); assert(machine.servo.build.boost==0 && MotorExecutor_GetBuildSnapshot(now).command==400);
 build_sample(250,1); off(); assert(machine.state==FORCE_TARGET_REACHED_OFF);
 ForceBuildState b={0};
 for (unsigned i=0;i<40;i++) ForceBuild_PulseBoost(&b,0,1000);
 assert(b.boost==1000 && b.low_count==0);
 ForceBuild_PulseBoost(&b,-1,1000); assert(b.boost==0 && b.low_count==0);
}
static void TestOldCoarseTimingCeilingAndContactLatch(void)
{
 ForceBuildState b={0};
 ForceBuild_CoarseBoost(&b,0,199); assert(b.coarse_boost==0);
 ForceBuild_CoarseBoost(&b,0,200); assert(b.coarse_boost==500);
 ForceBuild_CoarseBoost(&b,0,399); assert(b.coarse_boost==500);
 for (unsigned ms=400;ms<=2000;ms+=200) ForceBuild_CoarseBoost(&b,0,ms);
 assert(b.coarse_boost==3500);
 ForceBuild_CoarseBoost(&b,1,2200); assert(b.coarse_boost==0);
 build_start(250,0);
 for (unsigned i=0;i<20;i++) build_sample(0,100);
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 assert(e.command==8500 && e.hard_ms==100 && e.base_command==5000);
 assert(TIM3->CCR3==1700 && machine.servo.diagnostic.requested_equivalent_V==8.5f);
 uint32_t reserved=e.approach_command_ms;
 build_sample(10,1); off(); assert(machine.state==FORCE_BUILD);
 build_sample(10,30); assert(MotorExecutor_GetSnapshot()->command_mv==3000);
 build_sample(0,50); assert(machine.state==FORCE_BUILD); /* contact never returns to COARSE */
 assert(MotorExecutor_GetBuildSnapshot(now).approach_command_ms==reserved);
}
static void TestBuildStopAllStagesAndNoRestart(void)
{
 const int initial[]={0,10,220,248};
 for (unsigned i=0;i<4;i++) {
     build_start(250,initial[i]); assert(!MotorExecutor_OutputIsDisabled());
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
     uint32_t req=MotorExecutor_GetBuildSnapshot(now).request;
     uint32_t budget=machine.servo.build.no_response_ms;
     float anchor=machine.servo.build.progress_anchor;
     for (unsigned j=0;j<10;j++) { build_sample(initial[i]+2,100); off(); }
     assert(machine.servo.build.no_response_ms==budget && machine.servo.build.progress_anchor==anchor);
     assert(MotorExecutor_GetBuildSnapshot(now).request==req && !machine.servo.active);
     assert(command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 }
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
static void TestInterpulseInitialMicroFineAndNoOff(void)
{
 const int forces[]={10,248};
 for (unsigned i=0;i<2;i++) {
     build_start(250,forces[i]);
     assert(machine.servo.build.preload_command==300);
     uint32_t starts=FakeStm32Hal_GetState()->pwm_start_call_count;
     uint32_t enabled=GPIOB->ODR;
     for (unsigned t=0;t<10;t++) { advance(1); assert(GPIOB->ODR==enabled); }
     preload(300); /* Production TIM5 IRQ handoff, no machine/main polling. */
     assert(FakeStm32Hal_GetState()->pwm_start_call_count==starts);
     assert(machine.servo.build.post_pending);
     build_sample(forces[i],29); preload(300);
     uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
     build_sample(forces[i],1);
     assert(MotorExecutor_GetBuildSnapshot(now).request==request+1);
     assert(FakeStm32Hal_GetState()->pwm_start_call_count==starts); /* no OFF/rearm glitch */
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 }
 const unsigned limits[]={200,600};
 for (unsigned i=0;i<2;i++) {
     uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_TAPER,400,11,400,BUILD_MODE_FINE,limits[i]};
     assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
     executor_wait(10); preload(limits[i]); /* Old FINE can be below600 preload. */
 }
 const unsigned invalid[]={0,199,201,601,700};
 for (unsigned i=0;i<5;i++) {
     uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_BUILD,3000,11,3000,BUILD_MODE_MICRO,invalid[i]};
     assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
 }
}
static void TestInterpulseDroopFreshGateAndPersistence(void)
{
 build_start(500,100); build_sample(98,50); assert(machine.servo.build.preload_command==300); /* exactly2 */
 const int values[]={95,92,89,86}; const unsigned expected[]={400,500,600,600};
 for (unsigned i=0;i<4;i++) {
     build_sample(values[i],50); assert(machine.servo.build.preload_command==expected[i]);
     build_advance(10); preload(expected[i]);
 }
 assert(machine.servo.build.progress_anchor==100); /* falling force earns no safety progress */
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); build_sample(0,100); build_advance(5100);
 sample_at(0,++seq,now,true); build_plan(500); assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
 build_sample(10,5); assert(machine.servo.build.preload_command==600); build_advance(10); preload(600);
 build_start(250,249); build_sample(246,40);
 assert(machine.servo.build.preload_command==400); /* completed FINE uses identical droop rule */
 build_advance(10); preload(400);
 build_start(500,100); build_advance(10); preload(300);
 uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
 sample_at(90,seq,now,true); preload(300); /* duplicate */
 build_sample(90,29); preload(300); /* ordered but before30 ms cannot adapt */
 assert(machine.servo.build.preload_command==300 && MotorExecutor_GetBuildSnapshot(now).request==request);
 build_sample(97,1); assert(machine.servo.build.preload_command==400);
 build_advance(10); preload(400);
 assert(machine.servo.diagnostic.interpulse_active && machine.servo.diagnostic.interpulse_command==400);
 assert(machine.servo.diagnostic.pulse_force_before==100 && machine.servo.diagnostic.pulse_force_after==97);
}
static void TestInterpulseStopFaultTargetAndLease(void)
{
 for (unsigned during=0;during<2;during++) for (unsigned cause=0;cause<5;cause++) {
     build_start(250,10); if (during) { build_advance(10); preload(300); }
     uint32_t request=MotorExecutor_GetBuildSnapshot(now).request;
     if (cause==0) assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);
     else if (cause==1) sample_at(10,++seq,now,false);
     else if (cause==2) sample_at(3001,++seq,now,true);
     else if (cause==3) sample_at(250,++seq,now,true);
     else sample_at(10,++seq,now-21,true);
     off();
     for (unsigned j=0;j<5;j++) { build_sample(0,100); off(); }
     assert(MotorExecutor_GetBuildSnapshot(now).request==request);
     if (cause==3) assert(machine.state==FORCE_TARGET_REACHED_OFF && !machine.servo.ever_held);
     else assert(!machine.servo.active);
 }
 build_start(250,10); build_advance(126); off(); assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT);
 uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_BUILD,3000,11,3000,BUILD_MODE_MICRO,300};
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now-20,now)==MOTOR_RESULT_OK);
 for (unsigned i=0;i<10;i++) advance(1);
 preload(300); uint32_t lease=MotorExecutor_GetBuildSnapshot(now).receive_deadline_ms;
 while (now+1<lease) advance(1); /* ISR alone stops at receive+130 minus guard. */
 off(); assert(!MotorExecutor_BuildOwnerValid(token));
 assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
}
static void TestInterpulseEnergyDeadlineWithoutMain(void)
{
 uint32_t token=build_owner(); ForceBuildRequest r={BUILD_PHASE_BUILD,3000,11,3000,BUILD_MODE_MICRO,300};
 for (unsigned i=0;i<400;i++) {
     assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)==MOTOR_RESULT_OK);
     executor_wait(10); preload(300); MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
     if (e.preload_deadline_ms!=e.receive_deadline_ms) {
         assert(e.reserved_ms==12000 && e.preload_deadline_ms<e.receive_deadline_ms);
         while (now+1<e.preload_deadline_ms) advance(1);
         off(); e=MotorExecutor_GetBuildSnapshot(now);
         assert(e.inhibited && e.energized_upper_ms<=12000 && e.preload_spent_ms>8000);
         assert(!MotorExecutor_BuildOwnerValid(token));
         assert(MotorExecutor_StartBuildSegment(token,&r,++seq,now,now)!=MOTOR_RESULT_OK); off();
         return;
     }
     executor_wait(30);
     assert(MotorExecutor_AcceptBuildPost(token,e.request,++seq,now,now));
 }
 assert(!"Independent prepaid exposure cutoff was never exercised");
}
static void interpulse_pending_hard(void) { TIM5->SR|=TIM_SR_UIF; }
static void interpulse_pending_overcapture(void) { TIM5->SR|=TIM_SR_CC1OF; }
static void TestInterpulseRacingCutoffAndNoReverse(void)
{
 void (*hooks[])(void)={stop_before_commit,interpulse_pending_hard,interpulse_pending_overcapture};
 for (unsigned i=0;i<3;i++) {
     build_start(250,10); build_advance(9); dsb_hook=hooks[i]; advance(1); off();
     assert(!MotorExecutor_BuildOwnerValid(machine.servo.token));
     Machine_Tick(&machine,now); off();
 }
 for (unsigned i=0;i<3;i++) {
     build_start(250,10); build_advance(39); preload(300);
     dsb_hook=hooks[i]; build_sample(12,1); off();
     assert(machine.state==FAULT || !machine.servo.active);
 }
 build_start(250,10); build_advance(10); preload(300);
 assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,100,10,40,now)!=MOTOR_RESULT_OK);
 preload(300); /* No reverse owner accepted; no direction mutation. */
 build_sample(251,1); off(); /* negative error is true OFF, never forward preload/RELEASE */
}
int main(void)
{
 setvbuf(stdout,NULL,_IONBF,0);
#define RUN(f) f(); puts(#f " PASS")
 RUN(TestBuildApproachAndLowTargets); RUN(TestBuildSyntheticTargetsOffAndDecay);
 RUN(TestBuildSlowProgressAndShortPlateau); RUN(TestBuildNoiseAndPersistentBudgets);
 RUN(TestBuildSecondStartProgressAndPlatform); RUN(TestBuildStartAnchorRequiresFreshFrame);
 RUN(TestBuildRestartNoiseBadFramesAndStop);
 RUN(TestBuildContractAndPostFeedback); RUN(TestBuildCutoffsAndStopRaces);
 RUN(TestBuildMissingBadFeedbackAndTargetPriority); RUN(TestBuildBoostAndTaperEnergy);
 RUN(TestBuildExposureAndUninterruptedCooling); RUN(TestBuildPlanAndReadbackContract);
 RUN(TestOldPulseBoostResetAndFreshGate); RUN(TestOldCoarseTimingCeilingAndContactLatch);
 RUN(TestBuildStopAllStagesAndNoRestart);
 RUN(TestInterpulseInitialMicroFineAndNoOff); RUN(TestInterpulseDroopFreshGateAndPersistence);
 RUN(TestInterpulseEnergyDeadlineWithoutMain); RUN(TestInterpulseStopFaultTargetAndLease); RUN(TestInterpulseRacingCutoffAndNoReverse);
 RUN(TestTrajectory); RUN(TestNumericAndD); RUN(TestAntiWindup);
 puts("BUILD_TO_TARGET_GROUPS=24 PASS; SYNTHETIC_ONLY; PHYSICAL_NOT_RUN"); return 0;
}
