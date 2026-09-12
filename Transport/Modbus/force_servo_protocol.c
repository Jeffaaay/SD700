#include "Transport/Modbus/force_servo_protocol.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_atomic.h"
#include <string.h>
_Static_assert(FORCE_SERVO_CONFIG_WORDS<=64,"staging mask capacity");
_Static_assert(sizeof(float)==4,"IEEE754 binary32 transport required");
static void word32(uint16_t *out,unsigned *index,uint32_t v)
{ out[(*index)++]=(uint16_t)(v>>16); out[(*index)++]=(uint16_t)v; }
static void freeze(MachineContext *m)
{
 unsigned i=0; uint32_t bits;
 uint32_t key=MotorAtomic_Enter();
 ForceServoDiagnostic d=m->servo.diagnostic;
 /* Single main-context producer. IRQ is masked for the small copy only;
  * serialization and all UART work occur after restoring interrupts. */
 MotorAtomic_Leave(key);
#define FS_U32(n) word32(m->servo.frozen,&i,d.n);
 FORCE_SERVO_DIAG_U32(FS_U32)
#undef FS_U32
#define FS_FLOAT(n) memcpy(&bits,&d.n,4); word32(m->servo.frozen,&i,bits);
 FORCE_SERVO_DIAG_FLOAT(FS_FLOAT)
#undef FS_FLOAT
}
bool ForceServoProtocol_WriteAddress(uint8_t f,uint16_t a)
{
 return (f==5 && (a==FS_COIL_START || a==FS_COIL_RESET)) ||
        (f==6 && ((a>=FS_REG_BEGIN && a<=FS_REG_SNAPSHOT) ||
         (a>=FS_REG_CONFIG && a<FS_REG_CONFIG+FORCE_SERVO_CONFIG_WORDS)));
}
MachineCommandResult ForceServoProtocol_Write(MachineContext *m,uint8_t f,uint16_t a,uint16_t v,uint32_t now)
{
 if (!m || !ForceServoProtocol_WriteAddress(f,a)) return COMMAND_UNSUPPORTED;
 ForceServoMachine *s=&m->servo;
 if (f==5) {
     if (v!=0xFF00) return COMMAND_INVALID_VALUE;
     MachineCommand c={a==FS_COIL_START ? CMD_FORCE_START : CMD_FAULT_RESET,0};
     return Machine_HandleCommand(m,&c,now);
 }
 if (a==FS_REG_SNAPSHOT) {
     if (v!=0xD101) return COMMAND_INVALID_VALUE;
     freeze(m); return COMMAND_ACCEPTED;
 }
 if (m->state!=IDLE || s->active || !MotorExecutor_OutputIsDisabled() ||
     MotorExecutor_GetSnapshot()->logical_active) return COMMAND_BUSY;
 if (a==FS_REG_BEGIN) {
     if (v!=0xB101) return COMMAND_INVALID_VALUE;
     s->staging_mask=0; s->staging_version=s->config_version;
     s->staging_open=true; return COMMAND_ACCEPTED;
 }
 if (!s->staging_open) return COMMAND_NOT_READY;
 if (a>=FS_REG_CONFIG) {
     unsigned i=a-FS_REG_CONFIG; s->staging[i]=v; s->staging_mask|=UINT64_C(1)<<i;
     return COMMAND_ACCEPTED;
 }
 if (a==FS_REG_COMMIT) {
     if (v!=0xC101 || s->staging_mask!=((UINT64_C(1)<<FORCE_SERVO_CONFIG_WORDS)-1) ||
         s->staging_version!=s->config_version) return COMMAND_INVALID_VALUE;
     ForceServoConfig candidate; unsigned i=0; uint32_t u;
#define FS_DECODE(n,d,l,h) u=((uint32_t)s->staging[i]<<16)|s->staging[i+1]; i+=2; memcpy(&candidate.n,&u,4);
     FORCE_SERVO_PARAMETERS(FS_DECODE)
#undef FS_DECODE
     if (!ForceServo_ConfigValid(&candidate)) { s->staging_open=false; return COMMAND_INVALID_VALUE; }
     uint32_t key=MotorAtomic_Enter();
     if (m->state!=IDLE || s->active || !MotorExecutor_OutputIsDisabled()) {
         MotorAtomic_Leave(key); return COMMAND_BUSY;
     }
     s->config=candidate; s->config_digest=ForceServo_ConfigDigest(&candidate);
     if (++s->config_version==0) ++s->config_version;
     memset(&s->controller,0,sizeof(s->controller)); s->staging_open=false;
     MotorAtomic_Leave(key);
     Machine_Tick(m,now); /* Refresh identity only; IDLE cannot start motion. */
     return COMMAND_ACCEPTED;
 }
 return COMMAND_UNSUPPORTED;
}
bool ForceServoProtocol_Read(const MachineContext *m,bool holding,uint16_t a,uint16_t *v)
{
 if (!m || !v) return false;
 const ForceServoMachine *s=&m->servo;
 if (holding && a>=FS_REG_CONFIG && a<FS_REG_CONFIG+FORCE_SERVO_CONFIG_WORDS) {
     uint32_t bits; unsigned index=(a-FS_REG_CONFIG)/2;
     memcpy(&bits,(const unsigned char*)&s->config+index*4,4);
     *v=(uint16_t)((a-FS_REG_CONFIG)%2 ? bits : bits>>16); return true;
 }
 if (holding) return false;
 if (a>=FS_INPUT_SNAPSHOT && a<FS_INPUT_SNAPSHOT+FORCE_SERVO_DIAG_WORDS) {
     *v=s->frozen[a-FS_INPUT_SNAPSHOT]; return true;
 }
 switch (a-FS_INPUT_INFO) {
 case 0: *v=FORCE_SERVO_SCHEMA; break;
 case 1: *v=MotorExecutor_GetSnapshot()->physical_output_locked ? 1 : 0; break;
 case 2: *v=FORCE_SERVO_CONFIG_WORDS; break;
 case 3: *v=FORCE_SERVO_DIAG_WORDS; break;
 case 4: *v=(uint16_t)(s->config_version>>16); break;
 case 5: *v=(uint16_t)s->config_version; break;
 case 6: *v=(uint16_t)(s->config_digest>>16); break;
 case 7: *v=(uint16_t)s->config_digest; break;
 default: return false;
 }
 return true;
}
