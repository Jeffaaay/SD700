# Open questions for Phase 2 state design

This is an unresolved-facts list, not a proposed state table, event table, context, router, fault manager, or motor API.

## Closed by the retained schematic revision

| Fact | Schematic result | Qualification |
| --- | --- | --- |
| MCU/package | STM32F411RCT6 | `SCHEMATIC_CONFIRMED`; `ASSEMBLED_BOARD_REVISION_NOT_PROBED` |
| HSE crystal | 8 MHz | `SCHEMATIC_CONFIRMED`; `ASSEMBLED_BOARD_REVISION_NOT_PROBED` |
| Current-sense input | PA4 / ADC1_CH4 | `SCHEMATIC_CONFIRMED`; `ASSEMBLED_BOARD_REVISION_NOT_PROBED` |
| Driver shutdown nets | PB1 / SD1 and PB2 / SD2 | `SCHEMATIC_CONFIRMED`; no external pull-down is shown |

The schematic does not show an upper limit input, lower limit input, usable encoder connection, or dedicated E-stop input. One-touch up/down therefore has no reliable stop condition and must remain unsupported. The software-only early shutdown reduces risk but is not a complete power-on hardware guarantee.

## Still open

1. What operating modes are actually required, and which legacy UI/Modbus modes remain contractual?
2. What hardware proves the physical upper and lower travel limits, and what polarity/debounce applies?
3. Is an encoder fitted on the assembled machine, what does it measure, and may it be used as a safety limit or only telemetry?
4. What raw pressure is the maximum safe value independent of target-pressure configuration?
5. What overcurrent threshold, polarity, filter, and debounce are validated for the fitted INA240/shunt variant?
6. Is there a dedicated E-stop hardware input on the assembled machine, and what electrical state is safe?
7. What are the verified electrical and mechanical differences among stop, coast, and brake?
8. Does the legacy "both drivers enabled, zero PWM" request really brake this bridge without unsafe shoot-through or heating?
9. What is the pressure sensor's guaranteed sample rate and latency?
10. What do pressure-frame bytes 3 and 4 mean?
11. Does the pressure frame provide a sequence counter or status/fault bits that should qualify freshness?
12. What command timeout or lease is required for local, jog, and remote motion?
13. What physical event will eventually make one-touch up/down safe enough to support?
14. Is long-press motion intended to start only after qualification, and how should press/release loss be handled?
15. Is hold completion defined by fresh samples, elapsed stable time, both, or another production criterion?
16. Which faults latch, which auto-clear, and what authorized action resets each fault?
17. Which future tasks are watchdog-critical, and who owns watchdog feeding?
18. What is the proven active polarity and power-on behavior of both IR2104 shutdown pins on the assembled board?
19. What station-ID range is legal, is address zero allowed, and when must EEPROM identity be loaded?
20. Is the second target-pressure register part of a real external contract or dead unfinished work?
21. What separation, filtering, plausibility, and timeout rules apply to raw safety pressure versus control pressure?

Until brake behavior is measured and qualified, the fault reaction remains force-disable rather than indefinite dynamic braking.
