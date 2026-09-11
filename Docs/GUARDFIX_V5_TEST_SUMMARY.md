# GuardFixV5 当前 workspace 交付说明

下一次现场验证仅使用包内 `Firmware/SD700_GuardFixV5_RealBench_Release.hex`，其匹配 ELF 位于同目录。本次为 RealBench 单脉冲测试配置：**10000 mV / 100 ms / 150 ms backstop**，不是 production 保压参数。

**真机状态：未实测。** 本次未刷机、未运行物理脉冲、未调压；不能据此认定稳定保压或过冲已解决。历史资料中的测试记录不代表本次 V5 的硬件验证结果。ZIP 不包含旧构建固件作为候选。

## 修改

- 保留 `MotorExecutor_ActiveRequestIsValid()` 已有竞态修复及全部回归测试。
- `MotorExecutor_Service()` 在读取 physical status 后，以及报告 timer unhealthy、timer not armed、logical backstop、output dropped 前重新取 completion；有事件即交给原有 PublishCompletion，保留 NORMAL/BACKSTOP/ERROR 分类。
- `MotorExecutor_GuardOutput()` 在入口、physical 查询后、活动请求校验后及关闭输出复核后检查 pending。对 pending 再关闭输出、取消定时器、读取状态，只有输出确实关闭且 timer unarmed 才返回 OK。Guard 不消费、不清除、不替换事件；没有事件的异常和无法关闭的输出/定时器仍返回故障。
- 主循环调整为 pressure 更新 → ServiceSafety → GuardOutput → 原有 Modbus 处理。STOP 仍优先于普通 Modbus 请求，没有新增等待。
- 仅修改 direct-pulse 三个测试配置宏；相关直接脉冲断言/到期时间使用这些宏。未机械替换自动控制测试中的独立参数。
- 采集工具改用本次实际 HEX 名称/hash，标注未实测；保留一次运动写入、无重试、STOP 清理、CSV 和离线 self-test 行为。未新增 approach 工具。

生产代码变更：`Board/Motor/motor_executor_real.c`、`Application/direct_pulse_config.h`、`User/main.c`。

测试变更：`Tests/Host/fake_motor_hw_real.c/.h`、`fake_motor_stop_timer.c/.h`、`test_motor_executor_real.c`、`test_direct_pulse_full_chain.c`、`test_direct_pulse_commands.c`，新增 `Tests/Host/run_completion_race_regressions.ps1`。另更新 README、当前协议/状态机文档及 `tools/capture_pressure_response.ps1` 的固件身份。

Machine/runtime、PWM/SD 控制实现、方向映射、TIM5 硬停止、Modbus 实现、压力控制参数、模式/acknowledgement 策略和构建脚本保持不变。默认 Locked；RealBench 保留压力安全门槛；真实输出模式自动闭环仍禁用。

## 实际测试结果

| 测试 | 结果 |
| --- | --- |
| 本轮修改前 Service/Guard 故障复现 | PRESS/RELEASE × Service/Guard × O0/O2，共 8/8 预期断言失败；日志保留为 before_* |
| ActiveRequest 回归 | 两方向、ScopeTest/RealBench，O0/O2 PASS；正常到期时间 103 ms，早于 backstop 和压力过期 |
| Service/Guard 执行器回归 | 两方向 × 三类事件 × 八个注入位置，每档优化 48 个窗口；O0/O2 PASS |
| Service/Guard 全链路回归 | 每个模式/优化运行两方向 × 三类事件 × 两个入口 × 两种查询窗口，均 PASS |
| 真实异常验证 | 无事件掉输出、停表、不健康；pending 输出/定时器无法关闭；STOP、传感器故障及原有 Guard/Service 测试均 PASS |
| 无重复完成/无额外运动 | 验证下次 Service 进入 inactive 路径、事件次数及 arm/apply/request sequence 不增加，PASS |
| 完整 host suite | 23 个已有 host 可执行测试、编译策略测试、20 次并发重复、所有新增回归，PASS |
| 主循环顺序 / STOP 优先顺序检查 | PASS |
| 采集工具更新后 offline self-test | PASS；预期 hash 与实际 RealBench Release HEX 一致 |
| 固件构建 | Locked / RealCompileCheck / ScopeTest / RealBench，各 Debug 和 Release，8/8 PASS |

测试使用 Windows PowerShell 5.1、系统 `gcc` 命令（LLVM-MinGW clang 22.1.8）进行 host 编译，`-std=c11 -Wall -Wextra -Werror -pedantic`。固件使用 Arm GNU Toolchain 14.2.Rel1 / GCC 14.2.1。所有物理状态和中断时机注入都是 host fake，不涉及硬件。

可复跑命令：

```powershell
.\tools\run_host_tests.ps1
.\Tests\Host\run_completion_race_regressions.ps1
.\tools\capture_pressure_response.ps1 -SelfTest
```

8 个构建使用未修改的 `tools/build_gcc.ps1`，指定 `-BuildDir build/GuardFixV5`、对应 `-Configuration` 和 `-MotorMode`。ScopeTest 使用 `-ScopeTestAck MOTOR_POWER_DISCONNECTED`；RealBench 使用 `-RealBenchAck I_ACKNOWLEDGE_LOW_ENERGY_REAL_MOTOR_MOTION`。完整输出保存在 `Validation/GuardFixV5/`，所有模式生成物的 hash 记录在其中的 `BUILD_SHA256SUMS.txt`；仅下一次现场验证候选被放入 Firmware 目录。

## 实际固件 SHA-256

```text
SD700_GuardFixV5_RealBench_Release.hex
1D5AADF5C957B97654F7B3305813D47594B8C42C3373EA1182B6357559241C04

SD700_GuardFixV5_RealBench_Release.elf
015FC9D8F0D0A7EE21EAE7BE4DECDCF2F9D2FFA81560CDB1A374456B9CFDF6D5
```

`Firmware/SHA256SUMS.txt` 校验该匹配 HEX/ELF，包根 `SHA256SUMS.txt` 校验源码、说明、日志和固件。源码来自当前 workspace；打包排除了 `.git`、旧 `build/`、旧 `ReleaseArtifacts/` 和 field CSV，保留其余项目源码/资源。
