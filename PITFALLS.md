# Every pitfall in the volume

Generated from the Pitfalls section of each project, so it is an index and
never a second copy: the sentence that explains a pitfall stays in the project
that owns it. Alphabetical, because by project is what the chapters already give.

**163 pitfalls across 20 projects.**

| Pitfall | Project |
|---|---|
| 0x52 and 0x29 are the same sensor | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| 16 MHz on jumpers against 200 ms | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| 24 answers is a small table | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| 6 kHz times three axes over a byte-at-a-time port | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| `--live gpio` is the wrong flag here | [Project 3](chapters/03-led-status-explainer.md) |
| `AT+CMNB` must lock the radio access technology | [Project 17](chapters/17-cellular-status-summariser.md) |
| `BindsTo` in the wrong direction switches the monitoring off | [Project 14](chapters/14-joint-monitor.md) |
| `dtparam=i2c_arm=on` is not `/dev/i2c-1` | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| `hat_list` proves less than it reads as proving | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| `iio:deviceN` follows probe order | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| `tools/agreement.py` does not apply here | [Project 17](chapters/17-cellular-status-summariser.md) |
| `tools/agreement.py` reads a `decision` key | [Project 3](chapters/03-led-status-explainer.md) |
| `ttyUSB` against `ttyACM` | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| `use_mmap` and `llama_kv_self_*` are gone | [Project 2](chapters/02-the-decision-loop.md) |
| `vcgencmd` needs the video group | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| A 10 microsecond sampler cannot see a nanosecond pulse | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| A 50 Hz poll can miss a tap | [Project 5](chapters/05-adxl345-event-reporter.md) |
| A baseline taken on another mounting, or under a hand | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| A board that browns out under sustained load still prints a number | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| A C ABI that changes without its version being bumped | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| A CAN error frame may not carry the counters | [Project 14](chapters/14-joint-monitor.md) |
| A capture taken without a SIM measures an attach that never completes | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| A character outside the SMS set changes the arithmetic, not the message | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| A frame form that was not verified | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| A generated table that quietly loses a project | [Project 20](chapters/20-what-should-i-measure-next.md) |
| A line held by another consumer | [Project 3](chapters/03-led-status-explainer.md) |
| A literal that differs by one letter | [Project 2](chapters/02-the-decision-loop.md) |
| A literal that spends the token budget | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| A live log whose summary says replay | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| A modelled node that replays a hardware node's identifier | [Project 13](chapters/13-can-honesty-agent.md) |
| A polled sensor misses most taps | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| A reason that names both fields | [Project 3](chapters/03-led-status-explainer.md) |
| A rule file put into a static library | [Project 2](chapters/02-the-decision-loop.md) |
| A rule file that includes a header of its own project | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| A seated panel leaves no pin reachable | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| A serial read that returns what has arrived rather than a frame | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| A server restart loses nothing | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| A signal of 99 means unknown, not excellent | [Project 17](chapters/17-cellular-status-summariser.md) |
| A socket timeout must not stall the bus reader | [Project 14](chapters/14-joint-monitor.md) |
| A stale flash reads as a reproduction | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| A step on a window boundary | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| A stub log read as a bench log | [Project 5](chapters/05-adxl345-event-reporter.md) |
| A tag that moved | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| A tokenizer that splits an enum literal | [Project 2](chapters/02-the-decision-loop.md) |
| A tokenizer that splits FREEFALL | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| All three light at the request | [Project 3](chapters/03-led-status-explainer.md) |
| Ambient light and the target's reflectance | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| Ampere-meter mode where source-meter mode is meant | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| An agreement rate over a handful of events | [Project 20](chapters/20-what-should-i-measure-next.md) |
| An alert storm if the rule fires repeatedly | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| An attach takes minutes, so nothing is decided live | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| An echo that does not match | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| An LED on a strapping pin | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| An open contact is a read error, never a wrong value | [Project 5](chapters/05-adxl345-event-reporter.md) |
| An operator name the model invents is the thing being counted | [Project 17](chapters/17-cellular-status-summariser.md) |
| Bare copper under the screw | [Project 13](chapters/13-can-honesty-agent.md) |
| BindsTo= stops the console | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| Both SPI0 chip selects taken by the CAN overlays | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| Delivery is unobservable from the Pi | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| fbcon repaints over the frame | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| Four decisions in a one second period | [Project 13](chapters/13-can-honesty-agent.md) |
| Half-duplex direction control, and the receiver during transmit | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| Impacts faster than decisions | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| Insulation under the screw | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| INT1 is not available on this board | [Project 14](chapters/14-joint-monitor.md) |
| ModemManager, and a charge-only cable | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| No chat template on `/completion` | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| One byte order assumed throughout | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| One sampler chain serves the whole answer | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| Only range status 0 is a range | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| Ordering is not readiness | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| Paging that looks like slowness | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| Pin 11 is the touch interrupt | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| Pins 11, 13, 15 and the ground rail | [Project 3](chapters/03-led-status-explainer.md) |
| Restoring a state saved with a different model or context size | [Project 2](chapters/02-the-decision-loop.md) |
| SA0 high puts the LPS22DF at 0x5d | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| SPI0 CE0 belongs to the OLED | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The 1.5B model against a memory reading nobody has taken | [Project 20](chapters/20-what-should-i-measure-next.md) |
| The 100 kHz bus | [Project 5](chapters/05-adxl345-event-reporter.md) |
| The ABI's units are not the prompt's | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| The accelerometer's contacts are friction | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| The ADXL345 bus at 100 kHz, and in m/s^2 | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| The binary with the engine inside, and two builds of one engine | [Project 5](chapters/05-adxl345-event-reporter.md) |
| The bus must stay at 100 kHz | [Project 14](chapters/14-joint-monitor.md) |
| The bus-off recovery delay | [Project 13](chapters/13-can-honesty-agent.md) |
| The buzzer's polarity is an assumption | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The CS and SDO straps | [Project 5](chapters/05-adxl345-event-reporter.md) |
| The dead Pi 3B+ console | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| The direction mapping is read off a rendered page | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The DS3231 as hwclock against NTP time | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The error counters are cumulative | [Project 14](chapters/14-joint-monitor.md) |
| The event tag is 32 bytes | [Project 17](chapters/17-cellular-status-summariser.md) |
| The fallback was written against the wrong signature, and the guard is what found it | [Project 20](chapters/20-what-should-i-measure-next.md) |
| The firmware mode decides whether the port exists | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| The first bins hold the gravity offset | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| The flip event may precede the transfer | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| The framebuffer's number follows probe order, and its driver is not ssd1307fb | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The granted window is the network's choice, not the module's request | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| The has-occurred bits | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| The HAT covers the header and the 3B+ console is dead anyway | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The HAT covers the header and the console is dead anyway | [Project 17](chapters/17-cellular-status-summariser.md) |
| The HAT's AT command manual has not been read on this bench | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| The HAT's logic voltage, and the PL011 it talks on | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| The HAT's own clock accuracy is unread | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| The HAT's pass-through is an assumption until it is seen | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| The high-g channel is a peak-hold, not a sample | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| The host logic voltage is a 0 ohm resistor | [Project 17](chapters/17-cellular-status-summariser.md) |
| The humidity sensor is not an IIO device, and it has a rate | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| The IKS4A1 and the IKS5A1 never together | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| The instrument cannot meter a Raspberry Pi | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| The interface names swap between boots | [Project 14](chapters/14-joint-monitor.md) |
| The interface order between boots | [Project 13](chapters/13-can-honesty-agent.md) |
| The joystick lines are inputs with pull-ups, on two chips | [Project 8](chapters/08-explorer700-console-with-a-model.md) |
| The key is `trend`, not `decision` | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| The library version pair | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| The map keys are the hub's contract | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| The MCC 118 overlay on this card | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| The MCP2515 cannot do CAN FD | [Project 13](chapters/13-can-honesty-agent.md) |
| The model file over 60 percent of `MemAvailable` | [Project 2](chapters/02-the-decision-loop.md) |
| The model stalls the sampler | [Project 5](chapters/05-adxl345-event-reporter.md) |
| The model writes a plausible number the event never had | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| The model's latency against a fast approach | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| The modem is mid-attach when an event arrives | [Project 19](chapters/19-event-driven-cellular-alert-composer.md) |
| The NanoPi armv7 NEON flags | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| The NanoPi build is armv7 and 512 MB | [Project 3](chapters/03-led-status-explainer.md) |
| The narrator heats the board it narrates | [Project 4](chapters/04-pi-thermal-and-throttle-narrator.md) |
| The node table lives in the service, not the daemon | [Project 14](chapters/14-joint-monitor.md) |
| The panel's text size | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| The pipe fills while the model thinks | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| The placeholder tie | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| The published figure is an upper bound, not a battery life | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| The remote host that does not answer | [Project 20](chapters/20-what-should-i-measure-next.md) |
| The RS232 channel takes the Pi's console UART | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| The sc16is752 overlay half applied | [Project 18](chapters/18-mini-industrial-serial-logger.md) |
| The scale file's unit | [Project 5](chapters/05-adxl345-event-reporter.md) |
| The SPI0 chip selects shared with the MCC 118 | [Project 13](chapters/13-can-honesty-agent.md) |
| The stub always answers GREEN with an empty reason | [Project 3](chapters/03-led-status-explainer.md) |
| The stub always answers the first expansion | [Project 2](chapters/02-the-decision-loop.md) |
| The stub always answers the first expansion | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| The stub always answers the first literal | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| The stub answering the first literal | [Project 13](chapters/13-can-honesty-agent.md) |
| The stub answers the first literal | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| The stub in CI, read as a result | [Project 20](chapters/20-what-should-i-measure-next.md) |
| The swapping card | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| The ttyACM order with the ESP32 present | [Project 11](chapters/11-nucleo-cdc-sensor-hub-narrator.md) |
| The two forms of `+CEREG` | [Project 17](chapters/17-cellular-status-summariser.md) |
| The two IMUs are swapped against the first overlay | [Project 7](chapters/07-iks4a1-climate-and-motion-narrator.md) |
| The two supplies trap | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| The ULD's licence is read before the ULD is copied | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| The verdict shows seconds before the answer | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| The wheel that builds the engine | [Project 15](chapters/15-esp32-sidecar-with-a-bounded-pattern-proposal.md) |
| The window is shorter than the decision | [Project 12](chapters/12-stwin-box-vibration-band-reporter.md) |
| Thermal range 0 to 55 C (to verify) | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| Thermal throttling while narrating | [Project 1](chapters/01-bring-up-and-the-honest-tokens-per-second.md) |
| Thirteen points, not thirty-two | [Project 10](chapters/10-mcc-118-voltage-event-labeller.md) |
| Two 3V3 pins, three consumers | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
| Two crates whose symbol surface is unverified | [Project 14](chapters/14-joint-monitor.md) |
| Two devices on one bus at 100 kHz | [Project 6](chapters/06-vl53l4cd-too-close-explainer.md) |
| Two supplies into one device under test | [Project 16](chapters/16-nb-iot-sleep-interval-from-measured-microamps.md) |
| Two writers on one port | [Project 17](chapters/17-cellular-status-summariser.md) |
| Unsolicited result codes arrive in the middle of a response | [Project 17](chapters/17-cellular-status-summariser.md) |
| Whether the HAT's header passes through is not recorded | [Project 14](chapters/14-joint-monitor.md) |
| Which card is the panel moves between kernels | [Project 9](chapters/09-lcd-prompt-and-verdict-display.md) |
