# StaticForceRuntimeCharacterization2 handoff

Baseline36c9416c098d63a3ed7111034101616875573fa9 matched clean local and fetched
origin/main. This revision implements the latest user instruction: remove only
overall session/build/capture stops and allow a zero-force START with contact-gated
single assist. Historical firmware and field evidence are preserved.

Use README's one interactive wrapper command. Verify idle/OFF and current firmware
identity; keep PSU0.5 A unchanged, record the actual gap, and confirm supervised
longer motion/thermal exposure is permitted. No manual25-30 N preload is required.
Confirm the three runtime inputs and press ENTER for one script START. Below20 N
normal trajectory/PID continuous approach applies; the first fresh valid contact
at20 N enables one assist under existing demand/taper guards. No retry or escalation.
Targets1..2999 continue active HOLD until manual STOP or a real fault. Target3000
turns OFF at its first fresh valid reach; no HOLD. Use S/Escape or the physical
STOP immediately for abnormal motion, noise, current indication, heating or mechanics.
Return CSV/report/metadata and a brief field account. No automatic repeat.

Only runtime unit2 uses zero overall deadline fields. Executor still installs the
normal amplitude cap and receives fresh-pressure leases; removing its absolute
session deadline does not remove the receive or independent assist compare.20 N
is a contact latch, not a low-target trip. Legacy profiles retain contact-loss
faults and finite budgets. No PID, trajectory, TIM5 driver/IRQ, output guard,
START authority, STOP priority, post-assist pending, anti-windup or reversal
interlock changes. The first-contact latch never resets the assist ledger.

Conditional no-response and tracking guards remain5000 ms. They can fault a
stalled or unresponsive run with otherwise valid feedback; they are not overall
run deadlines. Freshness20 ms/gap125 ms and receive-anchored lease130 ms,1/2/4 ms
assist plan,30/2/25 N guards, RELEASE100 and locked gains10/0/0 are unchanged.
The5000 ms administrative re-arm lockout after STOP/fault is retained. It is not
validated cooling time. New deliberate plan/readback/ACK/START is always required.

CSV snapshots are flushed during capture with bounded capture memory. Finalization
attempts priority STOP/readback, then saves metadata/report. No elapsed observation
stop applies to the supervised wrapper; separate Observe diagnostics remain finite.
Old run reasons3/4 remain readable as historical data, not current termination policy.

physical test NOT RUN. HARDWARE_STOP_VALIDATION / STATIC_250 / ROTATING_LOAD remain
unvalidated. No continuous40%/10%, thermal, motor or3000 N rating is claimed. Host
software checks and register-OFF assertions do not establish physical stopping.
No agent serial connection, flash or motion occurred. No ZIP: pushed main is delivery.
