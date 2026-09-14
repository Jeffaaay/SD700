#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Application/runtime.h"
#include "Application/auto_target_config.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_stop_timer.h"
#include "Board/Motor/motor_hw_real.h"
#include "Application/motion_build_policy.h"
#include "Tests/Host/fake_stm32_hal.h"
#include "Transport/Modbus/force_servo_protocol.h"
#include "Transport/Modbus/modbus_semantic_map.h"
#include "Transport/Modbus/modbus_rtu_server.h"
#include "Protocol/Modbus/modbus_crc16.h"
static unsigned critical;
static void (*enter_hook)(void), (*dsb_hook)(void);
static MachineContext machine;
static uint32_t now;
static uint64_t seq;
uint32_t MotorAtomic_Enter(void)
{
 if (!critical && enter_hook) { void (*h)(void)=enter_hook; enter_hook=NULL; h(); }
 return critical++;
}
void MotorAtomic_Leave(uint32_t saved)
{
 critical=saved;
 if (!critical && MotorStopTimer_IsArmed() && FakeStm32Hal_GetState()->tim5_irq_enabled &&
     (TIM5->SR & (TIM_SR_CC1IF|TIM_SR_CC1OF|TIM_SR_UIF))) MotorStopTimer_IrqHandler();
}
uint32_t MotorAtomic_Now(uint32_t supplied) { return supplied; }
void FakeForceServo_Dsb(void)
{ if (dsb_hook) { void (*h)(void)=dsb_hook; dsb_hook=NULL; h(); } }
static void advance(uint32_t ms)
{
 now+=ms;
 if (MotorStopTimer_IsArmed()) {
     uint32_t old=TIM5->CNT; TIM5->CNT+=ms*10;
     if ((int32_t)(old-TIM5->CCR1)<0 && (int32_t)(TIM5->CNT-TIM5->CCR1)>=0) {
         TIM5->SR=TIM_SR_CC1IF;
         if (!critical) MotorStopTimer_IrqHandler();
     }
 }
}
static MachineCommandResult command(MachineCommandType type,int target)
{ MachineCommand c={type,target}; return Machine_HandleCommand(&machine,&c,now); }
static void sample_at(int raw,uint64_t sequence,uint32_t received,bool valid)
{
 MachinePressureSample p={0}; p.sequence=sequence; p.received_at_ms=received;
 p.raw_pressure_counts=(uint32_t)raw; p.control_pressure_units=raw;
 p.frame_valid=valid; p.control_units_valid=valid;
 Machine_HandlePressureSample(&machine,&p,now);
}
static void sample(int raw,uint32_t ms)
{ advance(ms); sample_at(raw,++seq,now,true); }
static void fixture(void)
{
 critical=0; enter_hook=NULL; dsb_hook=NULL; now=100; seq=1;
 FakeStm32Hal_Reset(); assert(MotorExecutor_Initialize()==MOTOR_RESULT_OK);
 Machine_Initialize(&machine,&g_sd700_auto_target_machine_config,now);
 /* Historical controller/long synthetic regressions use an explicit count
  * fixture envelope, NOT the live five-second profile. New Static tests below
  * restore g_force_servo_profile for all production-envelope checks. */
 machine.servo.profile.id=1; machine.servo.profile.peak_press=0;
 machine.servo.profile.boost_ms=0; machine.servo.profile.boost_total_ms=0; machine.servo.profile.taper_margin=0;
 machine.servo.profile.energized_ms=60000; machine.servo.profile.session_ms=60000;
 machine.servo.profile.build_ms=30000; machine.servo.profile.capture_ms=60000;
 Machine_CompleteBoot(&machine,true,now); assert(machine.state==IDLE);
 sample_at(30,seq,now,true);
}
static void start(int target)
{
 assert(command(CMD_SET_TARGET,target)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED);
#if SD700_FORCE_SERVO_COMMISSIONING
 assert(machine.servo.start_pending); sample(30,5);
#endif
 assert(machine.state==FORCE_BUILD);
}
static void off(void) { FakeStm32Hal_AssertMotorDisabled(); }
static void TestTrajectory(void)
{
 ForceServo s; ForceServoStep o; ForceServoConfig c=g_force_servo_default_config;
 int targets[]={1,20,60,137,250,275};
 for (unsigned j=0;j<sizeof(targets)/sizeof(targets[0]);j++) {
     assert(ForceServo_Init(&s,&c,30,(float)targets[j]));
     float previous_rate=0, previous_ref=30;
     for (unsigned k=0;k<4000;k++) {
         assert(ForceServo_Prepare(&s,&c,30,0.01f,&o));
         assert(fabsf(o.reference_rate)<=c.reference_rate+0.001f);
         assert(fabsf(o.reference_rate-previous_rate)<=c.reference_acceleration*0.01f+0.001f);
         assert(fabsf(o.reference-previous_ref)<=c.reference_rate*0.01f+0.002f);
         assert(o.reference>=fminf(30,(float)targets[j])-0.001f);
         assert(o.reference<=fmaxf(30,(float)targets[j])+0.001f);
         previous_rate=o.reference_rate; previous_ref=o.reference;
         assert(ForceServo_Commit(&s,&c,&o,(int)o.limited_output));
     }
     assert(fabsf(s.reference-(float)targets[j])<0.001f);
 }
 assert(ForceServo_Init(&s,&c,30,3000)); /* Arithmetic is unit-agnostic; machine profile gates live targets. */ assert(!ForceServo_Init(&s,&c,30,0));
}
static void TestNumericAndD(void)
{
 ForceServo s; ForceServoStep o; ForceServoConfig c=g_force_servo_default_config;
 c.kd=2; assert(ForceServo_Init(&s,&c,30,250));
 for (int j=0;j<50;j++) { assert(ForceServo_Prepare(&s,&c,30,0.01f,&o)); assert(o.d==0); }
 /* Target reference changes without measurement movement: no derivative kick. */
 s.reference=100; assert(ForceServo_Prepare(&s,&c,30,0.01f,&o)); assert(o.d==0);
 assert(ForceServo_Prepare(&s,&c,31,0.01f,&o)); assert(o.d<0 && fabsf(o.d)<200);
 c.kd=0; assert(ForceServo_Prepare(&s,&c,29,0.02f,&o)); assert(o.d==0);
 assert(!ForceServo_Prepare(&s,&c,30,NAN,&o));
 assert(!ForceServo_Prepare(&s,&c,30,c.feedback_gap_ms*.001f+.01f,&o));
 assert(!ForceServo_Prepare(&s,&c,30,0,&o));
 assert(!ForceServo_Prepare(&s,&c,INFINITY,0.01f,&o));
 c.kp=NAN; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.lease_ms=c.sample_age_ms; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.press_cap=FS_PRESS_PROFILE_CEILING+1; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.release_cap=800; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.ki=INFINITY; assert(!ForceServo_ConfigValid(&c));
}
static int drive_with_gains(float kp,float ki)
{
 fixture(); machine.servo.config.kp=kp; machine.servo.config.ki=ki;
 machine.servo.config.output_rate=100000;
 start(80);
 for (int j=0;j<200;j++) { sample(30,10); assert(machine.state==FORCE_BUILD); }
 assert(MotorExecutor_GetSnapshot()->last_action==MOTOR_ACTION_CONTINUOUS);
 assert(MotorExecutor_GetSnapshot()->planned_tim3_ccr3==TIM3->CCR3);
 assert(machine.force_pi_enabled==false);
 int pwm=(int)TIM3->CCR3; assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); return pwm;
}
static void TestFinalPwmGains(void)
{ int p=drive_with_gains(1,0); assert(drive_with_gains(2,0)>p); assert(drive_with_gains(1,2)>p); }
static void TestAntiWindup(void)
{
 ForceServo s; ForceServoStep o; ForceServoConfig c=g_force_servo_default_config;
 c.kp=100; c.ki=5; c.output_rate=100; assert(ForceServo_Init(&s,&c,30,250));
 for (int j=0;j<2000;j++) {
     assert(ForceServo_Prepare(&s,&c,30,0.01f,&o));
     float old=s.integral;
     int committed=j<1000 ? 0 : (int)o.limited_output; /* direction interlock/derating zero */
     if (j<1000) o.limits|=FS_LIMIT_INTERLOCK;
     float expected=fminf(c.integral_max,fmaxf(c.integral_min,
         old+0.01f*(c.ki*o.error+c.tracking_gain*(committed-o.raw_output))));
     assert(ForceServo_Commit(&s,&c,&o,committed));
     assert(fabsf(s.integral-expected)<0.005f);
     assert(fabsf(o.limited_output-s.previous_committed)<=c.output_rate*0.01f+1.01f);
 }
 assert(s.integral>=c.integral_min && s.integral<=c.integral_max);
 assert(ForceServo_Prepare(&s,&c,300,0.01f,&o));
 assert(o.limits&FS_LIMIT_AMPLITUDE); assert(o.limits&FS_LIMIT_RATE);
}
static void TestHoldAndContactLost(void)
{
 fixture(); machine.servo.config.kp=20; machine.servo.config.ki=1;
 machine.servo.config.output_rate=100000; start(30);
 sample(28,10); assert(machine.state==FORCE_HOLD);
 float integral=machine.servo.controller.integral;
 uint32_t starts=FakeStm32Hal_GetState()->pwm_start_call_count;
 sample(28,10); assert(machine.state==FORCE_HOLD); assert(TIM3->CCR3>0);
 assert(FakeStm32Hal_GetState()->pwm_start_call_count==starts);
 assert(machine.servo.controller.integral>=integral);
 sample(42,10); assert(machine.state==FORCE_BUILD); off(); /* reversing OFF */
 sample(42,10); assert(TIM2->CCR3>0 && TIM3->CCR3==0);
 sample(30,10); assert(machine.state==FORCE_HOLD);
 sample(19,10); assert(machine.state==FAULT && machine.fault_detail==FAULT_DETAIL_CONTACT_LOST);
 assert(machine.servo.diagnostic.contact_lost_count==1); off();
 sample(0,10); Machine_Tick(&machine,now); off();
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); assert(machine.state==FAULT);
}
static uint32_t continuous(void)
{
 fixture(); uint32_t token; assert(MotorExecutor_BeginContinuous(&token)==MOTOR_RESULT_OK); return token;
}
static MotorResult update(uint32_t token,int32_t requested,int32_t *actual,bool *interlock)
{ return MotorExecutor_UpdateContinuous(token,++seq,now,now,50,20,2,requested,actual,interlock); }
#if SD700_FORCE_SERVO_COMMISSIONING
#define TEST_OUTPUT_A 40
#define TEST_OUTPUT_B 80
#define TEST_OUTPUT_C 100
#define TEST_OUTPUT_INVALID (FS_PRESS_PROFILE_CEILING+1)
#else
#define TEST_OUTPUT_A 200
#define TEST_OUTPUT_B 250
#define TEST_OUTPUT_C 300
#define TEST_OUTPUT_INVALID 5001
#endif
static void TestExecutorUpdates(void)
{
 uint32_t token=continuous(); int32_t actual; bool interlock;
 assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK); assert(actual==TEST_OUTPUT_A);
 uint32_t starts=FakeStm32Hal_GetState()->pwm_start_call_count;
 advance(10); assert(update(token,TEST_OUTPUT_B,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(FakeStm32Hal_GetState()->pwm_start_call_count==starts);
 assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
 assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,100,10,40,now)==MOTOR_RESULT_BUSY);
 advance(10); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK); assert(actual==0 && interlock); off();
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK); assert(actual==0 && interlock);
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK); assert(actual==-100);
 assert(update(token,0,&actual,&interlock)==MOTOR_RESULT_OK); off();
 assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK && MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
 assert(update(token,TEST_OUTPUT_INVALID,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 assert(update(token,1,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void pending_expiry(void) { TIM5->SR=TIM_SR_CC1IF; }
static void stop_before_commit(void) { assert(MotorExecutor_Disable()==MOTOR_RESULT_OK); }
static void TestLeaseRaces(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 uint32_t deadline=MotorExecutor_GetSnapshot()->logical_deadline_ms;
 assert(MotorExecutor_UpdateContinuous(token,seq,now,now,50,20,2,TEST_OUTPUT_C,&actual,&interlock)==MOTOR_RESULT_INVALID);
 assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==deadline);
 advance(10); dsb_hook=pending_expiry;
 assert(update(token,TEST_OUTPUT_C,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 assert(update(token,TEST_OUTPUT_C,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK);
 assert(MotorExecutor_GetSnapshot()->last_completion==MOTOR_COMPLETION_LEASE);
 token=continuous(); assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 enter_hook=stop_before_commit;
 assert(update(token,TEST_OUTPUT_C,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 uint32_t new_token; assert(MotorExecutor_BeginContinuous(&new_token)==MOTOR_RESULT_OK);
 assert(new_token!=token); assert(update(token,TEST_OUTPUT_C,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 assert(update(new_token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 /* Simulate main/control/telemetry completely stalled; TIM5 IRQ alone shuts down. */
 advance(50); off(); assert(!MotorStopTimer_IsArmed());
 assert(update(new_token,TEST_OUTPUT_A,&actual,&interlock)!=MOTOR_RESULT_OK);
 token=continuous(); assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 TIM5->SR=TIM_SR_CC1IF; /* IRQ pending before renewal; must not clear it */
 assert(update(token,TEST_OUTPUT_A,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void TestSampleAgeAndWrap(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 now=UINT32_MAX-20; seq=UINT64_MAX-1;
 assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 advance(10); assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK); assert(seq==0);
 advance(15); assert(update(token,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK); assert(now==4);
 assert(MotorExecutor_UpdateContinuous(token,seq+1,now-21,now,50,20,2,TEST_OUTPUT_A,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 token=continuous();
 assert(MotorExecutor_UpdateContinuous(token,++seq,now-15,now,50,20,2,TEST_OUTPUT_A,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==now+35);
 advance(35); off();
 fixture(); now=UINT32_MAX-20; seq=UINT64_MAX-1;
 machine.servo.have_sample=false; sample_at(30,seq,now,true); start(80);
 sample(30,10); sample(30,10); sample(30,10); assert(machine.state==FORCE_BUILD);
 assert(machine.servo.diagnostic.control_sequence==3);
}
static void TestFeedbackFaults(void)
{
 fixture(); start(80); sample(30,10); uint32_t deadline=MotorExecutor_GetSnapshot()->logical_deadline_ms;
 sample_at(30,seq,now,true); assert(MotorExecutor_GetSnapshot()->logical_deadline_ms==deadline);
 sample_at(30,seq-1,now,true); assert(machine.state==FAULT); off();
 fixture(); start(80); sample_at(30,++seq,now,false); assert(machine.fault_detail==FAULT_DETAIL_PRESSURE_INVALID); off();
 fixture(); start(80); advance((uint32_t)machine.servo.config.feedback_gap_ms+1); Machine_CheckPressureSafety(&machine,now);
 assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT); off();
 fixture(); machine.servo.config.measurement_filter_s=1; start(80); sample(325,10);
 assert(machine.fault==FAULT_OVERPRESSURE); off();
 fixture(); start(80); sample(30,10); TIM2->CCR3=5;
 assert(MotorExecutor_GuardOutput()!=MOTOR_RESULT_OK); off();
 fixture(); start(80); FakeStm32Hal_GetState()->fail_start_instance=TIM3;
 for (int j=0;j<100 && machine.state!=FAULT;j++) sample(30,10);
 assert(machine.state==FAULT); off();
}
static void TestDeadlinesAndApproach(void)
{
 fixture(); machine.servo.config.tracking_error=275; machine.servo.config.kp=0;
 start(250);
 for (int j=0;j<2999;j++) { sample(30,10); assert(machine.state==FORCE_BUILD); }
 sample(30,10); assert(machine.fault==FAULT_MOTION_TIMEOUT && machine.fault_detail==FAULT_DETAIL_CYCLE_TIMEOUT); off();
 fixture(); machine.servo.config.session_ms=2000; machine.servo.config.saturation_ms=1000;
 machine.servo.config.tracking_ms=1000; start(30);
 for (int j=0;j<199;j++) sample(j%2 ? 30 : 42,10);
 sample(30,10); assert(machine.fault_detail==FAULT_DETAIL_SESSION_TIMEOUT); off();
#if SD700_FORCE_SERVO_COMMISSIONING
 fixture(); sample(0,10); assert(command(CMD_SET_TARGET,40)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); Machine_Tick(&machine,now); off();
 sample(0,5); assert(machine.state==FORCE_BUILD); off();
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);
 assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,10000,20,50,now)==MOTOR_RESULT_INVALID);
 assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,100,10,40,now)==MOTOR_RESULT_INVALID); off();
#else
 fixture(); sample(0,10); assert(command(CMD_SET_TARGET,250)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); assert(machine.state==FORCE_APPROACH);
 Machine_Tick(&machine,now); assert(MotorExecutor_GetSnapshot()->command_mv==10000);
 uint32_t start_time=now;
 advance(20); sample_at(0,++seq,now,true);
 assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK); Machine_HandleMotorService(&machine,now);
 assert(machine.state==FORCE_APPROACH); off();
 for (int j=0;j<6;j++) sample(0,10);
 Machine_Tick(&machine,now); assert(MotorExecutor_GetSnapshot()->command_mv==10000);
 sample(25,10); assert(machine.state==FORCE_BUILD);
 assert(machine.servo.contact_at_ms>start_time); assert(machine.servo.controller.previous_committed==0); off();
 sample(19,10); assert(machine.fault_detail==FAULT_DETAIL_CONTACT_LOST); off();
 /* Initial approach backstop is still fatal. */
 fixture(); sample(0,10); assert(command(CMD_SET_TARGET,80)==COMMAND_ACCEPTED);
 assert(command(CMD_FORCE_START,0)==COMMAND_ACCEPTED); Machine_Tick(&machine,now);
 TIM5->SR=TIM_SR_UIF; MotorStopTimer_IrqHandler(); assert(MotorExecutor_Service(now)==MOTOR_RESULT_OK);
 Machine_HandleMotorService(&machine,now); assert(machine.state==FAULT); off();
#endif
}
static void TestBoundedTracking(void)
{
 fixture(); machine.servo.config.kp=100; machine.servo.config.press_cap=5;
 machine.servo.config.saturation_ms=100; start(250);
 for (int i=0;i<4000 && machine.state!=FAULT;i++) sample(30,10);
 assert(machine.fault_detail==FAULT_DETAIL_SATURATION_TIMEOUT); off();
 fixture(); machine.servo.config.kp=0; machine.servo.config.tracking_error=1;
 machine.servo.config.tracking_ms=100; start(250);
 for (int i=0;i<4000 && machine.state!=FAULT;i++) sample(30,10);
 assert(machine.fault_detail==FAULT_DETAIL_TRACKING_TIMEOUT); off();
}
static MachineCommandResult write_reg(uint16_t a,uint16_t v)
{ return ModbusSemantic_ApplyWrite(&machine,6,a,v,now); }
static void stage(const ForceServoConfig *c)
{
 assert(write_reg(FS_REG_BEGIN,0xB101)==COMMAND_ACCEPTED);
 for (unsigned j=0;j<sizeof(*c)/4;j++) {
     uint32_t u; memcpy(&u,(const unsigned char*)c+j*4,4);
     assert(write_reg((uint16_t)(FS_REG_CONFIG+j*2),(uint16_t)(u>>16))==COMMAND_ACCEPTED);
     assert(write_reg((uint16_t)(FS_REG_CONFIG+j*2+1),(uint16_t)u)==COMMAND_ACCEPTED);
 }
}
static void TestConfigAndSnapshot(void)
{
 fixture(); ForceServoConfig old=machine.servo.config,c=old;
 c.ki=2; stage(&c); assert(memcmp(&old,&machine.servo.config,sizeof(c))==0);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_ACCEPTED);
 assert(machine.servo.config.ki==2 && machine.servo.config_version==2); off();
 old=machine.servo.config; c.ki=NAN; stage(&c);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE);
 assert(memcmp(&old,&machine.servo.config,sizeof(c))==0);
 assert(write_reg(FS_REG_BEGIN,0xB101)==COMMAND_ACCEPTED);
 assert(write_reg(FS_REG_COMMIT,0xC101)==COMMAND_INVALID_VALUE);
 start(80); assert(write_reg(FS_REG_BEGIN,0xB101)==COMMAND_BUSY);
 sample(30,10); assert(write_reg(FS_REG_SNAPSHOT,0xD101)==COMMAND_ACCEPTED);
 uint16_t frozen[FORCE_SERVO_DIAG_WORDS]; memcpy(frozen,machine.servo.frozen,sizeof(frozen));
 sample(31,10); assert(memcmp(frozen,machine.servo.frozen,sizeof(frozen))==0);
 for (unsigned i=0;i<FORCE_SERVO_DIAG_WORDS;i++) {
     uint16_t word; assert(ModbusSemantic_ReadInput(&machine,(uint16_t)(FS_INPUT_SNAPSHOT+i),now,&word));
     assert(word==frozen[i]);
 }
 assert(machine.servo.diagnostic.p+machine.servo.diagnostic.i+machine.servo.diagnostic.d==
        machine.servo.diagnostic.raw_output);
 assert(command(CMD_AUTO_START,0)==COMMAND_UNSUPPORTED);
 assert(command(CMD_DIRECT_PRESS_PULSE,0)==COMMAND_UNSUPPORTED);
}
static void request(ModbusRtuServer *server,uint8_t f,uint16_t a,uint16_t v)
{
 uint8_t b[8]={1,f,(uint8_t)(a>>8),(uint8_t)a,(uint8_t)(v>>8),(uint8_t)v,0,0};
 uint16_t crc=Modbus_Crc16(b,6); b[6]=(uint8_t)crc; b[7]=(uint8_t)(crc>>8);
 ModbusRtuServer_ProcessBytes(server,b,8);
}
static void TestModbusAndCongestion(void)
{
 fixture(); ModbusRtuServer server; ModbusRtuServer_Initialize(&server,&machine);
 request(&server,6,FS_REG_BEGIN,0xB101); assert(ModbusRtuServer_ProcessPending(&server,now));
 size_t length; const uint8_t *r=ModbusRtuServer_GetResponse(&server,&length); assert(length==8 && r[1]==6);
 ModbusRtuServer_CompleteResponse(&server); start(30); sample(28,10);
 for (int j=0;j<10;j++) request(&server,6,FS_REG_SNAPSHOT,0xD101);
 request(&server,5,1,0); assert(ModbusRtuServer_TakeStop(&server,now)); off(); assert(machine.state==IDLE);
 assert(ModbusRtuServer_GetSnapshot(&server)->normal_queue_full_drop_count>0);
 /* Pending telemetry never renews the lease when main stops. */
 fixture(); start(80); sample(30,10); advance((uint32_t)machine.servo.config.lease_ms); off();
}
static void TestRuntimeDeliveryAndLock(void)
{
#if SD700_FORCE_SERVO_COMMISSIONING
    fixture(); assert(MotorHwReal_OutputArmingAllowed());
#else
    fixture(); FakeStm32Hal_GetState()->output_arming_allowed=false;
    assert(command(CMD_SET_TARGET,250)==COMMAND_ACCEPTED);
    assert(command(CMD_FORCE_START,0)==COMMAND_NOT_READY); off();
#endif
    fixture(); ApplicationRuntime runtime;
    ApplicationRuntime_Initialize(&runtime,&g_sd700_auto_target_machine_config,now);
    ApplicationRuntime_CompleteBoot(&runtime,true,now);
    PressureReceiverSnapshot p={0}; p.raw_pressure_counts=30;
    p.sample_sequence=UINT64_MAX; p.received_at_ms=now;
    assert(ApplicationRuntime_ServicePressure(&runtime,&p,now));
    MachineCommand c={CMD_SET_TARGET,80};
    assert(Machine_HandleCommand(&runtime.machine,&c,now)==COMMAND_ACCEPTED);
    c.type=CMD_FORCE_START;
    assert(Machine_HandleCommand(&runtime.machine,&c,now)==COMMAND_ACCEPTED);
    advance(10); p.sample_sequence=0; p.received_at_ms=now;
    assert(ApplicationRuntime_ServicePressure(&runtime,&p,now));
    assert(runtime.machine.servo.diagnostic.control_sequence==(SD700_FORCE_SERVO_COMMISSIONING ? 0U : 1U));
    assert(!ApplicationRuntime_ServicePressure(&runtime,&p,now));
    assert(runtime.machine.servo.diagnostic.control_sequence==(SD700_FORCE_SERVO_COMMISSIONING ? 0U : 1U));
    p.sample_sequence=UINT64_MAX-1;
    assert(ApplicationRuntime_ServicePressure(&runtime,&p,now));
    assert(runtime.machine.fault_detail==FAULT_DETAIL_PRESSURE_ORDER_LOST); off();
    fixture(); uint32_t token;
    assert(MotorExecutor_BeginContinuous(&token)==MOTOR_RESULT_OK);
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,100,10,40,now)==MOTOR_RESULT_BUSY);
    off();
}
static void TestSyntheticPlants(void)
{
 for (int plant=0;plant<4;plant++) {
     fixture(); machine.servo.config.kp=3; machine.servo.config.ki=2;
     machine.servo.config.tracking_error=275; machine.servo.config.saturation_ms=30000;
     machine.servo.config.output_rate=500; start(60);
     float pressure=30, velocity=0; int hold_updates=0;
     for (int j=0;j<2000;j++) {
         float gain=plant<2 ? 0.5f : 1.0f, tau=plant%2 ? 0.3f : 0.05f;
         float u=machine.servo.diagnostic.current_committed;
         velocity+=0.01f/(tau+0.01f)*(gain*u-velocity);
         pressure+=velocity*0.01f;
         if (j==1000) pressure-=3; /* known SYNTHETIC hold disturbance */
         float noise=plant==3 ? (j%2 ? 0.5f : -0.5f) : 0;
         sample((int)roundf(fminf(324,fmaxf(20,pressure+noise))),10);
         assert(machine.state!=FAULT);
         assert(fabsf(machine.servo.controller.integral)<=1000);
         if (machine.state==FORCE_HOLD) hold_updates++;
     }
     assert(hold_updates>10);
     printf("SYNTHETIC plant=%d final=%.3f hold_updates=%d (NOT mechanical identification)\n",plant,(double)pressure,hold_updates);
     assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off();
 }
}
#if SD700_FORCE_SERVO_COMMISSIONING
static void TestCommissioningEnable(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(MotorHwReal_OutputArmingAllowed()); off();
 assert(update(token,40,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==40 && TIM3->CCR3>0 && TIM2->CCR3==0 && MotorStopTimer_IsArmed());
 assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
 ForceServoConfig c=g_force_servo_default_config;
 assert(c.kp==10 && c.press_cap==720 && c.release_cap==100);
 c.press_cap=FS_PRESS_PROFILE_CEILING+1; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.release_cap=FS_RELEASE_PROFILE_CEILING+1; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.lease_ms=131; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.feedback_gap_ms=126; assert(!ForceServo_ConfigValid(&c));
 c=g_force_servo_default_config; c.sample_age_ms=21; assert(!ForceServo_ConfigValid(&c));
 assert(update(token,FS_PRESS_PROFILE_CEILING+1,&actual,&interlock)==MOTOR_RESULT_HARDWARE_ERROR); off();
 token=continuous(); assert(update(token,-FS_RELEASE_PROFILE_CEILING-1,&actual,&interlock)==MOTOR_RESULT_HARDWARE_ERROR); off();
}
static void TestCommissioningSameDirection(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,40,&actual,&interlock)==MOTOR_RESULT_OK);
 uint32_t starts=FakeStm32Hal_GetState()->pwm_start_call_count, compare=TIM3->CCR3;
 advance(10); assert(update(token,80,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==80 && !interlock && TIM3->CCR3>compare);
 assert(FakeStm32Hal_GetState()->pwm_start_call_count==starts);
 assert(MotorExecutor_GuardOutput()==MOTOR_RESULT_OK);
}
static void TestCommissioningStopActive(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK); assert(TIM3->CCR3==20);
 assert(command(CMD_STOP,0)==COMMAND_ACCEPTED); off(); assert(!MotorStopTimer_IsArmed());
 assert(update(token,100,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 /* A STOP concurrent with an update still revokes the active generation. */
 token=continuous(); assert(update(token,40,&actual,&interlock)==MOTOR_RESULT_OK);
 enter_hook=stop_before_commit; assert(update(token,80,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void TestCommissioningFeedbackStop(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 /* Existing TIM5 compare reserves 1 ms: a nominal 50 ms lease expires at 49 ms. */
 advance(48); assert(TIM3->CCR3>0); advance(1); off();
 assert(!MotorStopTimer_IsArmed()); /* TIM5 alone; no main or telemetry service. */
 assert(update(token,40,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 fixture(); start(60);
 for (int i=0;i<100;i++) sample(30,10);
 assert(TIM3->CCR3>0); advance(126); Machine_CheckPressureSafety(&machine,now);
 assert(machine.fault==FAULT_PRESSURE_SENSOR_FAULT); off();
 token=continuous();
 assert(MotorExecutor_UpdateContinuous(token,++seq,now-21,now,50,20,2,40,&actual,&interlock)!=MOTOR_RESULT_OK); off();
 token=continuous();
 assert(MotorExecutor_UpdateContinuous(token,++seq,now,now,131,20,2,40,&actual,&interlock)!=MOTOR_RESULT_OK); off();
}
static void TestCommissioningNoRestart(void)
{
 fixture(); start(60);
 for (int i=0;i<100;i++) sample(30,10);
 assert(TIM3->CCR3>0); assert(command(CMD_STOP,0)==COMMAND_ACCEPTED);
 for (int i=0;i<100;i++) { sample(30,10); Machine_Tick(&machine,now); off(); }
 assert(machine.state==IDLE && !machine.servo.active);
 start(60); sample_at(30,++seq,now,false); assert(machine.state==FAULT); off();
 for (int i=0;i<100;i++) { sample(30,10); Machine_Tick(&machine,now); off(); }
 assert(machine.state==FAULT && command(CMD_FORCE_START,0)!=COMMAND_ACCEPTED);
 assert(command(CMD_FAULT_RESET,0)==COMMAND_ACCEPTED);
 for (int i=0;i<100;i++) { sample(30,10); Machine_Tick(&machine,now); off(); }
 assert(machine.state==IDLE && !machine.servo.active);
}
static void TestCommissioningReverseOff(void)
{
 int32_t actual; bool interlock; uint32_t token=continuous();
 assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 advance(10); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==0 && interlock); off();
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==0 && interlock); off();
 advance(1); assert(update(token,-100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==-100 && TIM2->CCR3>0 && TIM3->CCR3==0);
 advance(10); assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==0 && interlock); off();
 advance(2); assert(update(token,100,&actual,&interlock)==MOTOR_RESULT_OK);
 assert(actual==100 && TIM3->CCR3>0 && TIM2->CCR3==0);
 TIM2->CCR3=1; assert(MotorExecutor_GuardOutput()!=MOTOR_RESULT_OK); off();
}
#endif

#if SD700_FORCE_SERVO_COMMISSIONING
#include "Tests/Host/test_target250_cases.h"
#include "Tests/Host/test_continuous2_cases.h"
#include "Tests/Host/test_authority1_cases.h"
#include "Tests/Host/test_static_force_cases.h"
#include "Tests/Host/test_boost1_cases.h"
#endif
int main(void)
{
 unsigned count=0;
#define RUN(f) f(); ++count; puts(#f " PASS");
 RUN(TestTrajectory) RUN(TestNumericAndD) RUN(TestFinalPwmGains) RUN(TestAntiWindup)
 RUN(TestHoldAndContactLost) RUN(TestExecutorUpdates) RUN(TestLeaseRaces)
 RUN(TestSampleAgeAndWrap) RUN(TestFeedbackFaults) RUN(TestDeadlinesAndApproach)
 RUN(TestBoundedTracking) RUN(TestConfigAndSnapshot) RUN(TestModbusAndCongestion)
 RUN(TestRuntimeDeliveryAndLock) RUN(TestSyntheticPlants)
#if SD700_FORCE_SERVO_COMMISSIONING
 RUN(TestCommissioningEnable) RUN(TestCommissioningSameDirection) RUN(TestCommissioningStopActive)
 RUN(TestCommissioningFeedbackStop) RUN(TestCommissioningNoRestart) RUN(TestCommissioningReverseOff)
 RUN(TestTarget250StartFrame) RUN(TestTarget250PendingCancel) RUN(TestTarget250PendingValidation)
 RUN(TestTarget250BuildHold) RUN(TestTarget250SmallIntegral) RUN(TestTarget250FieldCadence)
 RUN(TestTarget250SyntheticPI)
 RUN(TestContinuous2Range) RUN(TestContinuous2Sweep) RUN(TestContinuous2Ramp)
 RUN(TestContinuous2Pwm) RUN(TestContinuous2Peak) RUN(TestContinuous2OldSaturation)
 RUN(TestAuthority1Sweep) RUN(TestAuthority1Ramp) RUN(TestAuthority1Safety)
 RUN(TestStaticUnitsAndQualification) RUN(TestStaticPlanAndAbsoluteBudget)
 RUN(TestAuthority2LiveEnvelope)
#if FS_SYNTHETIC_BOOST
 RUN(TestBoost1PeakHandoff) RUN(TestBoost1Safety) RUN(TestBoost1AdmissionAndBudget)
 RUN(TestAuthority2PIAndExit) RUN(TestAuthority2WrapAndCooling)
#endif
 RUN(TestStaticProgressNoiseAndCreep) RUN(TestStaticStopStagesAndReadback)
#if FS_SYNTHETIC_BOOST
 RUN(TestStaticBoostDeadlineAndTransfer) RUN(TestStaticBoostBudgetAndStop)
#endif
#endif
 printf("FORCE_SERVO_TEST_GROUPS=%u PASS; physical test NOT RUN\n",count);
 return 0;
}
