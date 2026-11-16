# Solder-free S3/C5 connection: schematic-qualified candidate

The CrowPanel Advance 4.3-inch **V1.3** has a useful alternative to its UART
sockets: two direct GPIOs on its wireless-module socket. Reassign UART1 to those
GPIOs. This keeps the built-in SD routing, display/touch and CH340 USB console.
The socket must be empty; this route reserves it for the C5 connection.

Status on 2026-11-15: manufacturer schematic and PCB nets checked. User photos now
confirm the S3 V1.3 marking, empty module socket and contact labels/orientation.
C5 is visibly the Colorful variant; P5 is its right-edge four-contact UART socket
below both USB ports. In the ORIGINAL photo orientation its contacts top-to-bottom
are P5 pins 4/3/2/1: GND, GPIO5, GPIO4, power. Exact C5 PCB revision is not legible.
Cable fit/termination, physical continuity and electrical operation still need
confirmation. This is a candidate wiring map, not a report of a tested pair.
No firmware was installed, device setting changed or wiring made during research.
The user requests **no soldering** and prefers OTA updates.

## Candidate cable

| CrowPanel S3 V1.3 | Direction | RockBase NM-CYD-C5 V1.0 schematic |
| --- | --- | --- |
| Wireless socket **J11 pin 6**, GPIO8, proposed UART1 TX | S3 → C5 | **P5 pin 2**, GPIO4 / LP_UART_RXD |
| Wireless socket **J11 pin 5**, GPIO2, proposed UART1 RX | S3 ← C5 | **P5 pin 3**, GPIO5 / LP_UART_TXD |
| Wireless socket **J9 pin 6**, GND | Common ground | **P5 pin 4**, GND |

These are connector numbers in the manufacturer files, not left/right positions
or wire colors. J9 and J11 are the two different seven-contact socket rows.
**J11 pin 6 is GPIO8; J9 pin 6 is GND.** Mixing those rows would be incorrect.
Confirm the actual board/component-side orientation before inserting jumpers.
J11 pins 4 and 7 are unconnected in the V1.3 files; do not assume either is ground.

The Eagle socket footprint is 2.54 mm pitch. Use insulated plug-in jumpers and
a matching, preterminated C5 P5 pigtail. Confirm the C5 connector's actual housing
and pin-1 orientation before specifying a cable part; the schematic part name
alone is insufficient. Keep the initial cable short, approximately 10 cm.

For initial testing, power each board through its own USB supply. The interboard
cable carries TX, RX and ground only. Leave the C5 P5 pin 1 power contact and all
CrowPanel power contacts disconnected. Do not join USB/5 V/3.3 V supply rails.
Wire with power removed, then run the test with both boards powered; avoid leaving
one unpowered while the other drives its input. A common battery/power system is
a later design task, not an assumption that one board's regulator can power both.

Keep the existing SD selector setting. On this V1.3 board SD uses S1=1/S0=1;
GPIO2/GPIO8 do not pass through that selector. No 0/1 switch change is needed for
this candidate. No wireless daughterboard can remain connected to these signals.

## Why these pins

The V1.3 schematic XML gives these complete signal paths, and the PCB agrees:

| Net | Relevant schematic/PCB contacts |
| --- | --- |
| IO2_W_BUSY | ESP32 U5 IO2 / module pad 38, J11 pin 5, test pad P14 |
| IO8_W_CS | ESP32 U5 IO8 / module pad 12, J11 pin 6, test pad P5 |
| GND | J9 pin 6, among other board grounds |

Neither signal connects to selector U9/U11, the SD slot, CH340, RGB data or the
touch/control I2C bus. GPIO2 and GPIO8 are general-purpose S3 IOs and are not S3
boot-strapping pins. ESP-IDF can route UART signals through the GPIO matrix.
The current board HAL does not initialize these GPIOs, and its experimental
link conflict guard already permits them. Schematic net names BUSY and CS describe
their intended wireless-module use, not a restriction to those functions.

The C5 schematic routes P5 RX/TX through existing 100-ohm resistors to GPIO4/5;
P5 pin 4 is ground. The RockBase pinout independently agrees with GPIO4 RX and
GPIO5 TX. A normal UART controller can be mapped onto these GPIOs; the LP_UART
labels do not require us to change the OS into a low-power mode.

## Routes avoided

* CrowPanel J12 UART1-OUT uses GPIO19/20 through selector U9. Its required
  S1=0/S0=1 setting also switches U11's GPIO4/5/6 away from the built-in SD slot.
* CrowPanel UART0 is shared with the CH340 programming circuit through Q1/Q2.
  Another transmitter on that circuit requires isolation analysis; unplugging USB
  is not proof of isolation.
* GPIO15/16 already serve touch, the STC controller and RTC; repurposing that
  socket as UART would break those services.
* GPIO1 is physically tied to the touch interrupt even though the current driver
  polls touch. It is not a spare TX output.

Wireless remains an alternative if the module socket is unavailable. ESP-NOW
would need a deliberate channel policy: S3 is 2.4 GHz, and the C5's Wi-Fi/ESP-NOW
channel must be compatible. It is not an automatic substitute while the C5 uses
an arbitrary 5 GHz network. No wireless transport has been implemented here.

## Next firmware and acceptance

1. Confirm the socket is empty, identify connector pin 1 on the actual boards,
   and fit a preterminated cable. Confirm the installed C5 version; 0.4.0c was
   prepared but its installation has not been independently confirmed.
2. Prepare separate OTA candidates using the existing bounded UART1 adapter:
   S3 TX=8/RX=2; C5 TX=5/RX=4; 115200 baud, 8N1, no flow control. Do not turn on
   the wiring-confirmed flag before the physical cable is qualified. Ordinary
   standalone release defaults remain disabled with GPIOs -1.
3. Check Settings > Connected boards on both: correct roles, remote version,
   fresh uptime/heap/SD status and repeated successful echoes. Check both startup
   orders, peer restart/disconnect/reconnect, SD app reads, display/touch and OTA
   while connected. Verify that a missing peer never prevents OS startup.
4. Only after that, add one bounded remote service, with app ownership,
   permissions, timeouts and authentication before sensitive operations.

The existing protocol exchanges health and echo only. It does not combine RAM,
share an SD bus, transfer credentials or make the C5 a network/sensor coprocessor
yet. The S3 remains the intended UI/application host; the C5 becomes the companion
through subsequent service integration. See DEVICE-LINK.md and PAIRED-ARCHITECTURE.md.

## Primary sources

* [Elecrow V1.3 schematic PDF](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3/ESP32-Display-4.3-inch-V1.3.pdf)
* [Elecrow V1.3 Eagle schematic and PCB directory](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3)
* [RockBase C5 V1.0 schematic](https://github.com/RockBase-lab/NM-CYD-C5/blob/a1366c3d63363019371dac476a03a01b821ec5de/Documention/2-sch/SCH_NM-CYD-C5-v1.0.pdf)
* [RockBase C5 pinout](https://wiki.rockbaseiot.com/docs/products/nm-cyd-c5/#pinout)
* [Espressif S3 GPIO routing and restrictions](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/gpio.html)
* [Espressif C5 ESP-NOW channel rules](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c5/api-reference/network/esp_now.html)
