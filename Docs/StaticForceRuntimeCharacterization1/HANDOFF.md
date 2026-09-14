# StaticForceRuntimeCharacterization1 handoff

Baseline80fe5767f702be26284a9d6b7f97cd445766e027 was clean and matched origin/main.
The latest user attachment explicitly authorizes the new supervised1/2/4 ms,
4 ms/session envelope,5000 ms re-arm lockout,30/2/25 N pressure guards and
installed-sensor Newton interface. It supersedes the earlier pending-profile-
timing blocker. Unit2/source3 distinguish that authorization from calibration
or mechanical/current/thermal qualification. No fake qualification bits are set.

One runtime field HEX only. Follow the README wrapper, choose one set of three
inputs, keep0.5 A unchanged, record gap, confirm the printed plan and send one
script START. No extra manual START or automated matrix/repeat. Abnormal behavior
requires immediate STOP. Return CSV/report/metadata and a short field account.
The existing fault latch may require a separate deliberate engineering reset
while outputs are OFF; the wrapper never silently resets a fault or waits/retries
until a run becomes allowed. Inter-run lockout is administrative, not cooling proof.

Software implementation keeps the trajectory/PID arithmetic, actual-output
anti-windup, immediate reductions, HOLD, direction OFF transition, pressure-age/
gap and receive-lease protections, TIM5 driver/IRQ and physical output guard.
The executor's bounded assist additionally accepts a healthy zero ordinary PRESS
owner so ContinuousPercent0 has defined behavior; no RELEASE-to-assist direction
bypass is permitted. Handoff is verified <=runtime continuous cap and <=current
assist command; zero is valid. No pressure packet is fabricated to service it.

The characterization config shortens its otherwise inactive tracking deadline
from10000 to5000 ms so the firmware-owned config itself matches its existing
5000 ms session ceiling. Saturation/no-response remains5000 ms. Reference rate/
acceleration and control/lease intervals are unchanged. High-target admission
allows useful bounded attempts without extending any timer.

Historical25%/10 ms evidence and3% no-motion evidence are history only. No physical
result exists for this envelope. physical test NOT RUN. HARDWARE_STOP_VALIDATION,
STATIC_250 and ROTATING_LOAD remain unvalidated. No3000 N, thermal,40% continuous,
10% continuous or PI tuning rating is claimed. No agent serial/flash/motion action.

Current fixed data/schema and exact hashes are in this directory, README and
Firmware/ForceServo1.SHA256SUMS.txt. The predecessor pair/hash manifest and original
field evidence are retained. No package/ZIP is generated; delivery is pushed main.
