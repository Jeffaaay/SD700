# 1. Current code flow

```text
main
  ↓
runtime
  ↓
pressure sensor
  ↓
machine state machine
  ↓
motor request
  ↓
locked MotorExecutor
  ↓
Modbus status
```

- **main**：`User/main.c` 先强制关闭电机输出，再初始化 runtime、USART6 压力输入和 USART2 Modbus；主循环按固定顺序处理压力、安全、命令、状态计时和回复。
- **runtime**：`Application/runtime.c` 是薄协调层，把新压力样本交给 Machine，并依次执行压力安全检查、MotorExecutor service 和状态机 tick。
- **pressure sensor**：USART6 中断逐字节接收 7-byte frame；`PressureReceiver` 校验 header、BCC、marker，发布 raw pressure counts 和递增 sequence。当前转换是 raw counts 原样映射，不是 N。
- **machine state machine**：`Application/machine.c` 接收 target、START/STOP 和压力样本，在 `BOOT_SAFE`、`IDLE`、`AUTO_APPROACH`、`AUTO_SETTLE`、`AUTO_PULSE`、`AUTO_HOLD`、`FAULT` 之间转换。
- **motor request**：状态机只发出 press/release、command mV、duration 和 backstop 请求，不直接操作 PWM。
- **locked MotorExecutor**：校验请求、计算 planned CCR、模拟逻辑 deadline；同时反复保持 SD 低、CCR=0、PWM channel 关闭，所以没有真实电机动作。
- **Modbus status**：USART2 可读当前压力、Machine state/fault、最后 motor action、planned CCR，以及 `physical output locked/disabled` flags；可写 target 和 START/STOP。

# 2. Core functions

| Function | file | purpose | caller | state effect | physical motor effect |
| --- | --- | --- | --- | --- | --- |
| `main` | `User/main.c` | 安全启动并运行唯一主循环 | reset/C runtime | 完成 `BOOT_SAFE → IDLE`，循环驱动全部后续转换 | 启动最早阶段和每次循环都强制 disable |
| `PressureReceiver_ProcessBytes` | `Transport/Pressure/pressure_receiver.c` | 拼接并校验压力 frame，发布 raw counts/sequence | `PressureUart6_HandleRxComplete` | 无直接影响 | 无 |
| `ApplicationRuntime_ServicePressure` | `Application/runtime.c` | 去重样本、把 raw counts 转为 control units，再送入 Machine | `main` | 间接触发自动状态转换或 pressure fault | 无直接影响 |
| `ModbusSemantic_ApplyWrite` | `Transport/Modbus/modbus_semantic_map.c` | 把 Modbus target、START、STOP 写请求翻译为 Machine command | `ModbusRtuServer_ProcessWrite` / `ModbusRtuServer_TakeStop` | 交给 `Machine_HandleCommand` | START 仅产生逻辑请求；STOP 走 disable |
| `Machine_HandleCommand` | `Application/machine.c` | 检查 target/START/STOP 是否允许并执行 | `ModbusSemantic_ApplyWrite` | 设置 target；START 进入 approach 或 settle；STOP 回到 `IDLE`，但不会清除 `FAULT` | 只调用 MotorExecutor API；当前始终无物理输出 |
| `Machine_HandlePressureSample` | `Application/machine.c` | 校验新样本，并按 contact、target tolerance 分类 | `ApplicationRuntime_ServicePressure` | approach 接触后进 settle；settle 后选择 recontact/pulse/hold；hold 超出 exit tolerance 后再修正；异常进 `FAULT` | 只发逻辑 run/pulse/disable 请求 |
| `Machine_StartApproach` | `Application/machine.c` | 发出首次 approach 或 recontact 的 press run | `Machine_HandleCommand`、settle/hold 分类 | 进入 `AUTO_APPROACH`，记录 request sequence/profile | 当前只记录 6000 mV/250 ms 或 3000 mV/100 ms 计划；电机不动 |
| `Machine_StartPulse` | `Application/machine.c` | 发出 press 或 release pulse | settle/hold 分类 | 进入 `AUTO_PULSE`，记录 request sequence | 当前只记录 2000 mV/20 ms pulse 计划；电机不动 |
| `Machine_EnterSettle` | `Application/machine.c` | 停止当前请求，开始等待机械稳定和新样本 | START、接触检测、pulse completion | 进入 `AUTO_SETTLE/SETTLE_WAIT_DELAY` | 调用 disable；物理输出保持关闭 |
| `Machine_Tick` | `Application/machine.c` | 推进 settle 的时间门 | `ApplicationRuntime_Tick` | settle delay 到期后改为 `SETTLE_WAIT_SAMPLE`；非法状态进 `FAULT` | 无新动作 |
| `Machine_EnterHold` | `Application/machine.c` | 在压力进入 hold tolerance 时停止修正 | settled sample 分类 | 进入 `AUTO_HOLD`，结束本次 cycle timeout 计时 | 调用 disable；物理输出保持关闭 |
| `MotorExecutor_Service` | `Board/Motor/motor_executor_locked.c` | 推进逻辑 deadline/backstop 并发布 completion | `ApplicationRuntime_ServiceSafety` | 自身不改 Machine；随后 `Machine_HandleMotorService` 可令 pulse 进 settle 或令 timeout 进 `FAULT` | 每次调用都再次强制 disable |
| `MotorExecutor_Disable` | `Board/Motor/motor_executor_locked.c` | 清除逻辑请求和 planned PWM | Machine 初始化、STOP、fault、settle、hold | 自身不改 Machine state | SD 保持低，TIM2/TIM3 CCR3 清零，PWM channel 不启用 |
| `ModbusSemantic_ReadInput` | `Transport/Modbus/modbus_semantic_map.c` | 把压力、状态、fault、motor plan 和 lock flags 映射为 input registers | `ModbusRtuServer_ProcessRead` | 无 | 只读 snapshot，无输出 |

# 3. Current project stage

**当前 CLEAN 已实现**

- 安全 boot、bounded main loop、USART6 压力 frame 接收/校验/sequence/freshness，以及 USART2 Modbus RTU 的 target、START、STOP、状态和诊断读取。
- `BOOT_SAFE → IDLE → APPROACH → SETTLE → PULSE/HOLD` 的状态逻辑，以及 pressure invalid/stale、顺序错误、settle feedback timeout、motion timeout 等 fault 路径。
- STOP 优先处理；Machine 的 stop/fault/settle/hold 都会走 disable。
- locked MotorExecutor 的参数校验、PWM 数值规划、逻辑计时、request/completion snapshot 和持续硬件关闭。

**因 MotorExecutor locked 而只有逻辑、没有物理动作**

- 首次 approach、recontact、press/release pulse、settle 后分类和 hold 内回差修正都只改变状态与 shadow plan。
- `planned_tim2_ccr3` / `planned_tim3_ccr3` 只用于诊断；active source 不把非零值写入 timer，也不启动 PWM 或解除 SD。
- 当前 `control_pressure_units` 是 raw sensor counts 的 identity conversion，bench 参数明确是 unvalidated，不代表 N。

**尚未实现或尚不能由当前 CLEAN 做物理验证**

- 真实 MotorExecutor、真实 PWM/driver enable，以及安全受控的真实 approach/pulse 动作尚未实现。
- raw counts 到 N 的标定、Target/Peak/Final/pulse command 的完整运行日志尚未实现。
- 真实闭环的 overshoot、accuracy、repeatability 尚不能用当前 locked firmware 验证；生产 cycle、`COMPLETE` 流程、jog/fault reset 等命令及完整 production safety 工作也未完成。当前 bench config 的 raw overpressure check 还是 disabled。
- 报告行为 `Target 60 N → Peak above 100 N → Final 60 N` 只能归入 older motor-enabled implementation；当前 CLEAN source 的 locked output 和 raw-count control 不能证明或复现该结果。

# 4. Remaining steps

1. 理解当前 locked code 和实际调用顺序。
2. 实现真实 MotorExecutor，并保留明确的 disable/fault 边界。
3. 进行 low-energy motor test 和 pulse test。
4. 记录每次运行的 Target / Peak / Final / pulse command。
5. 根据实测日志减少 overshoot。
6. 验证 accuracy 和 repeatability。
7. 完成 production cycle 和 safety 工作。
