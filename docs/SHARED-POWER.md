# Shared power for the paired boards — assessment, not qualified wiring

A common supply can power both boards. A direct connection between arbitrary
contacts marked5V is not yet a qualified harness for these particular boards.
Keep the present TX/RX/GND link and separate USB supplies during diagnosis.

In the CrowPanel Advance V1.3 manufacturer Eagle schematic and matching PCB,
wireless socket J9 pin7 is net IOT_5V. Its only route to VIN is L3, whose fitted
value is marked **NC**. Treat it as unconnected unless actual population and
voltage/continuity have been verified. Do not expect that socket's5V label to
provide USB power automatically. J9 pin5 is3V3 and pin6 isGND; those are different
rails and not substitutes for a5V power input.

The S3 UART0-IN J14 pin3 is VIN, while its USB VBUS reaches VIN through D10.
A VIN label is not a guarantee of5.0V in every power mode; battery operation and
diode/cable drops matter. The RockBase C5 V1.0 schematic routes P5 pin1 to its5V
rail through Q1(AO3401A). This is not itself proof of connector/cable current
capacity or safe use with every USB supply combination. Actual PCB population,
rail voltages under load, USB reverse paths and available current must be checked
before recommending one board's USB connector as the other's power source.

The straightforward no-solder arrangement is one suitable fixed5V supply with
separate power branches into both USB inputs. It can consolidate the external
supply while retaining short USB tails at the boards. A power distribution lead
must be intended for this use; do not combine independent host/charger outputs
with an arbitrary cable. Elecrow specifies5V/2A input for CrowPanel; account for
both boards, backlights, Wi-Fi bursts and any battery charging/modules rather than
assuming a computer's ordinary USB port can supply the pair.

No power wire was connected, voltage measured or live device setting changed in
this assessment. The user confirmed current interconnect is TX/RX/GND only.

Primary sources:

- [Elecrow V1.3 schematic](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3/ESP32-Display-4.3-inch-V1.3.pdf)
- [Elecrow V1.3 Eagle files](https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/tree/6c8611ed51e44b0e2a9e92c23e89f3993df2742f/Eagle_SCH%26PCB/1.3)
- [RockBase C5 V1.0 schematic](https://github.com/RockBase-lab/NM-CYD-C5/blob/a1366c3d63363019371dac476a03a01b821ec5de/Documention/2-sch/SCH_NM-CYD-C5-v1.0.pdf)
- [Elecrow input specification](https://www.elecrow.com/crowpanel-advance-4-3-hmi-esp32-800x480-ai-display-ips-touch-artificial-intelligent-screen.html)
