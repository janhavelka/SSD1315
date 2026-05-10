# SSD1315 Datasheet

- Source PDF: `docs/SSD1315_datasheet.pdf`
- Extraction date: 2026-05-09
- Page count: 62
- SHA256: `43c73665962419046a8e374d3e72e9dee3d681d3fbab138994b8fd373800828b`
- Extraction tool: pypdf

## Page 1

```text
SOLOMON SYSTECH
SEMICONDUCTOR TECHNICAL DATA
This document contains information on a new product. Specifications and information herein are subject to change
without notice.
http://www.solomon-systech.com
SSD1315 Rev 1.1 P 1/36 Apr 2017 Copyright - 2017 Solomon Systech Limited
SSD1315
Advance Information
128 x 64 Dot Matrix
OLED/PLED Segment/Common Driver with Controller
```

## Page 2

```text
Solomon Systech Apr 2017 P 2/36 Rev 1.1 SSD1315
Appendix: IC Revision history of SSD1315 Specification
Version Change Items Effective Date
1.0 1st Release 05-Jan-17
1.1 Revise typo of number of ISEG in DC characteristics 02-May-17
```

## Page 3

```text
SSD1315 Rev 1.1 P 3/36 Apr 2017 Solomon Systech
CONTENTS
1 GENERAL DESCRIPTION ................................................................................................ 6
2 FEATURES ........................................................................................................................... 6
3 ORDERING INFORMATION ............................................................................................ 6
4 BLOCK DIAGRAM ................................ ................................ ................................ ............. 7
5 PIN DESCRIPTION ............................................................................................................. 8
6 FUNCTIONAL BLOCK DESCRIPTIONS ....................................................................... 11
6.1 MCU INTERFACE SELECTION ......................................................................................................... 11
6.1.1 MCU Parallel 6800-series Interface .......................................................................................................... 11
6.1.2 MCU Parallel 8080-series Interface .......................................................................................................... 12
6.1.3 MCU Serial Interface (4-wire SPI) ............................................................................................................ 13
6.1.4 MCU Serial Interface (3-wire SPI) ............................................................................................................ 14
6.1.5 MCU I2C Interface ................................................................................................................................... 15
6.2 COMMAND DECODER ..................................................................................................................... 18
6.3 OSCILLATOR CIRCUIT AND DISPLAY TIME GENERATOR ................................................................ . 18
6.4 FR SYNCHRONIZATION .................................................................................................................. 19
6.5 RESET CIRCUIT .............................................................................................................................. 19
6.6 SEGMENT DRIVERS / COMMON DRIVERS ........................................................................................ 20
6.7 GRAPHIC DISPLAY DATA RAM (GDDRAM) ................................................................................. 21
6.8 SEG/COM DRIVING BLOCK ........................................................................................................... 22
6.9 POWER ON AND OFF SEQUENCE ................................................................................................... 23
6.9.1 Power ON and OFF sequence with External VCC....................................................................................... 23
6.9.2 Power ON and OFF sequence with Charge Pump Application ................................................................... 24
6.10 CHARGE PUMP REGULATOR ....................................................................................................... 25
7 MAXIMUM RATINGS ...................................................................................................... 26
8 DC CHARACTERISTICS ................................................................................................ . 27
9 AC CHARACTERISTICS ................................................................................................ . 28
10 APPLICATION EXAMPLE .............................................................................................. 34
```

## Page 4

```text
Solomon Systech Apr 2017 P 4/36 Rev 1.1 SSD1315
TABLES
Table 3-1: Ordering Information .................................................................................................................................... 6
Table 5-1: Pin Description ............................................................................................................................................. 8
Table 5-2: Bus Interface selection .................................................................................................................................. 8
Table 6-1: MCU interface assignment under different bus interface mode .................................................................... 11
Table 6-2: Control pins of 6800 interface ..................................................................................................................... 11
Table 6-3: Control pins of 8080 interface ..................................................................................................................... 13
Table 6-4: Control pins of 4-wire Serial interface ......................................................................................................... 13
Table 6-5: Control pins of 3-wire Serial interface ......................................................................................................... 14
Table 7-1: Maximum Ratings ...................................................................................................................................... 26
Table 8-1: DC Characteristics ...................................................................................................................................... 27
Table 9-1: AC Characteristics ...................................................................................................................................... 28
Table 9-2: 6800-Series MCU Parallel Interface Timing Characteristics ........................................................................ 29
Table 9-3: 8080-Series MCU Parallel Interface Timing Characteristics ........................................................................ 30
Table 9-4: Serial Interface Timing Characteristics (4-wire SPI) .................................................................................... 31
Table 9-5: Serial Interface Timing Characteristics (3-wire SPI) .................................................................................... 32
Table 9-6: I2C Interface Timing Characteristics ........................................................................................................... 33
```

## Page 5

```text
SSD1315 Rev 1.1 P 5/36 Apr 2017 Solomon Systech
FIGURES
Figure 4-1: SSD1315 Block Diagram ............................................................................................................................ 7
Figure 6-1: Data read back procedure - insertion of dummy read .................................................................................. 12
Figure 6-2: Example of Write procedure in 8080 parallel interface mode ...................................................................... 12
Figure 6-3: Example of Read procedure in 8080 parallel interface mode....................................................................... 12
Figure 6-4: Display data read back procedure - insertion of dummy read ...................................................................... 13
Figure 6-5: Write procedure in 4-wire Serial interface mode ........................................................................................ 14
Figure 6-6: Write procedure in 3-wire Serial interface mode ........................................................................................ 14
Figure 6-7: I2C-bus data format ................................................................................................................................... 16
Figure 6-8: Definition of the Start and Stop Condition ................................................................................................ . 17
Figure 6-9: Definition of the acknowledgement condition ............................................................................................ 17
Figure 6-10: Definition of the data transfer condition ................................................................................................... 17
Figure 6-11: Oscillator Circuit and Display Time Generator ......................................................................................... 18
Figure 6-12: Segment Output Waveform in three phases .............................................................................................. 20
Figure 6-13: GDDRAM pages structure ....................................................................................................................... 21
Figure 6-14: Enlargement of GDDRAM (No row re-mapping and column-remapping)................................................. 21
Figure 6-15: IREF Current Setting by Resistor Value ..................................................................................................... 22
Figure 6-16: The Power ON Sequence ......................................................................................................................... 23
Figure 6-17: The Power OFF Sequence ....................................................................................................................... 23
Figure 6-18: The Power ON sequence with Charge Pump Application ......................................................................... 24
Figure 6-19: The Power OFF sequence with Charge Pump Application ........................................................................ 24
Figure 9-1: 6800-series MCU parallel interface characteristics ..................................................................................... 29
Figure 9-2: 8080-series parallel interface characteristics ............................................................................................... 30
Figure 9-3: Serial interface characteristics (4-wire SPI) ................................................................................................ 31
Figure 9-4: Serial interface characteristics (3-wire SPI) ................................................................................................ 32
Figure 9-5 I2C interface Timing characteristics ............................................................................................................ 33
Figure 10-1: Application Example of SSD1315 with External VCC and I2C interface .................................................... 34
Figure 10-2: Application Example of SSD1315 with Internal Charge Pump and I2C interface ....................................... 34
```

## Page 6

```text
Solomon Systech Apr 2017 P 6/36 Rev 1.1 SSD1315
1 GENERAL DESCRIPTION
SSD1315 is a single-chip CMOS OLED/PLED driver with controller for organic/polymer light emitting
diode dot-matrix graphic display system. It consists of 128 segments and 64 commons. This IC is
designed for Common Cathode type OLED/PLED panel.
SSD1315 displays data directly from its internal 128 x 64 bits Graphic Display Data RAM (GDDRAM).
Data/Commands are sent from general MCU through the hardware selectable I2C Interface, 6800-/8080-
series compatible Parallel Interface or Serial Peripheral Interface.
The 256 steps contrast control and oscillator which embedded in SSD1315 reduces the number of external
components. SSD1315 is suitable for portable applications requiring a compact size and high output
brightness, such as set-top box, car audio, wearable electronics, etc.
2 FEATURES
- Resolution: 128 x 64 dot matrix panel
- Power supply
o VDD = 1.65V - 3.5V, <= VBAT (for IC logic)
o VBAT = 3.0V - 4.5V (for charge bump regulator circuit)
o VCC = 7.5 V - 16.5V (for Panel driving)
- Segment maximum source current: 240uA
- Common maximum sink current: 30mA
- Embedded 128 x 64 bit SRAM display buffer
- Pin selectable MCU Interfaces:
o 8 bits 6800/8080-series parallel Interface
o 3/4 wire Serial Peripheral Interface
o I2C Interface
- Screen saving continuous scrolling function in both horizontal and vertical direction
- Screen saving infinite content scrolling function
- Internal or external IREF selection
- Internal charge pump regulator
- RAM write synchronization signal
- Programmable Frame Rate and Multiplexing Ratio
- Row Re-mapping and Column Re-mapping
- Power On Reset (POR)
- Dynamic Grayscale
- On-Chip Oscillator
- Chip layout for COG, COF
- Wide range of operating temperature: -40C to 85C
3 ORDERING INFORMATION
Table 3-1: Ordering Information
Ordering Part Number SEG COM Package Form Remark
SSD1315Z 128 64 COG
o Min SEG pad pitch : 27um
o Min COM pad pitch : 27um
o Min I/O pad pitch : 30um
o Die thickness: 250um
o Bump height: nominal 9um
```

## Page 7

```text
SSD1315 Rev 1.1 P 7/36 Apr 2017 Solomon Systech
4 BLOCK DIAGRAM
Figure 4-1: SSD1315 Block Diagram
Common Drivers Segment Driver Common Drivers
Display
Timing
Generator
Oscillator
MCU
Interface
RES#
CS#
D/C#
E(RD#)
R/W# (WR#)
BS0
BS1
BS2
LS
Command
Decoder
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
Com31
Com32
Seg127
Seg126
.
.
.
.
.
Seg1
Seg0
CL
CLS
Com0
Com1
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
.
SEG/COM
Driving Block
VDD
VSS
VLSS
BGGND
FR
VCOMH
IREF
VBAT
Com63
Com62
Com30
Com31
GDDRAM
Charge-pump
D7
D6
D5
D4
D3
D2
D1
D0
VCC
C1N
C1P
C2N
C2P
```

## Page 8

```text
Solomon Systech Apr 2017 P 8/36 Rev 1.1 SSD1315
5 PIN DESCRIPTION
Key:
I = Input NC = Not Connected
O = Output Pull LOW = connect to Ground
I/O = Bi-directional (input/output) Pull HIGH = connect to VDD
P = Power pin
Table 5-1: Pin Description
Pin Name Type Description
VDD P Power supply pin for core logic operation.
VCC P Power supply for panel driving voltage. This is also the most positive power voltage supply pin.
When charge pump is enabled, a capacitor should be connected between this pin and VSS.
VSS P Ground pin. It must be connected to external ground.
VLSS P This is an analog ground pin. It should be connected to VSS externally.
VCOMH O COM signal deselected voltage level.
A capacitor should be connected between this pin and VSS.
VBAT P Power supply for charge pump regulator circuit.
Status VBAT VDD VCC
Enable
charge pump
Connect to external
VBAT source
Connect to external
VDD source
A capacitor should be
connected between this pin
and VSS
Disable
charge pump
Keep float Connect to external
VDD source
Connect to external VCC
source
BGGND P Reserved pin. It should be connected to VSS.
C1P/C1N
C2P/C2N
I C1P/C1N - Pin for charge pump capacitor; Connect to each other with a capacitor.
C2P/C2N - Pin for charge pump capacitor; Connect to each other with a capacitor.
LS I Reserved pin. It should be connected to VSS.
BS[2:0] I MCU bus interface selection pins. Select appropriate logic setting as described in the following
table. BS2, BS1 and BS0 are pin select.
Table 5-2: Bus Interface selection
BS[2:0] Interface
000 4 line SPI
001 3 line SPI
010 I2C
110 8-bit 8080 parallel
100 8-bit 6800 parallel
Note
(1) 0 is connected to VSS
(2) 1 is connected to VDD
```

## Page 9

```text
SSD1315 Rev 1.1 P 9/36 Apr 2017 Solomon Systech
Pin Name Type Description
IREF I This is segment output current reference pin.
When external I REF is used, a resistor should be connected between this pin and V SS to maintain
the IREF current at 30uA. Please refer to Figure 6-15 for the details of resistor value.
When internal IREF is used, this pin should be kept NC.
FR O This pin outputs RAM write synchronization signal. Proper timing between MCU data writing
and frame display timing can be achieved to prevent tearing effect.
It should be kept NC if it is not used.
CL I This is external clock input pin.
When internal clock is enabled (i.e. HIGH in CLS pin), this pin is not used and should be
connected to VSS. When internal clock is disabled (i.e. LOW in CLS pin), this pin is the external
clock source input pin.
CLS I This is internal clock enable pin. When it is pulled HIGH (i.e. connect to V DD), internal clock is
enabled. When it is pulled LOW, the internal clock is disabled; an external clock source must be
connected to the CL pin for normal operation.
RES# I This pin is reset signal input. When the pin is pulled LOW, initialization of the chip is executed.
Keep this pin HIGH (i.e. connect to VDD) during normal operation.
CS# I This pin is the chip select input connecting to the MCU.
The chip is enabled for MCU communication only when CS# is pulled LOW (active LOW).
D/C# I This pin is Data/Command control pin connecting to the MCU.
When the pin is pulled HIGH, the data at D[7:0] will be interpreted as data.
When the pin is pulled LOW, the data at D[7:0] will be transferred to a command register.
In I2C mode, this pin acts as SA0 for slave address selection.
When 3-wire serial interface is selected, this pin must be connected to VSS.
For detail relationship to MCU interface signals, refer to Timing Characteristics Diagrams Figure
9-1 to Figure 9-3.
E (RD#) I This pin is MCU interface input.
When 6800 interface mode is selected, this pin will be used as the Enable (E) signal. Read/write
operation is initiated when this pin is pulled HIGH and the chip is selected.
When 8080 interface mode is selected, this pin receives the Read (RD#) signal. Read operation is
initiated when this pin is pulled LOW and the chip is selected.
When serial or I2C interface is selected, this pin must be connected to VSS.
R/W#(WR#) I This is read / write control input pin connecting to the MCU interface.
When interfacing to a 6800 -series microprocessor, this pin will be u sed as Read/Write (R/W#)
selection input. Read mode will be carried out when this pin is pulled HIGH (i.e. connect to VDD)
and write mode when LOW.
When 8080 interface mode is selected, this pin will be the Write (WR#) input. Data write
operation is initiated when this pin is pulled LOW and the chip is selected.
When serial or I2C interface is selected, this pin must be connected to VSS.
```

## Page 10

```text
Solomon Systech Apr 2017 P 10/36 Rev 1.1 SSD1315
Pin Name Type Description
D[7:0] IO These pins are bi-directional data bus connecting to the MCU data bus.
Unused pins are recommended to tie LOW.
When serial interface mode is selected, D2 should be either tied LOW or tied together with D1 as
the serial data input: SDIN, and D0 will be the serial clock input: SCLK.
When I2C mode is selected, D2, D1 should be tied together and serve as SDAout, SDAin in
application and D0 is the serial clock input, SCL.
TR[12:0] - Reserved pin. It should be kept NC.
SEG0 ~
SEG127
O These pins provide Segment switch signals to OLED panel. These pins are VSS state when display
is OFF.
COM0 ~
COM63
O These pins provide Common switch signals to OLED panel. They are in high impedance state
when display is OFF.
NC - This is dummy pin. It should be kept NC.
```

## Page 11

```text
SSD1315 Rev 1.1 P 11/36 Apr 2017 Solomon Systech
6 FUNCTIONAL BLOCK DESCRIPTIONS
6.1 MCU Interface Selection
SSD1315 MCU interface consist of 8 data pins and 5 control pins. The pin assignment at different interface
mode is summarized in Table 6-1. Different MCU mode can be set by hardware selection on BS[2:0] pins
(please refer to Table 5-2 for BS[2:0] setting).
Table 6-1: MCU interface assignment under different bus interface mode
Pin Name
Bus
Interface
Data/Command Interface Control Signal
D7 D6 D5 D4 D3 D2 D1 D0 E R/W# CS# D/C# RES#
8-bit 8080 D[7:0] RD# WR# CS# D/C# RES#
8-bit 6800 D[7:0] E R/W# CS# D/C# RES#
3-wire SPI Tie LOW SDIN(1) SCLK Tie LOW CS# Tie LOW RES#
4-wire SPI Tie LOW SDIN(1) SCLK Tie LOW CS# D/C# RES#
I2C Tie LOW SDAOUT SDAIN SCL Tie LOW SA0 RES#
Note: (1) In 3-wire SPI or 4-wire SPI interface, D2 should be either tied LOW or tied together with D1 as the serial data
input: SDIN.
6.1.1 MCU Parallel 6800-series Interface
The parallel interface consists of 8 bi-directional data pins (D[7:0]), R/W#, D/C#, E and CS#.
A LOW in R/W# indicates WRITE operation and HIGH in R/W# indicates READ operation.
A LOW in D/C# indicates COMMAND read/write and HIGH in D/C# indicates DATA read/write.
The E input serves as data latch signal while CS# is LOW. Data is latched at the falling edge of E signal.
Table 6-2: Control pins of 6800 interface
Function E R/W# CS# D/C#
Write command v L L L
Read status v H L L
Write data v L L H
Read data v H L H
Note
(1) v stands for falling edge of signal
H stands for HIGH in signal
L stands for LOW in signal
In order to match the operating frequency of display RAM with that of the microprocessor, some pipeline
processing is internally performed which requires the insertion of a dummy read before the first actual display
data read. This is shown in Figure 6-1.
```

## Page 12

```text
Solomon Systech Apr 2017 P 12/36 Rev 1.1 SSD1315
Figure 6-1: Data read back procedure - insertion of dummy read
N n n+1 n+2
R/W#
E
Databus
Write column
address Read 1st dataDummy read Read 2nd data Read 3rd data
6.1.2 MCU Parallel 8080-series Interface
The parallel interface consists of 8 bi-directional data pins (D[7:0]), RD#, WR#, D/C# and CS#.
A LOW in D/C# indicates COMMAND read/write and HIGH in D/C# indicates DATA read/write.
A rising edge of RD# input serves as a data READ latch signal while CS# is kept LOW.
A rising edge of WR# input serves as a data/command WRITE latch signal while CS# is kept LOW.
Figure 6-2: Example of Write procedure in 8080 parallel interface mode
CS#
WR#
D[7:0]
D/C#
RD#
high
low
Figure 6-3: Example of Read procedure in 8080 parallel interface mode
CS#
WR#
D[7:0]
D/C#
RD#
high
low
```

## Page 13

```text
SSD1315 Rev 1.1 P 13/36 Apr 2017 Solomon Systech
Table 6-3: Control pins of 8080 interface
Function RD# WR# CS# D/C#
Write command H ^ L L
Read status ^ H L L
Write data H ^ L H
Read data ^ H L H
Note
(1) ^ stands for rising edge of signal
(2) H stands for HIGH in signal
(3) L stands for LOW in signal
In order to match the operating frequency of display RAM with that of the microprocessor, some pipeline
processing is internally performed which requires the insertion of a dummy read before the first actual display
data read. This is shown in Figure 6-4.
Figure 6-4: Display data read back procedure - insertion of dummy read
N n n+1 n+2
WR#
RD#
Databus
Write column
address Read 1st dataDummy read Read 2nd data Read 3rd data
6.1.3 MCU Serial Interface (4-wire SPI)
The 4-wire serial interface consists of serial clock: SCLK, serial data: SDIN, D/C#, CS#. In 4-wire SPI mode,
D0 acts as SCLK, D1 and D2 are tied together to act as SDIN. For the unused data pins from D3 to D7,
E(RD#) and R/W#(WR#) can be connected to an external ground.
Table 6-4: Control pins of 4-wire Serial interface
Function E R/W# CS# D/C# D0
Write command Tie LOW Tie LOW L L ^
Write data Tie LOW Tie LOW L H ^
Note
(1) H stands for HIGH in signal
(2) L stands for LOW in signal
(3) ^ stands for rising edge of signal
SDIN is shifted into an 8-bit shift register on every rising edge of SCLK in the order of D7, D6, ..., D0. D/C#
is sampled on every eighth clock and D/C# should be kept stable throughout eight clock period. The data byte
in the shift register is written to the Graphic Display Data RAM (GDDRAM) or command register in the
same clock.
Under serial mode, only write operations are allowed.
```

## Page 14

```text
Solomon Systech Apr 2017 P 14/36 Rev 1.1 SSD1315
Figure 6-5: Write procedure in 4-wire Serial interface mode
6.1.4 MCU Serial Interface (3-wire SPI)
The 3-wire serial interface consists of serial clock SCLK, serial data SDIN and CS#.
In 3-wire SPI mode, D0 acts as SCLK, D1 and D2 are tied together to act as SDIN. For the unused data pins
from D3 to D7, R/W# (WR#), E(RD#) and D/C# can be connected to an external ground.
The operation is similar to 4-wire serial interface while D/C# pin is not used. There are altogether 9-bits will
be shifted into the shift register on every ninth clock in sequence: D/C# bit, D7 to D0 bit. The D/C# bit (first
bit of the sequential data) will determine the following data byte in the shift register is written to the Display
Data RAM (D/C# bit = 1) or the command register (D/C# bit = 0).
Under serial mode, only write operations are allowed.
Table 6-5: Control pins of 3-wire Serial interface
Function E(RD#) R/W#(WR#) CS# D/C# D0 Note
(1) L stands for LOW in signal
(2) ^ stands for rising edge of signalWrite command Tie LOW Tie LOW L Tie LOW ^
Write data Tie LOW Tie LOW L Tie LOW ^
Figure 6-6: Write procedure in 3-wire Serial interface mode
D7 D6 D5 D4 D3 D2 D1 D0
SCLK
(D0)
SDIN(D1)
DB1 DB2 DBn
CS#
D/C#
SDIN/
SCLK
D7 D6 D5 D4 D3 D2 D1 D0
SCLK
(D0)
SDIN(D1)
DB1 DB2 DBn
CS#
D/C#
SDIN/
SCLK
```

## Page 15

```text
SSD1315 Rev 1.1 P 15/36 Apr 2017 Solomon Systech
6.1.5 MCU I2C Interface
The I2C communication interface consists of slave address bit SA0, I2C-bus data signal SDA (SDAOUT/D2 for
output and SDAIN/D1 for input) and I2C-bus clock signal SCL (D0). Both the data and clock signals must be
connected to pull-up resistors. RES# is used for the initialization of device.
a) Slave address bit (SA0)
SSD1315 has to recognize the slave address before transmitting or receiving any information by the
I2C-bus. The device will respond to the slave address following by the slave address bit ("SA0" bit)
and the read/write select bit ("R/W#" bit) with the following byte format,
b7 b6 b5 b4 b3 b2 b1 b0
0 1 1 1 1 0 SA0 R/W#
"SA0" bit provides an extension bit for the slave address. Either "0111100" or "0111101", can be
selected as the slave address of SSD1315. D/C# pin acts as SA0 for slave address selection.
"R/W#" bit is used to determine the operation mode of the I2C-bus interface. R/W# = 1, it is in read
mode. R/W# = 0, it is in write mode.
b) I 2C-bus data signal (SDA)
SDA acts as a communication channel between the transmitter and the receiver. The data and the
acknowledgement are sent through the SDA.
It should be noticed that the ITO track resistance and the pulled-up resistance at "SDA" pin becomes
a voltage potential divider. As a result, the acknowledgement would not be possible to attain a valid
logic 0 level in "SDA".
"SDAIN" and "SDAOUT" are tied together and serve as SDA. The "SDAIN" pin must be connected to
act as SDA. The "SDAOUT" pin may be disconnected. When "SDAOUT" pin is disconnected, the
acknowledgement signal will be ignored in the I2C-bus.
c) I2C-bus clock signal (SCL)
The transmission of information in the I2C-bus is following a clock signal, SCL. Each transmission of
data bit is taken place during a single clock period of SCL.
```

## Page 16

```text
Solomon Systech Apr 2017 P 16/36 Rev 1.1 SSD1315
6.1.5.1 I2C-bus Write Data
The I2C-bus interface gives access to write data and command into the device. Please refer to for the write
mode of I2C-bus in chronological order.
Figure 6-7: I2C-bus data format
6.1.5.2 Write mode for I2C
1) The master device initiates the data communication by a start condition. The definition of the start
condition is shown in Figure 6-8. The start condition is established by pulling the SDA from HIGH to
LOW while the SCL stays HIGH.
2) The slave address is following the start condition for recognition use. For the SSD1315, the slave
address is either "b0111100" or "b0111101" by changing the SA0 to LOW or HIGH (D/C pin acts as
SA0).
3) The write mode is established by setting the R/W# bit to logic "0".
4) An acknowledgement signal will be generated after receiving one byte of data, including the slave
address and the R/W# bit. Please refer to the Figure 6-9 for the graphical representation of the
acknowledge signal. The acknowledge bit is defined as the SDA line is pulled down during the HIGH
period of the acknowledgement related clock pulse.
5) After the transmission of the slave address, either the control byte or the data byte may be sent across
the SDA. A control byte mainly consists of Co and D/C# bits following by six "0" 's.
a. If the Co bit is set as logic "0", the transmission of the following information will contain
data bytes only.
b. The D/C# bit determines the next data byte is acted as a command or a data. If the D/C# bit is
set to logic "0", it defines the following data byte as a command. If the D/C# bit is set to logic
"1", it defines the following data byte as a data which will be stored at the GDDRAM. The
GDDRAM column address pointer will be increased by one automatically after each data
write.
6) Acknowledge bit will be generated after receiving each control byte or data byte.
7) The write mode will be finished when a stop condition is applied. The stop condition is also defined
in Figure 6-8. The stop condition is established by pulling the "SDA in" from LOW to HIGH while
the "SCL" stays HIGH.

A0
P 0 1 1 1 1 0
m >= 0 words n >= 0 bytes
MSB ...................LSB
1 byte
Write mode
SSD1315
Slave Address
R/W#
D/C#
Co
ACK
K
ACK
Control byte Data byte Control byte
ACK
Data byte
ACK
S
SA0
R/W#
Co
D/C
#
ACK
Control byte
Note: Co - Continuation bit
D/C# - Data / Command Selection bit
ACK - Acknowledgement
SA0 - Slave address bit
R/W# - Read / Write Selection bit
S - Start Condition / P - Stop Condition
0 0 0 0 0 0
0 1 1 1 1 0
D/C#
Co
ACK
K
SA0
```

## Page 17

```text
SSD1315 Rev 1.1 P 17/36 Apr 2017 Solomon Systech
Figure 6-8: Definition of the Start and Stop Condition
Figure 6-9: Definition of the acknowledgement condition
Please be noted that the transmission of the data bit has some limitations.
1. The data bit, which is transmitted during each SCL pulse, must keep at a stable state within the "HIGH"
period of the clock pulse. Please refer to the Figure 6-10 for graphical representations. Except in start or
stop conditions, the data line can be switched only when the SCL is LOW.
2. Both the data line (SDA) and the clock line (SCL) should be pulled up by external resistors.
Figure 6-10: Definition of the data transfer condition
SDA
SCL
Data line is
stable
Change of
data
DATA OUTPUT
BY RECEIVER
DATA OUTPUT
BY TRANSMITTER
SCL FROM
MASTER
S
START
Condition
Clock pulse for acknowledgement
1 8 9
Non-acknowledge
2
Acknowledge
S
START condition
SDA
SCL
P
STOP condition
SDA
SCL
tHSTART tSSTOP
START
```

## Page 18

```text
Solomon Systech Apr 2017 P 18/36 Rev 1.1 SSD1315
6.2 Command Decoder
This module determines whether the input data is interpreted as data or command. Data is interpreted based
upon the input of the D/C# pin.
If D/C# pin is HIGH, D[7:0] is interpreted as display data written to Graphic Display Data RAM (GDDRAM).
If it is LOW, the input at D[7:0] is interpreted as a command. Then data input will be decoded and written to
the corresponding command register.
6.3 Oscillator Circuit and Display Time Generator
Figure 6-11: Oscillator Circuit and Display Time Generator
Divider
Internal
Oscillator
Fosc
M
U
XCL
CLK DCLK
Display
Clock
CLS
This module is an on-chip LOW power RC oscillator circuitry. The operation clock (CLK) can be generated
either from internal oscillator or external source CL pin. This selection is done by CLS pin. If CLS pin is
pulled HIGH, internal oscillator is chosen and CL should be connected to V SS. Pulling CLS pin LOW
disables internal oscillator and external clock must be connected to CL pins for proper operation. When the
internal oscillator is selected, its output frequency FOSC can be changed by command D5h A[7:4].
The display clock (DCLK) for the Display Timing Generator is deriv ed from CLK. The division factor "D"
can be programmed from 1 to 16 by command D5h
DCLK = FOSC / D
The frame frequency of display is determined by the following formula.
Mux of No. K D
FF osc
FRM
 
where
- D stands for clock divide ratio. It is set by command D5h A[3:0]. The divide ratio has the range from 1 to
16.
- K is the number of display clocks per row. The value is derived by
K = Phase 1 period + Phase 2 period + K0 = 2 + 2 + 99 = 103 at power on reset (i.e. K0 = 99)
Please refer to Section 6.6 for the details of the "Phase".
- Number of multiplex ratio is set by command A8h. The power on reset value is 63 (i.e. 64MUX).
- FOSC is the oscillator frequency. It can be changed by command D5h A[7:4]. The higher the register
setting results in higher frequency.
```

## Page 19

```text
SSD1315 Rev 1.1 P 19/36 Apr 2017 Solomon Systech
6.4 FR Synchronization
FR synchronization signal can be used to prevent tearing effect.
The starting time to write a new image to OLED driver is depended on the MCU writing speed. If MCU can
finish writing a frame image within one frame period, it is classified as fast write MCU. For MCU needs
longer writing time to complete (more than one frame but within two frames), it is a slow write one.
For fast write MCU: MCU should start to write new frame of ram data just after rising edge of FR pulse and
should be finished well before the rising edge of the next FR pulse.
For slow write MCU: MCU should start to write new frame ram data after the falling edge of the 1st FR pulse
and must be finished before the rising edge of the 3rd FR pulse.
6.5 Reset Circuit
When RES# input is LOW, the chip is initialized with the following status:
1. Display is OFF
2. 128 x 64 Display Mode
3. Normal segment and display data column address and row address mapping (SEG0 mapped to
address 00h and COM0 mapped to address 00h)
4. Shift register data clear in serial interface
5. Display start line is set at display RAM address 0
6. Column address counter is set at 0
7. Normal scan direction of the COM outputs
8. Contrast control register is set at 7Fh
9. Normal display mode (Equivalent to A4h command)
Fast write MCU
Slow write MCU
SSD1315 displaying memory updates to OLED screen
One frame
FR
100%

0%
Memory
Access
Process
Time
```

## Page 20

```text
Solomon Systech Apr 2017 P 20/36 Rev 1.1 SSD1315
6.6 Segment Drivers / Common Drivers
Segment drivers deliver 128 current sources to drive the OLED panel. The driving current can be adjusted by
altering the registers of the contrast setting command (81h). Common drivers generate voltage-scanning
pulses.
The segment driving waveform is divided into three phases:
1. In phase 1, the OLED pixel charges of previous image are discharged in order to prepare for next
image content display.
2. In phase 2, the OLED pixel is driven to the targeted voltage. The pixel is driven to attain the
corresponding voltage level from VSS. The period of phase 2 can be programmed in length from 1 to
16 DCLKs. If the capacitance value of the pixel of OLED panel is larger, a longer period is required
to charge up the capacitor to reach the desired voltage.
3. In phase 3, the OLED driver switches to use current source to drive the OLED pixels and this is the
current drive stage.
Figure 6-12: Segment Output Waveform in three phases
After finishing phase 3, the driver IC will go back to phase 1 to display the next row image data. This three-
step cycle is run continuously to refresh image display on OLED panel.
In phase 3, if the length of current drive pulse width is set to 99, after finishing 99 DCLKs in current drive
phase, the driver IC will go back to phase 1 for next row display.
Segment
VSS
Phase: 1 2 3 Time
```

## Page 21

```text
SSD1315 Rev 1.1 P 21/36 Apr 2017 Solomon Systech
6.7 Graphic Display Data RAM (GDDRAM)
The GDDRAM is a bit mapped static RAM holding the bit pattern to be displayed. The size of the RAM is
128 x 64 bits and the RAM is divided into eight pages, from PAGE0 to PAGE7, which are used for
monochrome 128x64 dot matrix display, as shown in Figure 6-13.
Figure 6-13: GDDRAM pages structure
Row re-mapping
PAGE0 (COM0-COM7) Page 0 PAGE0 (COM 63-COM56)
PAGE1 (COM8-COM15) Page 1 PAGE1 (COM 55-COM48)
PAGE2 (COM16-COM23) Page 2 PAGE2 (COM47-COM40)
PAGE3 (COM24-COM31) Page 3 PAGE3 (COM39-COM32)
PAGE4 (COM32-COM39) Page 4 PAGE4 (COM31-COM24)
PAGE5 (COM40-COM47) Page 5 PAGE5 (COM23-COM16)
PAGE6 (COM48-COM55) Page 6 PAGE6 (COM15-COM8)
PAGE7 (COM56-COM63) Page 7 PAGE7 (COM 7-COM0)
SEG0 ---------------------------------------------SEG127
Column re-mapping SEG127 ---------------------------------------------SEG0
When one data byte is written into GDDRAM, all the rows image data of the same page of the current column
are filled (i.e. the whole column (8 bits) pointed by the column address pointer is filled.). Data bit D0 is
written into the top row, while data bit D7 is written into bottom row as shown in Figure 6-14.
Figure 6-14: Enlargement of GDDRAM (No row re-mapping and column-remapping)

For mechanical flexibility, re-mapping on both Segment and Common outputs can be selected by software as
shown in Figure 6-13.
For vertical shifting of the display, an internal register storing the display start line can be set to control the
portion of the RAM data to be mapped to the display (command D3h).
LSB D0
MSB D7
Each box represents one bit of image data
....................PAGE2
COM16
COM17
:
:
:
:
:
COM23
SEG0
SEG1
SEG2
SEG3
SEG4
SEG123
SEG134
SEG125
SEG126
SEG127
....................
```

## Page 22

```text
Solomon Systech Apr 2017 P 22/36 Rev 1.1 SSD1315
6.8 SEG/COM Driving Block
This block is used to derive the incoming power sources into the different levels of internal use voltage and
current.
- VCC is the most positive voltage supply.
- VCOMH is the Common deselected level. It is internally regulated.
- VLSS is the ground path of the analog and panel current.
- IREF is a reference current source for segment current drivers ISEG. The relationship between reference
current and segment current of a color is:
ISEG = (Contrast / 32) x IREF
in which the contrast (1~255) is set by Set Contrast command 81h
When external I REF is used, the magnitude of I REF is controlled by the value of resistor, which is connected
between IREF pin and V SS as shown in Figure 6-15. It is recommended to set I REF to 30+/-2uA so as to achieve
ISEG = 240uA at maximum contrast 255.
Figure 6-15: IREF Current Setting by Resistor Value
Since the voltage at IREF pin is VCC - 3V, the value of resistor R1 can be found as below:
For IREF = 30uA, VCC =12V:
R1 = (Voltage at IREF - VSS) / IREF
 (12 - 3) / 30uA
= 300K
When internal IREF is used, the I REF pin should be kept NC and the I SEG can be set as either 150uA or 240uA
(max) by software command ADh setting. The selection of external or internal I REF is also controlled by
command ADh. For details, please refer to SSD1315 Command Table.
SSD1315
IREF (voltage at
this pin  VCC
- 3)
R1
VSS
IREF ~30uA
```

## Page 23

```text
SSD1315 Rev 1.1 P 23/36 Apr 2017 Solomon Systech
6.9 Power ON and OFF Sequence
The following figures illustrate the recommended power ON and power OFF sequence of SSD1315.
6.9.1 Power ON and OFF sequence with External VCC
Power ON sequence:
1. Power ON V DD
2. After V DD become stable, wait at least 20ms (t0), set RES# pin LOW (logic low) for at least 3us (t1) (4)
and then HIGH (logic high).
3. After set RES# pin LOW (logic low), wait for at least 3us (t 2). Then Power ON VCC.(1)
4. After V CC become stable, send command AFh for display ON. SEG/COM will be ON after 100ms
(tAF).
Figure 6-16: The Power ON Sequence
Power OFF sequence:
1. Send command AEh for display OFF.
2. Power OFF V CC.(1), (2)
3. Power OFF VDD after tOFF. (4) (where Minimum tOFF=0ms, typical tOFF=100ms)
Figure 6-17: The Power OFF Sequence
Note:
(1) VCC should be kept float (i.e. disable) when it is OFF.
(2) Power Pins (VDD, VCC) can never be pulled to ground under any circumstance.
(3) The register values are reset after t1.
(4) VDD should not be Power OFF before VCC Power OFF.
```

## Page 24

```text
Solomon Systech Apr 2017 P 24/36 Rev 1.1 SSD1315
6.9.2 Power ON and OFF sequence with Charge Pump Application
Power ON sequence:
1. Power ON V DD
2. Wait for t ON. Power ON VBAT. (where Minimum tON = 0ms)
3. After V DD become stable, wait at least 20ms (t0), set RES# pin LOW (logic low) for at least 3us (t1) (3)
and then HIGH (logic high).
4. After set RES# pin LOW (logic low), wait for at least 3us (t 2). Then input commands with below
sequence:
a. 8Dh for enabling internal charge pump
b. AFh for display ON
5. SEG/COM will be ON after 100ms (t AF).
Figure 6-18: The Power ON sequence with Charge Pump Application
Power OFF sequence:
1. Send command AEh for display OFF
2. Send command 8Dh 10h to disable charge pump
3. Power OFF VBAT after tOFF. . (1), (2) (Typical tOFF =100ms)
4. Power OFF VDD after tOFF2. (where Minimum tOFF2 = 0ms (4) , Typical tOFF2=5ms)
Figure 6-19: The Power OFF sequence with Charge Pump Application
Note:
(1) VBAT should be kept float (i.e. disable) when it is OFF.
(2) Power Pins (VBAT) can never be pulled to ground under any circumstance.
(3) The register values are reset after t1.
(4) VDD should not be Power OFF before VBAT Power OFF.
```

## Page 25

```text
SSD1315 Rev 1.1 P 25/36 Apr 2017 Solomon Systech
6.10 Charge Pump Regulator
The internal regulator circuit in SSD1315 accompanying only 2 external capacitors can generate a maximum
of 9.0V voltage supply, VCC and a maximum output loading of 12mA from a low voltage supply input, V BAT.
In SSD1315, there are 3 charge pump output V CC setting, 7.5V, 8.5V and 9V, which can be selected by
software command 8Dh setting. The V CC is the voltage supply to the OLED driver block. This regulator can
be turned ON/OFF by software command 8Dh setting. For details, please refer to SSD1315 Command Table.
```

## Page 26

```text
Solomon Systech Apr 2017 P 26/36 Rev 1.1 SSD1315
7 MAXIMUM RATINGS
Table 7-1: Maximum Ratings
(Voltage Reference to VSS)
Symbol Parameter Value Unit
VDD
Supply Voltage
-0.3 to +4 V
VBAT -0.3 to +6 V
VCC 0 to 18 V
VSEG SEG output voltage 0 to VCC V
VCOM COM output voltage 0 to 0.9*VCC V
Vin Input voltage VSS-0.3 to VDD+0.3 V
TA Operating Temperature -40 to +85 oC
Tstg Storage Temperature Range -65 to +150 oC
*Maximum Ratings are those values beyond which damage to the device may occur. Functional operation should be restricted to the
limits in the Electrical Characteristics tables or Pin Description.
*This device may be light sensitive. Caution should be taken to avoid exposure of this device to any light source during normal
operation. This device is not radiation protected.
```

## Page 27

```text
SSD1315 Rev 1.1 P 27/36 Apr 2017 Solomon Systech
8 DC CHARACTERISTICS
Condition (Unless otherwise specified):
Voltage referenced to VSS
VDD = 1.65V to 3.5V
TA = 25C
Table 8-1: DC Characteristics
Symbol Parameter Test Condition Min Typ Max Unit
VCC Operating Voltage - 7.5 - 16.5 V
VDD Logic Supply Voltage - 1.65 - 3.5 V
VBAT Charge Pump Regulator Supply
Voltage - 3.0 - 4.5 V
Charge
Pump VCC Charge Pump Output Voltage
7.5V mode 7 7.5 -
V 8.5V mode 8 8.5 -
9V mode 8.5 9 -
VOH High Logic Output Level IOUT = 100uA, 3.3MHz 0.9 x VDD - - V
VOL Low Logic Output Level IOUT = 100uA, 3.3MHz - - 0.1 x VDD V
VIH High Logic Input Level - 0.8 x VDD - - V
VIL Low Logic Input Level - - - 0.2 x VDD V
ICC, SLEEP ICC, Sleep mode Current VDD = 1.65V~3.5V, VCC = 7.5V~16.5V
Display OFF, No panel attached - - 10 uA
IDD, SLEEP IDD, Sleep mode Current VDD = 1.65V~3.5V, VCC = 7.5V~16.5V
Display OFF, No panel attached - - 10 uA
IBAT, SLEEP IBAT, Sleep mode Current VDD = 1.65V~3.5V, VBAT = 3V~4.5V
Display OFF, No panel attached - - 10 uA
ICC
VCC Supply Current
VDD = 2.8V, VCC = 12V, IREF = 30uA
No loading, Display ON, All ON Contrast = FFh
- 625 1000 uA
IDD VDD Supply Current
VDD = 2.8V, VCC = 12V, IREF = 30uA
No loading, Display ON, All ON
- 240 330 uA
ISEG
Segment Output Current
VDD=2.8V, VCC=12V, IREF=30uA,
Display ON.
Contrast=FFh - 240 -
uA Contrast=AFh - 165 -
Contrast=3Fh - 60 -
ISEG
Segment Output Current
VDD=2.8V, VCC=12V, IREF=19uA,
Display ON.
Contrast=FFh - 150 -
uA Contrast=AFh - 104 -
Contrast=3Fh - 38 -
Dev Segment output current uniformity
Dev = (ISEG - IMID)/IMID
IMID = (IMAX + IMIN)/2
ISEG[0:127] = Segment current at
contrast = FFh
-3 - +3 %
Adj. Dev Adjacent pin output current uniformity
(contrast = FF) Adj Dev = (I[n]-I[n+1]) / (I[n]+I[n+1]) -2 - +2 %
```

## Page 28

```text
Solomon Systech Apr 2017 P 28/36 Rev 1.1 SSD1315
9 AC CHARACTERISTICS
Conditions:
Voltage referenced to VSS
VDD=1.65 to 3.5V
TA = 25C
Table 9-1: AC Characteristics
Symbol Parameter Test Condition Min Typ Max Unit
FOSC (1) Oscillation Frequency of
Display Timing Generator
VDD = 2.8V 620 688 756 kHz
FFRM Frame Frequency 128x64 Graphic Display Mode, Display
ON, Internal Oscillator Enabled
- FOSC x 1/(DxKx64)(2) - Hz
RES# Reset low pulse width 3 - - us
Note
(1) FOSC stands for the frequency value of the internal oscillator and the value is measured when command D5h A[7:4] is
in default value.
(2) D: divide ratio (default value = 1)
K: number of display clocks per row period (default value = 103)
```

## Page 29

```text
SSD1315 Rev 1.1 P 29/36 Apr 2017 Solomon Systech
Table 9-2: 6800-Series MCU Parallel Interface Timing Characteristics
(VDD - VSS = 1.65V to 3.5V, TA = 25 deg C)
Symbol Parameter Min Typ Max Unit
tcycle Clock Cycle Time 300 - - ns
tAS Address Setup Time 5 - - ns
tAH Address Hold Time 0 - - ns
tDSW Write Data Setup Time 40 - - ns
tDHW Write Data Hold Time 20 - - ns
tDHR Read Data Hold Time 20 - - ns
tOH Output Disable Time - - 70 ns
tACC Access Time - - 180 ns
PWCSL Chip Select Low Pulse Width (read)
Chip Select Low Pulse Width (write)
180
60 - - ns
PWCSH Chip Select High Pulse Width (read)
Chip Select High Pulse Width (write)
60
60 - - ns
tR Rise Time - - 40 ns
tF Fall Time - - 40 ns
Figure 9-1: 6800-series MCU parallel interface characteristics
D[7:0](WRITE)
D[7:0](READ)
E
CS#
R/W#
PWCSL
tR
tF tDHW
tOH
tACC tDHR
Valid Data
tDSW
Valid Data
tcycle
PWCSH
tAHtAS
D/C#
```

## Page 30

```text
Solomon Systech Apr 2017 P 30/36 Rev 1.1 SSD1315
Table 9-3: 8080-Series MCU Parallel Interface Timing Characteristics
(VDD - VSS = 1.65V ~3.5V, TA = 25 deg C)
Symbol Parameter Min Typ Max Unit
tcycle Clock Cycle Time 300 - - ns
tAS Address Setup Time 10 - - ns
tAH Address Hold Time 0 - - ns
tDSW Write Data Setup Time 40 - - ns
tDHW Write Data Hold Time 20 - - ns
tDHR Read Data Hold Time 20 - - ns
tOH Output Disable Time - - 70 ns
tACC Access Time - - 180 ns
tPWLR Read Low Time 180 - - ns
tPWLW Write Low Time 60 - - ns
tPWHR Read High Time 60 - - ns
tPWHW Write High Time 60 - - ns
tR Rise Time - - 40 ns
tF Fall Time - - 40 ns
tCS Chip select setup time 0 - - ns
tCSH Chip select hold time to read signal 0 - - ns
tCSF Chip select hold time 20 - - ns
Figure 9-2: 8080-series parallel interface characteristics
WR#
D[7:0]
tAS
D/C#
CS#
tCS
tAH
tPWLW
tcycle
tDSW tDHW
tPWHW
tCSF
RD#
D[7:0]
tAS
D/C#
CS#
tCS
tAH
tPWLR
tcycle
tPWHR
tCSH
tACC tDHR
tOH
tF tR tF tR
Write cycle (Form 1) Read cycle (Form 1)
Write Cycle

WR#
D[7:0]
tAS
D/C#
CS#
tCS
tAH
tPWLW
tcycle
tDSW tDHW
tPWHW
tCSF
RD#
D[7:0]
tAS
D/C#
CS#
tCS
tAH
tPWLR
tcycle
tPWHR
tCSH
tACC tDHR
tOH
tF tR tF tR
Write cycle (Form 1) Read cycle (Form 1)
Read Cycle
```

## Page 31

```text
SSD1315 Rev 1.1 P 31/36 Apr 2017 Solomon Systech
Table 9-4: Serial Interface Timing Characteristics (4-wire SPI)
(VDD - VSS = 1.65V~3.5V, TA = 25 deg C)
Symbol Parameter Min Typ Max Unit
tcycle Clock Cycle Time 100 - - ns
tAS Address Setup Time 15 - - ns
tAH Address Hold Time 15 - - ns
tCSS Chip Select Setup Time 20 - - ns
tCSH Chip Select Hold Time 20 - - ns
tDSW Write Data Setup Time 15 - - ns
tDHW Write Data Hold Time 25 - - ns
tCLKL Clock Low Time 30 - - ns
tCLKH Clock High Time 30 - - ns
tR Rise Time - - 40 ns
tF Fall Time - - 40 ns
Figure 9-3: Serial interface characteristics (4-wire SPI)
t AH t AS
D/C#
Valid Data
t DHW
t CLKL
t DSW
t CLKH
t cycle
t CSS t CSH
t F t R
SDIN(D1)
CS#
SCLK(D0)
D7
SDIN(D1)
CS#
SCLK(D0)
D6 D5 D4 D3 D2 D1 D0
```

## Page 32

```text
Solomon Systech Apr 2017 P 32/36 Rev 1.1 SSD1315
Table 9-5: Serial Interface Timing Characteristics (3-wire SPI)
(VDD - VSS = 1.65V~3.5V, TA = 25 deg C)
Symbol Parameter Min Typ Max Unit
tcycle Clock Cycle Time 100 - - ns
tCSS Chip Select Setup Time 20 - - ns
tCSH Chip Select Hold Time 20 - - ns
tDSW Write Data Setup Time 15 - - ns
tDHW Write Data Hold Time 25 - - ns
tCLKL Clock Low Time 30 - - ns
tCLKH Clock High Time 30 - - ns
tR Rise Time - - 40 ns
tF Fall Time - - 40 ns
Figure 9-4: Serial interface characteristics (3-wire SPI)
Valid Data
t
t CLKL
t DSW
t CLKH
t CYCLE
t CSS t CSH
t F t R
SDIN
(D1)
CS#
SCLK
(D0)
DHW
SDIN
(D1)
CS#
SCLK
(D0)
D7 D6 D5 D4 D3 D2 D1 D0 D/C#
```

## Page 33

```text
SSD1315 Rev 1.1 P 33/36 Apr 2017 Solomon Systech
Table 9-6: I2C Interface Timing Characteristics
Symbol Parameter Min Typ Max Unit
tcycle Clock Cycle Time 2.5 - - us
tHSTART Start condition Hold Time 0.6 - - us
tHD Data Hold Time (for "SDAOUT" pin) 0 - - ns
Data Hold Time (for "SDAIN" pin) 300 - - ns
tSD Data Setup Time 100 - - ns
tSSTART Start condition Setup Time (Only relevant for a repeated
Start condition)
0.6 - - us
tSSTOP Stop condition Setup Time 0.6 - - us
tR Rise Time for data and clock pin - - 300 ns
tF Fall Time for data and clock pin - - 300 ns
tIDLE Idle Time before a new transmission can start 1.3 - - us
Figure 9-5 I2C interface Timing characteristics
// //
SDA
SCL
tHSTART
tCYCLE
tHD
tR
tF
tSD tSSTART tSSTOP
tIDLE
```

## Page 34

```text
Solomon Systech Apr 2017 P 34/36 Rev 1.1 SSD1315
The configuration for I2C interface mode is shown in the following diagram:
(VDD= 2.8V, VCC =12V, IREF =30uA)
Pin connected to MCU interface: RES#, SDA, SCL
Pin internally connected to VSS: BS0, BS2, CL, D[7:3], E, R/W#, CS#, D/C#, BGGND, LS
Pin internally connected to VDD: BS1, CLS
VBAT, C1P, C1N, C2P, C2N, FR should be left open.
C1, C2: 2.2uF (1)
C3: 1.0uF (1)
RP : Pull up resistor
Voltage at IREF = VCC - 3V. For VCC = 12V, IREF = 30uA:
R1 = (Voltage at IREF - VSS) / IREF
= (12-3) / 30uA
= 300K
Note
(1) The capacitor value is recommended value. Select appropriate value against module application.
(2) VLSS of IC pad no. 29-30 are recommended to be connected to the VLSS of pad no. 77-78 to form a larger area of GND.
(3) VLSS and VSS are not recommended to be connected on the ITO routing, but connected together in the PCB level at one
common ground point for better grounding and noise insulation.
10 APPLICATION EXAMPLE
Figure 10-1: Application Example of SSD1315 with External VCC and I2C interface
DISPLAY PANEL
128 x 64
COM62
COM60
.
.
COM2
COM0
SEG0
.
.
.
.
.
.
.
.
.
.
.
.
.
SEG127
COM1
COM3
.
.
COM61
COM63
SSD1315
VCC VCOMH IREF D2 D1 D0 RES# VDD VSS
R1
VCC SDA SCL RES# V DD VSS
C3
C1
C2
RP RP
```

## Page 35

```text
SSD1315 Rev 1.1 P 35/36 Apr 2017 Solomon Systech
The configuration for I2C interface mode is shown in the following diagram:
(VDD= 1.65V ~ 3.5V, VBAT=3.0V~4.5V, IREF=19uA)
Pin connected to MCU interface: RES#, SDA, SCL
Pin internally connected to VSS: BS0, BS2, CL, D[7:3], E, R/W#, CS#, D/C#, BGGND, LS
Pin internally connected to VDD: BS1, CLS
FR should be left open.
C1, C2: 2.2uF (1)
C3, C4 : 1.0uF (1)
C5, C6: 1.0uF/10V (1)
RP : Pull up resistor
Voltage at IREF = VCC - 3V. For VCC = 7.5V, IREF = 19uA:
R1 = (Voltage at IREF - VSS) / IREF
= (7.5-3) / 19uA
~= 240K
Note
(1) The capacitor value is recommended value. Select appropriate value against module application.
(2) VLSS of IC pad no. 29-30 are recommended to be connected to the VLSS of pad no. 77-78 to form a larger area of GND.
(3) VLSS and VSS are not recommended to be connected on the ITO routing, but connected together in the PCB level at one
common ground point for better grounding and noise insulation.
Figure 10-2: Application Example of SSD1315 with Internal Charge Pump and I2C interface
DISPLAY PANEL
128 x 64
COM62
COM60
.
.
COM2
COM0
SEG0
.
.
.
.
.
.
.
.
.
.
.
.
SEG127
COM1
COM3
.
.
COM61
COM63
SSD1315
VCC VCOMH IREF D2 D1 D0 RES# VDD VBAT VSS C1N C1P C2N C2P
R1
SDA SCL RES# VDD VBAT VSS
C3
C4 C1
C2
C5 C6
RP RP
```

## Page 36

```text
Solomon Systech Apr 2017 P 36/36 Rev 1.1 SSD1315
Solomon Systech reserves the right to make changes without notice to any products herein. Solomon Systech makes no warranty,
representation or guarantee regarding the suitability of its products for any particular purpose, nor does Solomon Systech assume any
liability arising out of the application or use of any product or circuit, and specifically disclaims any, and all, liability, including without
limitation consequential or incidental damages. "Typical" p arameters can and do vary in different applications. All operating parameters,
including "Typical" must be validated for each customer application by the customer's technical experts. Solomon Systech does not con-
vey any license under its patent rights nor the rights of others. Solomon Systech products are not designed, intended, or authorized for use
as components in systems intended for surgical implant into the body, or other applications intended to support or sustain life, or for any
other application in which the failure of the Solomon Systech product could create a situation where personal injury or death may occur.
Should Buyer purchase or use Solomon Systech products for any such unintended or unauthorized application, Buyer shall indemnify and
hold Solomon Systech and its offices, employees, subsidiaries, affiliates, and distributors harmless against all claims, costs, damages, and
expenses, and reasonable attorney fees arising out of, directly or indirectly, any claim of personal injury or death associated with such
unintended or unauthorized use, even if such claim alleges that Solomon Systech was negligent regarding the design or manufacture of the
part.
The product(s) listed in this datasheet comply with Directive 2011/65/EU of the European Parliament and of the council of 8 June 2011 on the
restriction of the use of certain hazardous substances in electrical and electronic equipment and People's Republic of China Electronic Industry
Standard GB/T 26572-2011 "Requirements for concentration l imits for certain hazardous substances in electronic information products ( 
)". Hazardous Substances test report is available upon request.
http://www.solomon-systech.com
```

## Page 37

```text
SSD1315 Rev 1.0 P 5/34 Dec 2016 Solomon Systech
Appendix IV: Command Table and Command Descriptions
1 COMMAND TABLE
Table 1-1: SSD1315 Command Table
(D/C#=0, R/W#(WR#) = 0, E(RD#=1) unless specific setting is stated)
Fundamental Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 00~0F 0 0 0 0 X3 X2 X1 X0 Set Lower Column
Start Address for
Page Addressing
Mode
Set the lower nibble of the column start address
register for Page Addressing Mode using X[3:0] as
data bits. The initial display line register is reset to
0000b after RESET.
Note
(1) This command is only for page addressing mode
0 10~17 0 0 0 1 0 X2 X1 X0 Set Higher
Column Start
Address for Page
Addressing Mode
Set the higher nibble of the column start address
register for Page Addressing Mode using X[2:0] as
data bits. The initial display line register is reset to
0000b after RESET.
Note
(1) This command is only for page addressing mode
0 20 0 0 1 0 0 0 0 0 Set Memory
Addressing Mode
A[1:0] = 00b, Horizontal Addressing Mode
A[1:0] = 01b, Vertical Addressing Mode
A[1:0] = 10b, Page Addressing Mode (RESET)
A[1:0] = 11b, Invalid
0 A[1:0] 0 0 0 0 0 0 A1 A0
0 21 0 0 1 0 0 0 0 1 Set Column
Address
Setup column start and end address
A[6:0] : Column start address, range : 0-127d,
(RESET=0d)
B[6:0]: Column end address, range : 0-127d,
(RESET =127d)
Note
(1) This command is only for horizontal or vertical
addressing mode.
0 A[6:0] * A6 A5 A4 A3 A2 A1 A0
0 B[6:0] * B6 B5 B4 B3 B2 B1 B0
0 22 0 0 1 0 0 0 1 0 Set Page Address Setup page start and end address
A[2:0] : Page start Address, range : 0-7d,
(RESET = 0d)
B[2:0] : Page end Address, range : 0-7d,
(RESET = 7d)
Note
(1) This command is only for horizontal or vertical
addressing mode.
0 A[2:0] 0 0 0 0 0 A2 A1 A0
0 B[2:0] 0 0 0 0 0 B2 B1 B0
0 40~7F 0 1 X5 X4 X3 X2 X1 X0 Set Display Start
Line
Set display RAM display start line register from 0-
63 using X5 X4X3X2X1X0.
Display start line register is reset to 000000b
during RESET.
```

## Page 38

```text
Solomon Systech Dec 2016 P 6/34 Rev 1.0 SSD1315
Fundamental Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 81 1 0 0 0 0 0 0 1 Set Contrast
Control
Double byte command to select one of the contrast
steps. Contrast increases as the value increases.
(RESET = 7Fh)
A[7:0] valid range: 01h to FFh
0 A[7:0] A7 A6 A5 A4 A3 A2 A1 A0
0 A0/A1 1 0 1 0 0 0 0 X0 Set Segment Re-
map
A0h, X[0]=0b: column address 0 is mapped to
SEG0 (RESET)
A1h, X[0]=1b: column address 127 is mapped to
SEG0
0 A4/A5 1 0 1 0 0 1 0 X0 Entire Display ON A4h, X0=0b: Resume to RAM content display
(RESET)
Output follows RAM content
A5h, X0=1b: Entire display ON
Output ignores RAM content
0 A6/A7 1 0 1 0 0 1 1 X0 Set
Normal/Inverse
Display
A6h, X[0]=0b: Normal display (RESET)
0 in RAM: OFF in display panel
1 in RAM: ON in display panel
A7h, X[0]=1b: Inverse display
0 in RAM: ON in display panel
1 in RAM: OFF in display panel
0 A8 1 0 1 0 1 0 0 0 Set Multiplex
Ratio
Set MUX ratio to N+1 MUX
N=A[5:0] : from 16MUX to 64MUX.
RESET = 111111b (i.e. 63d, 64MUX)
A[5:0] from 0 to 14 are invalid entry
0 A[7:0] * * A5 A4 A3 A2 A1 A0
0
0
AD
A[5:4]
1
0
0
0
1
A5
0
A4
1
0
1
0
0
0
1
0
Internal IREF
Setting
Select external or internal IREF:
A[4] = '0': Select external IREF (RESET)
A[4] = '1': Enable internal IREF during display ON
Internal IREF value setting:
A[5] = '0': Internal IREF setting: 19uA, output a
maximum ISEG=150uA (RESET)
A[5] = '1': Internal IREF setting: 30uA, output a
maximum ISEG=240uA
0 AE/AF 1 0 1 0 1 1 1 X0 Set Display
ON/OFF
AEh, X[0]=0b: Display OFF (sleep mode)
(RESET)
AFh X[0]=1b: Display ON in normal mode
0 B0~B7 1 0 1 1 0 X2 X1 X0 Set Page Start
Address for Page
Addressing Mode
Set GDDRAM Page Start Address
PAGE0~PAGE7 for Page Addressing Mode
using X[2:0].
Note
(1) This command is only for page addressing mode
```

## Page 39

```text
SSD1315 Rev 1.0 P 7/34 Dec 2016 Solomon Systech
Fundamental Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 C0/C8 1 1 0 0 X3 0 0 0 Set COM Output
Scan Direction
C0h, X[3]=0b: normal mode (RESET) Scan from
COM0 to COM[N -1]
C8h, X[3]=1b: remapped mode. Scan from
COM[N-1] to COM0
Where N is the Multiplex ratio.
0 D3 1 1 0 1 0 0 1 1 Set Display Offset Set vertical shift by COM from 0d~63d.
The value is reset to 00h after RESET. 0 A[5:0] * * A5 A4 A3 A2 A1 A0
0 D5 1 1 0 1 0 1 0 1 Set Display Clock
Divide
Ratio/Oscillator
Frequency
A[3:0] : Define the divide ratio (D) of the display
clocks (DCLK):
Divide ratio= A[3:0] + 1, RESET is 0000b
(divide ratio = 1)
A[7:4] : Set the Oscillator Frequency, FOSC.
Oscillator Frequency increases with the
value of A[7:4] and vice versa. RESET
is 1000b.
Range: 0000b~1111b. Frequency increases
as setting value increases.
0 A[7:0] A7 A6 A5 A4 A3 A2 A1 A0
0 D9 1 1 0 1 1 0 0 1 Set Pre-charge
Period
A[3:0] : Phase 1 period of up to 30 DCLK
(i.e. 2, 4, 6, ...30)
Clocks 0 is invalid entry (RESET=2h)
A[7:4] : Phase 2 period of up to 30 DCLK
(i.e. 2, 4, 6, ...30)
Clocks 0 is invalid entry (RESET=2h )
0 A[7:0] A7 A6 A5 A4 A3 A2 A1 A0
0 DA 1 1 0 1 1 0 1 0 Set COM Pins
Hardware
Configuration
A[4]=0b, Sequential COM pin configuration
A[4]=1b (RESET), Alternative COM pin
Configuration
A[5]=0b (RESET), Disable COM Left/Right remap
A[5]=1b, Enable COM Left/Right remap
0 A[5:4] 0 0 A5 A4 0 0 1 0
0 DB 1 1 0 1 1 0 1 1 Set VCOMH select
Level
Set COM select voltage level.
A[5:4] Hex
code
V COMH deselect level
00b 00h ~ 0.65 x VCC
01b 10h ~ 0.71 x VCC
10b 20h ~ 0.77 x VCC (RESET)
11b 30h ~ 0.83 x VCC
0 A[5:4] 0 0 A5 A4 0 0 0 0
0 E3 1 1 1 0 0 0 1 1 NOP Command for no operation
```

## Page 40

```text
Solomon Systech Dec 2016 P 8/34 Rev 1.0 SSD1315
Internal Charge Pump Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 8D 1 0 0 0 1 1 0 1 Charge
Pump
Setting
Enable / Disable internal charge pump:
A[2] = 0b, Disable charge pump (RESET)
A[2] = 1b, Enable charge pump during display on
Note
(1) The Charge Pump must be enabled by the
following command sequence:
8Dh ; Charge Pump Setting
14h / 94h / 95h ; Enable Charge Pump
AFh; Display ON
A[7] A[0] Hex code Charge Pump Mode
0b 0b 14h 7.5V (RESET)
1b 0b 94h 8.5V
1b 1b 95h 9.0V
0 A[7:0] A7 0 0 1 0 A2 0 A0
```

## Page 41

```text
SSD1315 Rev 1.0 P 9/34 Dec 2016 Solomon Systech
Scrolling Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 26/27 0 0 1 0 0 1 1 X0 Continuous
Horizontal Scroll
Setup
26h, X[0]=0, Right Horizontal Scroll
27h, X[0]=1, Left Horizontal Scroll
(Horizontal scroll by 1 column)
A[7:0] : Dummy byte (Set as 00h)
B[2:0] : Define start page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
C[2:0] : Set time interval between each scroll step in
terms of frame frequency
000b - 6 frames 100b - 3 frames
001b - 32 frames 101b - 4 frames
010b - 64 frames 110b - 5 frame
011b - 128 frames 111b - 2 frame
D[2:0] : Define end page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
E[6:0] : Define start column address (RESET = 00h)
F[6:0] : Define end column address (RESET = 7Fh)
Notes:
(1) The value of D[2:0] must be larger than or equal to
B[2:0]
(2) The value of F[6:0] must be larger than or equal to
E[6:0]
0 A[7:0] 0 0 0 0 0 0 0 0
0 B[2:0] 0 0 0 0 0 B2 B1 B0
0 C[2:0] 0 0 0 0 0 C2 C1 C0
0 D[2:0] 0 0 0 0 0 D2 D1 D0
0 E[7:0] 0 E6 E5 E4 E3 E2 E1 E0
0 F[7:0] 0 F6 F5 F4 F3 F2 F1 F0
```

## Page 42

```text
Solomon Systech Dec 2016 P 10/34 Rev 1.0 SSD1315
Scrolling Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 29/2A 0 0 1 0 1 0 X1 X0 Continuous
Vertical and
Horizontal Scroll
Setup
29h, X1X0=01b : Vertical and Right Horizontal Scroll
2Ah, X1X0=10b : Vertical and Left Horizontal Scroll
A[0] : Set number of column scroll offset
0b No horizontal scroll
1b Horizontal scroll by 1 column
B[2:0] : Define start page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
C[2:0] : Set time interval between each scroll step in
terms of frame frequency
000b - 6 frames 100b - 3 frames
001b - 32 frames 101b - 4 frames
010b - 64 frames 110b - 5 frame
011b - 128 frames 111b - 2 frame
D[2:0] : Define end page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
E[5:0] : Vertical scrolling offset
e.g. E[5:0]= 01h refer to offset =1 row
E[5:0] =3Fh refer to offset =63 rows
F[6:0] : Define the start column address (RESET =
00h)
G[6:0] : Define the end column address (RESET =
7Fh)
Note
(1) The value of D[2:0] must be larger than or equal
to B[2:0]
(2) The value of E[5:0] must be less than B[6:0] in
A3h
(3) The value of G[6:0] must be larger than or equal
to F[6:0]
0 A[2:0] 0 0 0 0 0 0 0 A0
0 B[2:0] 0 0 0 0 0 B2 B1 B0
0 C[2:0] 0 0 0 0 0 C2 C1 C0
0 D[2:0] 0 0 0 0 0 D2 D1 D0
0 E[5:0] 0 0 E5 E4 E3 E2 E1 E0
0 F[5:0] 0 F6 F5 F4 F3 F2 F1 F0
0 G[5:0] 0 G6 G5 G4 G3 G2 G1 G0
```

## Page 43

```text
SSD1315 Rev 1.0 P 11/34 Dec 2016 Solomon Systech
Scrolling Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 2E 0 0 1 0 1 1 1 0 Deactivate scroll Stop scrolling that is configured by command
26h/27h/29h/2Ah.
Note
(1) After sending 2Eh command to deactivate the scrolling
action, the ram data needs to be rewritten.
0 2F 0 0 1 0 1 1 1 1 Activate scroll Start scrolling that is configured by the scrolling setup
commands :26h/27h/29h/2Ah with the following valid
sequences:
Valid command sequence 1: 26h ;2Fh.
Valid command sequence 2: 27h ;2Fh.
Valid command sequence 3: 29h ;2Fh.
Valid command sequence 4: 2Ah ;2Fh.
For example, if "26h; 2Ah; 2Fh." commands are
issued, the setting in the last scrolling setup command,
i.e. 2Ah in this case, will be executed. In other words,
setting in the last scrolling setup command overwrites
the setting in the previous scrolling setup commands.
0 A3 1 0 1 0 0 0 1 1 Set Vertical Scroll
Area
A[5:0] : Set No. of rows in top fixed area. The No. of
rows in top fixed area is referenced to the top
of the GDDRAM (i.e. row 0). [RESET = 0]
B[6:0] : Set No. of rows in scroll area. This is the
number of rows to be used for vertical
scrolling. The scroll area starts in the first
row below the top fixed area. [RESET = 64]
Note
(1) A[5:0]+B[6:0] <= MUX ratio
(2) B[6:0] <= MUX ratio
(3a) Vertical scrolling offset (E[5:0] in 29h/2Ah) <
B[6:0]
(3b) Set Display Start Line (X5X4X3X2X1X0 of
40h~7Fh) < B[6:0]
(4) The last row of the scroll area shifts to the first row
of the scroll area.
(5) For 64d MUX display
A[5:0] = 0, B[6:0]=64 : whole area scrolls
A[5:0] = 0, B[6:0] < 64 : top area scrolls
A[5:0] + B[6:0] < 64 : central area scrolls
A[5:0] + B[6:0] = 64 : bottom area scrolls
0 A[5:0] 0 0 A5 A4 A3 A2 A1 A0
0 B[6:0] 0 B6 B5 B4 B3 B2 B1 B0
```

## Page 44

```text
Solomon Systech Dec 2016 P 12/34 Rev 1.0 SSD1315
Scrolling Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 2C/2D 0 0 1 0 1 1 0 X0 Content Scroll
Setup
2Ch, X[0]=0, Right Horizontal Scroll by one column
2Dh, X[0]=1, Left Horizontal Scroll by one column
A[7:0] : Dummy byte (Set as 00h)
B[2:0] : Define start page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
C[7:0] : Dummy byte (Set as 01h)
D[2:0] : Define end page address
000b - PAGE0 011b - PAGE3 110b - PAGE6
001b - PAGE1 100b - PAGE4 111b - PAGE7
010b - PAGE2 101b - PAGE5
E[6:0] : Define start column address (RESET = 00h)
F[6:0] : Define end column address (RESET = 7Fh)
Note
(1) The value of D[2:0] must be larger than or equal to
B[2:0]
(2) The value of F[6:0] must be larger than E[6:0]
(3) A delay time of 2 frame frequency must be set if
sending the command of 2Ch / 2Dh consecutively
0 A[7:0] 0 0 0 0 0 0 0 0
0 B[2:0] 0 0 0 0 0 B2 B1 B0
0 C[7:0] 0 0 0 0 0 0 0 1
0 D[2:0] 0 0 0 0 0 D2 D1 D0
0 E[7:0] 0 E6 E5 E4 E3 E2 E1 E0
0 F[7:0] 0 F6 F5 F4 F3 F2 F1 F0
```

## Page 45

```text
SSD1315 Rev 1.0 P 13/34 Dec 2016 Solomon Systech
Advance Graphic Command Table
D/C# Hex D7 D6 D5 D4 D3 D2 D1 D0 Command Description
0 23 0 0 1 0 0 0 1 1 Set Fade
Out and
Blinking
A[5:4] = 00b Disable Fade Out / Blinking Mode[RESET]
A[5:4] = 10b Enable Fade Out mode.
Once Fade Out mode is enabled, contrast decrease
gradually to all pixels OFF. Output follows RAM content
when Fade mode is disabled.
A[5:4] = 11b Enable Blinking mode.
Once Blinking mode is enabled, contrast decrease
gradually to all pixels OFF and then contrast increase
gradually to normal display. This process loop
continuously until the Blinking mode is disabled.
A[3:0] : Set time interval for each fade step
Note
(1) Refer to section 1.4.1 for details.
A[3:0] Time interval for each fade step
0000b 8 Frames
0001b 16 Frames
0010b 24 Frames
:
1111b 128 Frames
0 A[5:0] * * A5 A4 A3 A2 A1 A0
0 D6 1 1 0 1 0 1 1 0 Set Zoom In A[0] = 0b Disable Zoom in Mode [RESET]
A[0] = 1b Enable Zoom in Mode
Note
(1) The panel must be in alternative COM pin
configuration (command DAh A[4] =1)
(2) Refer to section 1.4.2 for details.
0 A[0] * * * * * * * A0
Note
(1) "*" stands for "Don't care".
```

## Page 46

```text
Solomon Systech Dec 2016 P 14/34 Rev 1.0 SSD1315
Table 1-2 : Read Command Table
Bit Pattern Command Description
D7D6D5D4D3D2D1D0 Status Register Read D[7] : Reserved
D[6] : "1" for display OFF / "0" for display ON
D[5] : Reserved
D[4] : Reserved
D[3] : Reserved
D[2] : Reserved
D[1] : Reserved
D[0] : Reserved
Note
(1) Patterns other than those given in the Command Table are prohibited to enter the chip as a command; as unexpected
results can occur.
1.1 Data Read / Write
To read data from the GDDRAM, select HIGH for both the R/W# (WR#) pin and the D/C# pin for 6800-
series parallel mode and select LOW for the E (RD#) pin and HIGH for the D/C# pin for 8080-series parallel
mode. No data read is provided in serial mode operation.
In normal data read mode the GDDRAM column address pointer will be increased automatically by one after
each data read.
Also, a dummy read is required before the first data read.
To write data to the GDDRAM, select LOW for the R/W# (WR#) pin and HIGH for the D/C# pin for both
6800-series parallel mode and 8080-series parallel mode. The serial interface mode is always in write mode.
The GDDRAM column address pointer will be increased automatically by one after each data write.
Table 1-3 : Address increment table (Automatic)
D/C# R/W# (WR#) Comment Address Increment
0 0 Write Command No
0 1 Read Status No
1 0 Write Data Yes
1 1 Read Data Yes
```

## Page 47

```text
SSD1315 Rev 1.0 P 15/34 Dec 2016 Solomon Systech
1.2 Fundamental Command
1.2.1 Set Lower Column Start Address for Page Addressing Mode (00h~0Fh)
This command specifies the lower nibble of the 8-bit column start address for the display data RAM under
Page Addressing Mode. The column address will be incremented by each data access. Please refer to Section
Table 1-1 and Section 1.2.3 for details.
1.2.2 Set Higher Column Start Address for Page Addressing Mode (10h~17h)
This command specifies the higher nibble of the 8-bit column start address for the display data RAM under
Page Addressing Mode. The column address will be incremented by each data access. Please refer to Section
Table 1-1 and Section 1.2.3 for details.
1.2.3 Set Memory Addressing Mode (20h)
There are 3 different memory addressing mode in SSD1315: page addressing mode, horizontal addressing
mode and vertical addressing mode. This command sets the way of memory addressing into one of the above
three modes. In there, "COL" means the graphic display data RAM column.
Page addressing mode (A[1:0]=10xb)
In page addressing mode, after the display RAM is read/written, the column address pointer is increased
automatically by 1. Users have to set the new page and column addresses in order to access the next page
RAM content. The sequence of movement of the PAGE and column address point for page addressing mode
is shown in Figure 1-1.
Figure 1-1 : Address Pointer Movement of Page addressing mode
COL0 COL 1 ..... COL 126 COL 127
PAGE0
PAGE1
: : : : : :
PAGE6
PAGE7
In normal display data RAM read or write and page addressing mode, the following steps are required to
define the starting RAM access pointer location:
- Set the page start address of the target display location by command B0h to B7h.
- Set the lower start column address of pointer by command 00h~0Fh.
- Set the upper start column address of pointer by command 10h~17h.
For example, if the page address is set to B2h, lower column address is 03h and upper column address is 10h,
then that means the starting column is SEG3 of PAGE2. The RAM access pointer is located as shown in
Figure 1-2. The input data byte will be written into RAM position of column 3.
Figure 1-2 : Example of GDDRAM access pointer setting in Page Addressing Mode (No row and column-
remapping)

LSB D0
MSB D7
Each lattice represents
one bit of image data
....................PAGE2
(Starting page)
COM16
COM17
:
:
:
:
:
COM23
SEG0 SEG3 (Starting column) SEG127
RAM access pointer
```

## Page 48

```text
Solomon Systech Dec 2016 P 16/34 Rev 1.0 SSD1315
Horizontal addressing mode (A[1:0]=00b)
In horizontal addressing mode, after the display RAM is read/written, the column address pointer is increased
automatically by 1. If the column address pointer reaches column end address, the column address pointer is
reset to column start address and page address pointer is increased by 1. The sequence of movement of the
page and column address point for horizontal addressing mode is shown in Figure 1-3. When both column
and page address pointers reach the end address, the pointers are reset to column start address and page start
address (Dotted line in Figure 1-3.)
Figure 1-3 : Address Pointer Movement of Horizontal addressing mode
COL0 COL 1 ..... COL 126 COL 127
PAGE0
PAGE1
: : : : : :
PAGE6
PAGE7
Vertical addressing mode: (A[1:0]=01b)
In vertical addressing mode, after the display RAM is read/written, the page address pointer is increased
automatically by 1. If the page address pointer reaches the page end address, the page address pointer is reset
to page start address and column address pointer is increased by 1 . The sequence of movement of the page
and column address point for vertical addressing mode is shown in Figure 1-4. When both column and page
address pointers reach the end address, the pointers are reset to column start address and page start address
(Dotted line in Figure 1-4.)
Figure 1-4 : Address Pointer Movement of Vertical addressing mode
COL0 COL 1 ..... COL 126 COL 127
PAGE0 .....
PAGE1 .....
: :
PAGE6 .....
PAGE7 .....
In normal display data RAM read or write and horizontal / vertical addressing mode, the following steps are
required to define the RAM access pointer location:
- Set the column start and end address of the target display location by command 21h.
- Set the page start and end address of the target display location by command 22h.
Example is shown in Figure 1-5.
1.2.4 Set Column Address (21h)
This triple byte command specifies column start address and end address of the display data RAM. This
command also sets the column address pointer to column start address. This pointer is used to define the
current read/write column address in graphic display data RAM. If horizontal address increment mode is
enabled by command 20h, after finishing read/write one column data, it is incremented automatically to the
next column address. Whenever the column address pointer finishes accessing the end column address, it is
reset back to start column address and the row address is incremented to the next row.
```

## Page 49

```text
SSD1315 Rev 1.0 P 17/34 Dec 2016 Solomon Systech
1.2.5 Set Page Address (22h)
This triple byte command specifies page start address and end address of the display data RAM. This
command also sets the page address pointer to page start address. This pointer is used to define the current
read/write page address in graphic display data RAM. If vertical address increment mode is enabled by
command 20h, after finishing read/write one page data, it is incremented automatically to the next page
address. Whenever the page address pointer finishes accessing the end page address, it is reset back to start
page address.
The figure below shows the way of column and page address pointer movement through the example: column
start address is set to 2 and column end address is set to 125, page start address is set to 1 and page end
address is set to 6; Horizontal address increment mode is enabled by command 20h. In this case, the graphic
display data RAM column accessible range is from column 2 to column 125 and from page 1 to page 6 only.
In addition, the column address pointer is set to 2 and page address pointer is set to 1. After finishing
read/write one pixel of data, the column address is increased automatically by 1 to access the next RAM
location for next read/write operation ( solid line in Figure 1-5). Whenever the column address pointer
finishes accessing the end column 125, it is reset back to column 2 and page address is automatically
increased by 1 ( solid line in Figure 1-5). While the end page 6 and end column 125 RAM location is
accessed, the page address is reset back to 1 and the column address is reset back to 2 ( dotted line in Figure
1-5). .
Figure 1-5 : Example of Column and Row Address Pointer Movement
Col 0 Col 1 Col 2 ..... ....... Col 125 Col 126 Col 127
PAGE0
PAGE1
: :
PAGE6
PAGE7 :
1.2.6 Set Display Start Line (40h~7Fh)
This command sets the Display Start Line register to determine starting address of display RAM, by selecting
a value from 0 to 63. With value equal to 0, RAM row 0 is mapped to COM0. With value equal to 1, RAM
row 1 is mapped to COM0 and so on.
Refer to Table 1-4 for more illustrations.
1.2.7 Set Contrast Control (81h)
This command sets the Contrast Setting of the display with a valid range from 01h to FFh. The segment
output current increases as the contrast step value increases.
1.2.8 Set Segment Re-map (A0h/A1h)
This command changes the mapping between the display data column address and the segment driver. It
allows flexibility in OLED module design. Please refer to Table 1-1.
This command only affects subsequent data input. Data already stored in GDDRAM will have no changes.
1.2.9 Entire Display ON (A4h/A5h)
A4h command enable display outputs according to the GDDRAM contents.
If A5h command is issued, then by using A4h command, the display will resume to the GDDRAM contents.
In other words, A4h command resumes the display from entire display "ON" stage.
A5h command forces the entire display to be "ON", regardless of the contents of the display data RAM.
```

## Page 50

```text
Solomon Systech Dec 2016 P 18/34 Rev 1.0 SSD1315
1.2.10 Set Normal/Inverse Display (A6h/A7h)
This command sets the display to be either normal or inverse. In normal display a RAM data of 1 indicates an
"ON" pixel while in inverse display a RAM data of 0 indicates an "ON" pixel.
1.2.11 Set Multiplex Ratio (A8h)
This command switches the default 63 multiplex mode to any multiplex ratio, ranging from 16 to 63. The
output pads COM0~COM63 will be switched to the corresponding COM signal.
1.2.12 External or internal IREF Selection (ADh)
This command selects the external IREF or internal IREF and to define the value of internal IREF setting. Refer to
SEG/COM Driving block for details.
1.2.13 Set Display ON/OFF (AEh/AFh)
These single byte commands are used to turn the OLED panel display ON or OFF.
When the display is ON, the selected circuits by Set Master Configuration command will be turned ON.
When the display is OFF, those circuits will be turned OFF and the segment and common output are in V SS
state and high impedance state, respectively. These commands set the display to one of the two states:
o AEh : Display OFF
o AFh : Display ON
Figure 1-6 :Transition between different modes
1.2.14 Set Page Start Address for Page Addressing Mode (B0h~B7h)
This command positions the page start address from 0 to 7 in GDDRAM under Page Addressing Mode.
Please refer to Table 1-1 and Section 1.2.3 for details.
1.2.15 Set COM Output Scan Direction (C0h/C8h)
This command sets the scan direction of the COM output, allowing layout flexibility in the OLED module
design. Additionally, the display will show once this command is issued. For example, if this command is
sent during normal display then the graphic display will be vertically flipped immediately. Please refer to
Table 1-6 for details.
1.2.16 Set Display Offset (D3h)
This is a double byte command. The second command specifies the mapping of the display start line to one of
COM0~COM63 (assuming that COM0 is the display start line then the display start line register is equal to 0).
For example, to move the COM16 towards the COM0 direction by 16 lines the 6-bit data in the second byte
should be given as 010000b. To move in the opposite direction by 16 lines the 6-bit data should be given by
64 - 16, so the second byte would be 110000b. The following two tables ( Table 1-4, Table 1-5) show the
example of setting the command C0h/C8h and D3h.
Normal mode Sleep mode
AFh
AEh
```

## Page 51

```text
SSD1315 Rev 1.0 P 19/34 Dec 2016 Solomon Systech
Table 1-4 : Example of Set Display Offset and Display Start Line with no Remap
Set MUX ratio(A8h)
COM Normal / Remapped (C0h / C8h)
Display offset (D3h)
Display start line (40h - 7Fh)
COM0 Row0 RAM0 Row8 RAM8 Row0 RAM8 Row0 RAM0 Row8 RAM8 Row0 RAM8
COM1 Row1 RAM1 Row9 RAM9 Row1 RAM9 Row1 RAM1 Row9 RAM9 Row1 RAM9
COM2 Row2 RAM2 Row10 RAM10 Row2 RAM10 Row2 RAM2 Row10 RAM10 Row2 RAM10
COM3 Row3 RAM3 Row11 RAM11 Row3 RAM11 Row3 RAM3 Row11 RAM11 Row3 RAM11
COM4 Row4 RAM4 Row12 RAM12 Row4 RAM12 Row4 RAM4 Row12 RAM12 Row4 RAM12
COM5 Row5 RAM5 Row13 RAM13 Row5 RAM13 Row5 RAM5 Row13 RAM13 Row5 RAM13
COM6 Row6 RAM6 Row14 RAM14 Row6 RAM14 Row6 RAM6 Row14 RAM14 Row6 RAM14
COM7 Row7 RAM7 Row15 RAM15 Row7 RAM15 Row7 RAM7 Row15 RAM15 Row7 RAM15
COM8 Row8 RAM8 Row16 RAM16 Row8 RAM16 Row8 RAM8 Row16 RAM16 Row8 RAM16
COM9 Row9 RAM9 Row17 RAM17 Row9 RAM17 Row9 RAM9 Row17 RAM17 Row9 RAM17
COM10 Row10 RAM10 Row18 RAM18 Row10 RAM18 Row10 RAM10 Row18 RAM18 Row10 RAM18
COM11 Row11 RAM11 Row19 RAM19 Row11 RAM19 Row11 RAM11 Row19 RAM19 Row11 RAM19
COM12 Row12 RAM12 Row20 RAM20 Row12 RAM20 Row12 RAM12 Row20 RAM20 Row12 RAM20
COM13 Row13 RAM13 Row21 RAM21 Row13 RAM21 Row13 RAM13 Row21 RAM21 Row13 RAM21
COM14 Row14 RAM14 Row22 RAM22 Row14 RAM22 Row14 RAM14 Row22 RAM22 Row14 RAM22
COM15 Row15 RAM15 Row23 RAM23 Row15 RAM23 Row15 RAM15 Row23 RAM23 Row15 RAM23
COM16 Row16 RAM16 Row24 RAM24 Row16 RAM24 Row16 RAM16 Row24 RAM24 Row16 RAM24
COM17 Row17 RAM17 Row25 RAM25 Row17 RAM25 Row17 RAM17 Row25 RAM25 Row17 RAM25
COM18 Row18 RAM18 Row26 RAM26 Row18 RAM26 Row18 RAM18 Row26 RAM26 Row18 RAM26
COM19 Row19 RAM19 Row27 RAM27 Row19 RAM27 Row19 RAM19 Row27 RAM27 Row19 RAM27
COM20 Row20 RAM20 Row28 RAM28 Row20 RAM28 Row20 RAM20 Row28 RAM28 Row20 RAM28
COM21 Row21 RAM21 Row29 RAM29 Row21 RAM29 Row21 RAM21 Row29 RAM29 Row21 RAM29
COM22 Row22 RAM22 Row30 RAM30 Row22 RAM30 Row22 RAM22 Row30 RAM30 Row22 RAM30
COM23 Row23 RAM23 Row31 RAM31 Row23 RAM31 Row23 RAM23 Row31 RAM31 Row23 RAM31
COM24 Row24 RAM24 Row32 RAM32 Row24 RAM32 Row24 RAM24 Row32 RAM32 Row24 RAM32
COM25 Row25 RAM25 Row33 RAM33 Row25 RAM33 Row25 RAM25 Row33 RAM33 Row25 RAM33
COM26 Row26 RAM26 Row34 RAM34 Row26 RAM34 Row26 RAM26 Row34 RAM34 Row26 RAM34
COM27 Row27 RAM27 Row35 RAM35 Row27 RAM35 Row27 RAM27 Row35 RAM35 Row27 RAM35
COM28 Row28 RAM28 Row36 RAM36 Row28 RAM36 Row28 RAM28 Row36 RAM36 Row28 RAM36
COM29 Row29 RAM29 Row37 RAM37 Row29 RAM37 Row29 RAM29 Row37 RAM37 Row29 RAM37
COM30 Row30 RAM30 Row38 RAM38 Row30 RAM38 Row30 RAM30 Row38 RAM38 Row30 RAM38
COM31 Row31 RAM31 Row39 RAM39 Row31 RAM39 Row31 RAM31 Row39 RAM39 Row31 RAM39
COM32 Row32 RAM32 Row40 RAM40 Row32 RAM40 Row32 RAM32 Row40 RAM40 Row32 RAM40
COM33 Row33 RAM33 Row41 RAM41 Row33 RAM41 Row33 RAM33 Row41 RAM41 Row33 RAM41
COM34 Row34 RAM34 Row42 RAM42 Row34 RAM42 Row34 RAM34 Row42 RAM42 Row34 RAM42
COM35 Row35 RAM35 Row43 RAM43 Row35 RAM43 Row35 RAM35 Row43 RAM43 Row35 RAM43
COM36 Row36 RAM36 Row44 RAM44 Row36 RAM44 Row36 RAM36 Row44 RAM44 Row36 RAM44
COM37 Row37 RAM37 Row45 RAM45 Row37 RAM45 Row37 RAM37 Row45 RAM45 Row37 RAM45
COM38 Row38 RAM38 Row46 RAM46 Row38 RAM46 Row38 RAM38 Row46 RAM46 Row38 RAM46
COM39 Row39 RAM39 Row47 RAM47 Row39 RAM47 Row39 RAM39 Row47 RAM47 Row39 RAM47
COM40 Row40 RAM40 Row48 RAM48 Row40 RAM48 Row40 RAM40 Row48 RAM48 Row40 RAM48
COM41 Row41 RAM41 Row49 RAM49 Row41 RAM49 Row41 RAM41 Row49 RAM49 Row41 RAM49
COM42 Row42 RAM42 Row50 RAM50 Row42 RAM50 Row42 RAM42 Row50 RAM50 Row42 RAM50
COM43 Row43 RAM43 Row51 RAM51 Row43 RAM51 Row43 RAM43 Row51 RAM51 Row43 RAM51
COM44 Row44 RAM44 Row52 RAM52 Row44 RAM52 Row44 RAM44 Row52 RAM52 Row44 RAM52
COM45 Row45 RAM45 Row53 RAM53 Row45 RAM53 Row45 RAM45 Row53 RAM53 Row45 RAM53
COM46 Row46 RAM46 Row54 RAM54 Row46 RAM54 Row46 RAM46 Row54 RAM54 Row46 RAM54
COM47 Row47 RAM47 Row55 RAM55 Row47 RAM55 Row47 RAM47 Row55 RAM55 Row47 RAM55
COM48 Row48 RAM48 Row56 RAM56 Row48 RAM56 Row48 RAM48 - - Row48 RAM56
COM49 Row49 RAM49 Row57 RAM57 Row49 RAM57 Row49 RAM49 - - Row49 RAM57
COM50 Row50 RAM50 Row58 RAM58 Row50 RAM58 Row50 RAM50 - - Row50 RAM58
COM51 Row51 RAM51 Row59 RAM59 Row51 RAM59 Row51 RAM51 - - Row51 RAM59
COM52 Row52 RAM52 Row60 RAM60 Row52 RAM60 Row52 RAM52 - - Row52 RAM60
COM53 Row53 RAM53 Row61 RAM61 Row53 RAM61 Row53 RAM53 - - Row53 RAM61
COM54 Row54 RAM54 Row62 RAM62 Row54 RAM62 Row54 RAM54 - - Row54 RAM62
COM55 Row55 RAM55 Row63 RAM63 Row55 RAM63 Row55 RAM55 - - Row55 RAM63
COM56 Row56 RAM56 Row0 RAM0 Row56 RAM0 - - Row0 RAM0 - -
COM57 Row57 RAM57 Row1 RAM1 Row57 RAM1 - - Row1 RAM1 - -
COM58 Row58 RAM58 Row2 RAM2 Row58 RAM2 - - Row2 RAM2 - -
COM59 Row59 RAM59 Row3 RAM3 Row59 RAM3 - - Row3 RAM3 - -
COM60 Row60 RAM60 Row4 RAM4 Row60 RAM4 - - Row4 RAM4 - -
COM61 Row61 RAM61 Row5 RAM5 Row61 RAM5 - - Row5 RAM5 - -
COM62 Row62 RAM62 Row6 RAM6 Row62 RAM6 - - Row6 RAM6 - -
COM63 Row63 RAM63 Row7 RAM7 Row63 RAM7 - - Row7 RAM7 - -
Display
examples (e) (f)(a) (b) (c) (d)
0 8 0
0 0 8
56 56 56
Normal Normal Normal
64
Normal
0
8
Output
Hardware
pin name 0
0
Normal
64 64
Normal
8
0
(a) (b) (d) (c)
(e) (f)
(RAM)
```

## Page 52

```text
Solomon Systech Dec 2016 P 20/34 Rev 1.0 SSD1315
Table 1-5 : Example of Set Display Offset and Display Start Line with Remap

(a) (b) (d) (c)
(e) (f) (g)
(RAM)
Set MUX ratio(A8h)
COM Normal / Remapped (C0h / C8h)
Display offset (D3h)
Display start line (40h - 7Fh)
COM0 Row63 RAM63 Row7 RAM7 Row63 RAM7 Row47 RAM47 - - Row47 RAM55 - -
COM1 Row62 RAM62 Row6 RAM6 Row62 RAM6 Row46 RAM46 - - Row46 RAM54 - -
COM2 Row61 RAM61 Row5 RAM5 Row61 RAM5 Row45 RAM45 - - Row45 RAM53 - -
COM3 Row60 RAM60 Row4 RAM4 Row60 RAM4 Row44 RAM44 - - Row44 RAM52 - -
COM4 Row59 RAM59 Row3 RAM3 Row59 RAM3 Row43 RAM43 - - Row43 RAM51 - -
COM5 Row58 RAM58 Row2 RAM2 Row58 RAM2 Row42 RAM42 - - Row42 RAM50 - -
COM6 Row57 RAM57 Row1 RAM1 Row57 RAM1 Row41 RAM41 - - Row41 RAM49 - -
COM7 Row56 RAM56 Row0 RAM0 Row56 RAM0 Row40 RAM40 - - Row40 RAM48 - -
COM8 Row55 RAM55 Row63 RAM63 Row55 RAM63 Row39 RAM39 Row47 RAM47 Row39 RAM47 Row47 RAM63
COM9 Row54 RAM54 Row62 RAM62 Row54 RAM62 Row38 RAM38 Row46 RAM46 Row38 RAM46 Row46 RAM62
COM10 Row53 RAM53 Row61 RAM61 Row53 RAM61 Row37 RAM37 Row45 RAM45 Row37 RAM45 Row45 RAM61
COM11 Row52 RAM52 Row60 RAM60 Row52 RAM60 Row36 RAM36 Row44 RAM44 Row36 RAM44 Row44 RAM60
COM12 Row51 RAM51 Row59 RAM59 Row51 RAM59 Row35 RAM35 Row43 RAM43 Row35 RAM43 Row43 RAM59
COM13 Row50 RAM50 Row58 RAM58 Row50 RAM58 Row34 RAM34 Row42 RAM42 Row34 RAM42 Row42 RAM58
COM14 Row49 RAM49 Row57 RAM57 Row49 RAM57 Row33 RAM33 Row41 RAM41 Row33 RAM41 Row41 RAM57
COM15 Row48 RAM48 Row56 RAM56 Row48 RAM56 Row32 RAM32 Row40 RAM40 Row32 RAM40 Row40 RAM56
COM16 Row47 RAM47 Row55 RAM55 Row47 RAM55 Row31 RAM31 Row39 RAM39 Row31 RAM39 Row39 RAM55
COM17 Row46 RAM46 Row54 RAM54 Row46 RAM54 Row30 RAM30 Row38 RAM38 Row30 RAM38 Row38 RAM54
COM18 Row45 RAM45 Row53 RAM53 Row45 RAM53 Row29 RAM29 Row37 RAM37 Row29 RAM37 Row37 RAM53
COM19 Row44 RAM44 Row52 RAM52 Row44 RAM52 Row28 RAM28 Row36 RAM36 Row28 RAM36 Row36 RAM52
COM20 Row43 RAM43 Row51 RAM51 Row43 RAM51 Row27 RAM27 Row35 RAM35 Row27 RAM35 Row35 RAM51
COM21 Row42 RAM42 Row50 RAM50 Row42 RAM50 Row26 RAM26 Row34 RAM34 Row26 RAM34 Row34 RAM50
COM22 Row41 RAM41 Row49 RAM49 Row41 RAM49 Row25 RAM25 Row33 RAM33 Row25 RAM33 Row33 RAM49
COM23 Row40 RAM40 Row48 RAM48 Row40 RAM48 Row24 RAM24 Row32 RAM32 Row24 RAM32 Row32 RAM48
COM24 Row39 RAM39 Row47 RAM47 Row39 RAM47 Row23 RAM23 Row31 RAM31 Row23 RAM31 Row31 RAM47
COM25 Row38 RAM38 Row46 RAM46 Row38 RAM46 Row22 RAM22 Row30 RAM30 Row22 RAM30 Row30 RAM46
COM26 Row37 RAM37 Row45 RAM45 Row37 RAM45 Row21 RAM21 Row29 RAM29 Row21 RAM29 Row29 RAM45
COM27 Row36 RAM36 Row44 RAM44 Row36 RAM44 Row20 RAM20 Row28 RAM28 Row20 RAM28 Row28 RAM44
COM28 Row35 RAM35 Row43 RAM43 Row35 RAM43 Row19 RAM19 Row27 RAM27 Row19 RAM27 Row27 RAM43
COM29 Row34 RAM34 Row42 RAM42 Row34 RAM42 Row18 RAM18 Row26 RAM26 Row18 RAM26 Row26 RAM42
COM30 Row33 RAM33 Row41 RAM41 Row33 RAM41 Row17 RAM17 Row25 RAM25 Row17 RAM25 Row25 RAM41
COM31 Row32 RAM32 Row40 RAM40 Row32 RAM40 Row16 RAM16 Row24 RAM24 Row16 RAM24 Row24 RAM40
COM32 Row31 RAM31 Row39 RAM39 Row31 RAM39 Row15 RAM15 Row23 RAM23 Row15 RAM23 Row23 RAM39
COM33 Row30 RAM30 Row38 RAM38 Row30 RAM38 Row14 RAM14 Row22 RAM22 Row14 RAM22 Row22 RAM38
COM34 Row29 RAM29 Row37 RAM37 Row29 RAM37 Row13 RAM13 Row21 RAM21 Row13 RAM21 Row21 RAM37
COM35 Row28 RAM28 Row36 RAM36 Row28 RAM36 Row12 RAM12 Row20 RAM20 Row12 RAM20 Row20 RAM36
COM36 Row27 RAM27 Row35 RAM35 Row27 RAM35 Row11 RAM11 Row19 RAM19 Row11 RAM19 Row19 RAM35
COM37 Row26 RAM26 Row34 RAM34 Row26 RAM34 Row10 RAM10 Row18 RAM18 Row10 RAM18 Row18 RAM34
COM38 Row25 RAM25 Row33 RAM33 Row25 RAM33 Row9 RAM9 Row17 RAM17 Row9 RAM17 Row17 RAM33
COM39 Row24 RAM24 Row32 RAM32 Row24 RAM32 Row8 RAM8 Row16 RAM16 Row8 RAM16 Row16 RAM32
COM40 Row23 RAM23 Row31 RAM31 Row23 RAM31 Row7 RAM7 Row15 RAM15 Row7 RAM15 Row15 RAM31
COM41 Row22 RAM22 Row30 RAM30 Row22 RAM30 Row6 RAM6 Row14 RAM14 Row6 RAM14 Row14 RAM30
COM42 Row21 RAM21 Row29 RAM29 Row21 RAM29 Row5 RAM5 Row13 RAM13 Row5 RAM13 Row13 RAM29
COM43 Row20 RAM20 Row28 RAM28 Row20 RAM28 Row4 RAM4 Row12 RAM12 Row4 RAM12 Row12 RAM28
COM44 Row19 RAM19 Row27 RAM27 Row19 RAM27 Row3 RAM3 Row11 RAM11 Row3 RAM11 Row11 RAM27
COM45 Row18 RAM18 Row26 RAM26 Row18 RAM26 Row2 RAM2 Row10 RAM10 Row2 RAM10 Row10 RAM26
COM46 Row17 RAM17 Row25 RAM25 Row17 RAM25 Row1 RAM1 Row9 RAM9 Row1 RAM9 Row9 RAM25
COM47 Row16 RAM16 Row24 RAM24 Row16 RAM24 Row0 RAM0 Row8 RAM8 Row0 RAM8 Row8 RAM24
COM48 Row15 RAM15 Row23 RAM23 Row15 RAM23 - - Row7 RAM7 - - Row7 RAM23
COM49 Row14 RAM14 Row22 RAM22 Row14 RAM22 - - Row6 RAM6 - - Row6 RAM22
COM50 Row13 RAM13 Row21 RAM21 Row13 RAM21 - - Row5 RAM5 - - Row5 RAM21
COM51 Row12 RAM12 Row20 RAM20 Row12 RAM20 - - Row4 RAM4 - - Row4 RAM20
COM52 Row11 RAM11 Row19 RAM19 Row11 RAM19 - - Row3 RAM3 - - Row3 RAM19
COM53 Row10 RAM10 Row18 RAM18 Row10 RAM18 - - Row2 RAM2 - - Row2 RAM18
COM54 Row9 RAM9 Row17 RAM17 Row9 RAM17 - - Row1 RAM1 - - Row1 RAM17
COM55 Row8 RAM8 Row16 RAM16 Row8 RAM16 - - Row0 RAM0 - - Row0 RAM16
COM56 Row7 RAM7 Row15 RAM15 Row7 RAM15 - - - - - - - -
COM57 Row6 RAM6 Row14 RAM14 Row6 RAM14 - - - - - - - -
COM58 Row5 RAM5 Row13 RAM13 Row5 RAM13 - - - - - - - -
COM59 Row4 RAM4 Row12 RAM12 Row4 RAM12 - - - - - - - -
COM60 Row3 RAM3 Row11 RAM11 Row3 RAM11 - - - - - - - -
COM61 Row2 RAM2 Row10 RAM10 Row2 RAM10 - - - - - - - -
COM62 Row1 RAM1 Row9 RAM9 Row1 RAM9 - - - - - - - -
COM63 Row0 RAM0 Row8 RAM8 Row0 RAM8 - - - - - - - -
Display
examples
48
Remap
8
16
8 0
0 8
48 48
Remap Remap
48
Remap
0
0
0
0 0 8
Hardware
pin name
Output
64 64 64
Remap Remap Remap
0 8
(e) (f) (g)(a) (b) (c) (d)
```

## Page 53

```text
SSD1315 Rev 1.0 P 21/34 Dec 2016 Solomon Systech
1.2.17 Set Display Clock Divide Ratio/ Oscillator Frequency (D5h)
This command consists of two functions:
- Display Clock Divide Ratio (D) (A[3:0])
Set the divide ratio to generate DCLK (Display Clock) from CLK. The divide ratio is from 1 to 16,
with reset value = 1. Please refer to Oscillator Circuit and Display Time Generator for the details
relationship of DCLK and CLK.
- Oscillator Frequency (A[7:4])
Program the oscillator frequency Fosc that is the source of CLK if CLS pin is pulled high. The 4-bit
value results in 16 different frequency settings. The default setting is 1000b.
1.2.18 Set Pre-charge Period (D9h)
This command is used to set the duration of the pre-charge period. The interval is counted in number of
DCLK, where RESET equals 4 DCLKs.
1.2.19 Set COM Pins Hardware Configuration (DAh)
This command sets the COM signals pin configuration to match the OLED panel hardware layout. The table
below shows the COM pin configuration under different conditions (for MUX ratio =64):
Table 1-6 : COM Pins Hardware Configuration
Conditions COM pins Configurations
1 Sequential COM pin configuration (DAh A[4] =0)
COM output Scan direction: from COM0 to COM63 (C0h)
Disable COM Left/Right remap (DAh A[5] =0)
128x 64
ROW0
ROW31 ROW32
ROW63
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM31
COM32
COM63
```

## Page 54

```text
Solomon Systech Dec 2016 P 22/34 Rev 1.0 SSD1315
Conditions COM pins Configurations
2 Sequential COM pin configuration (DAh A[4] =0)
COM output Scan direction: from COM0 to COM63 (C0h)
Enable COM Left/Right remap (DAh A[5] =1)
3 Sequential COM pin configuration (DAh A[4] =0)
COM output Scan direction: from COM63 to COM0 (C8h)
Disable COM Left/Right remap (DAh A[5] =0)
4 Sequential COM pin configuration (DAh A[4] =0)
COM output Scan direction: from COM63 to COM0 (C8h)
Enable COM Left/Right remap (DAh A[5] =1)
128 x 64
ROW31
ROW0
ROW63
ROW32
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM31 COM63
COM32
128 x 64
ROW63
ROW32 ROW31
ROW0
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM31
COM32
COM63
128 x 64
ROW32
ROW63
ROW0
ROW31
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM63
COM32
```

## Page 55

```text
SSD1315 Rev 1.0 P 23/34 Dec 2016 Solomon Systech
Conditions COM pins Configurations
5 Alternative COM pin configuration (DAh A[4] =1)
COM output Scan direction: from COM0 to COM63 (C0h)
Disable COM Left/Right remap (DAh A[5] =0)
6 Alternative COM pin configuration (DAh A[4] =1)
COM output Scan direction: from COM0 to COM63 (C0h)
Enable COM Left/Right remap (DAh A[5] =1)
7 Alternative COM pin configuration (DAh A[4] =1)
COM output Scan direction: from COM63 to COM0(C8h)
Disable COM Left/Right remap (DAh A[5] =0)
128 x 64
ROW0
ROW62
ROW1
ROW63
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM31
COM32
COM63
ROW61
COM62
ROW2
COM1
128 x 64
ROW63
ROW1
ROW62
ROW0
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0
COM31
COM32
COM63
ROW2
COM62
ROW61
COM1
128 x 64
ROW2
ROW63
ROW0
ROW62
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0 COM32
COM63
ROW61
COM33
ROW1
COM31
COM30
```

## Page 56

```text
Solomon Systech Dec 2016 P 24/34 Rev 1.0 SSD1315
Conditions COM pins Configurations
8 Alternative COM pin configuration (DAh A[4] =1)
COM output Scan direction: from COM63 to COM0(C8h)
Enable COM Left/Right remap (DAh A[5] =1)
1.2.20 Set VCOMH Deselect Level (DBh)
This command adjusts the VCOMH regulator output. Refer to Table 1-1 for detail setting.
1.2.21 NOP (E3h)
No Operation Command.
1.2.22 Status register Read
This command is issued by setting D/C# ON LOW during a data read (See AC timing section for parallel
interface waveform). It allows the MCU to monitor the internal status of the chip. No status read is provided
for serial mode.
1.2.23 Charge Pump Setting (8Dh)
This command controls the ON/OFF of the Charge Pump. The Charge Pump must be enabled by the
following command sequence:
8Dh; Charge Pump Setting
14h / 94h / 95h; Enable Charge Pump at different output mode
AFh; Display ON
128 x 64
ROW61
ROW0
ROW63
ROW1
Pad 1,2,3,...->95
Gold Bumps face up
SSD1315
COM0 COM32
COM63
ROW2
COM33
ROW62
COM31
COM30
```

## Page 57

```text
SSD1315 Rev 1.0 P 25/34 Dec 2016 Solomon Systech
1.3 Graphic Acceleration Command
1.3.1 Horizontal Scroll Setup (26h/27h)
This command consists of 7 consecutive bytes to set up the horizontal scroll parameters and determines the
scrolling start page, end page and scrolling speed.
Before issuing this command the horizontal scroll must be deactivated (2Eh). Otherwise, RAM content may
be corrupted.
The SSD1315 horizontal scroll is designed for 128 columns scrolling. The following two figures ( Figure 1-7,
Figure 1-8, Figure 1-9) show the examples of using the horizontal scroll:
Figure 1-7 : Horizontal scroll example: Scroll RIGHT by 1 column
Original Setting
SEG0
SEG1
SEG2
SEG3
SEG4
SEG5
...
...
...
SEG122
SEG123
SEG124
SEG125
SEG126
SEG127
After one scroll
step
SEG127
SEG0
SEG1
SEG2
SEG3
SEG4
...
...
...
SEG121
SEG122
SEG123
SEG124
SEG125
SEG126
Figure 1-8 : Horizontal scroll example: Scroll LEFT by 1 column
Original
Setting
SEG0
SEG1
SEG2
SEG3
SEG4
SEG5
...
...
...
SEG122
SEG123
SEG124
SEG125
SEG126
SEG127
After one
scroll step
SEG1
SEG2
SEG3
SEG4
SEG5
SEG6
...
...
...
SEG123
SEG124
SEG125
SEG126
SEG127
SEG0
Figure 1-9 : Horizontal scrolling setup example
```

## Page 58

```text
Solomon Systech Dec 2016 P 26/34 Rev 1.0 SSD1315
1.3.2 Continuous Vertical and Horizontal Scroll Setup (29h/2Ah)
This command consists of 8 consecutive bytes to set up the continuous vertical scroll parameters and
determine the scrolling start page, end page, start column, end column, scrolling speed, horizontal and vertical
scrolling offset.
If the vertical scrolling offset byte E[3:0] of command 29h / 2Ah is set to zero, then only horizontal scrolling
is performed (like command 26/27h). On the other hand, if the number of column scroll offset byte A[0] is set
to zero, then only vertical scrolling is performed. Continuous diagonal (horizontal + vertical) scrolling would
be enabled if both A[0] and E[3:0] are set to be non-zero, whereas full column diagonal scrolling mode is
suggested by setting F[6:0]=00h and G[6:0]=7Fh.
Before issuing this command the scroll must be deactivated (2Eh), or otherwise, RAM content may be
corrupted. The following figure ( Figure 1-10 ) show the examples of using the continuous vertical and
horizontal scroll.
Figure 1-10 : Continuous Vertical and Horizontal scrolling setup example
1.3.3 Deactivate Scroll (2Eh)
This command stops the motion of scrolling. After sending 2Eh command to deactivate the scrolling action,
the ram data needs to be rewritten.
1.3.4 Activate Scroll (2Fh)
This command starts the motion of scrolling and should only be issued after the scroll setup parameters have
been defined by the scrolling setup commands : 26h/27h/29h/2Ah . The setting in the last scrolling setup
command overwrites the setting in the previous scrolling setup commands.
The following actions are prohibited after the scrolling is activated
1. RAM access (Data write or read)
2. Changing the horizontal scroll setup parameters
1.3.5 Set Vertical Scroll Area (A3h)
This command consists of 3 consecutive bytes to set up the vertical scroll area. For the continuous vertical
scroll function (command 29/2Ah), the number of rows that in vertical scrolling can be set smaller or equal to
the MUX ratio.
1.3.6 Content Scroll Setup (2Ch/2Dh)
This command consists of 7 consecutive bytes to set up the horizontal scroll parameters and determine the
scrolling start page, end page, start column and end column. One column will be scrolled horizontally by
sending the setting of command 2Ch / 2Dh once.
```

## Page 59

```text
SSD1315 Rev 1.0 P 27/34 Dec 2016 Solomon Systech
When command 2Ch / 2Dh are sent consecutively, a delay time of 2 / Frame Frequency must be set. Figure
1-11 shown an example of using 2Dh "Content Scroll Setup" command for horizontal scrolling to left wi th
infinite content update. In there, "Col" means the graphic display data RAM column.
Figure 1-11: Content Scrolling example (2Dh, Left Horizontal Scroll by one column)
By using command 2Ch/2Dh, RAM contents are scrolled and updated by one column. Table 1-7 is an
example of content scrolling setting of SSD1315 (scrolling window of 4 pages). The values of registers
depend on different conditions and applications.
Table 1-7 : Content Scrolling software flow example (Page addressing mode - command 20h, 02h)
Step Action D/C# Code Remarks
1 For i= 1 to n - - Create "For loop" for infinite content scrolling
2 Set Content scrolling command
(scrolling window : Page 2 to 5, Col
8 to Col 120)
0 2Dh Left Horizontal Scroll by one column
0 00h A[7:0] : Dummy byte (Set as 00h)
0 02h B[2:0] : Define start page address
0 01h C[7:0] : Dummy byte (Set as 01h)
0 05h D[2:0] : Define end page address
0 08h E[6:0] : Define start column address
0 78h F[6:0] : Define end column address
3 Add Delay time of
FrameFreq2 - - E.g. Delay 20ms if frame freq  100Hz
4 Write RAM on the beginning column
of the scrolling window
Write RAM on (Page2, Col 120)
(Content update in beginning
column)
0 B2h Set Page Start Address for Page Addressing Mode
0 17h Set Higher Column Start Address for Page Addressing Mode
0 08h Set Lower Column Start Address for Page Addressing Mode
1 - Write data to fill the RAM
Write RAM on (Page3, Col 120)
(Content update in beginning
column)
0 B3h Set Page Start Address for Page Addressing Mode
0 17h Set Higher Column Start Address for Page Addressing Mode
0 08h Set Lower Column Start Address for Page Addressing Mode
1 - Write data to fill the RAM
Write RAM on (Page4, Col 120)
(Content update in beginning
column)
0 B4h Set Page Start Address for Page Addressing Mode
0 17h Set Higher Column Start Address for Page Addressing Mode
0 08h Set Lower Column Start Address for Page Addressing Mode
1 - Write data to fill the RAM
Write RAM on (Page5, Col 120)
(Content update in beginning
column)
0 B5h Set Page Start Address for Page Addressing Mode
0 17h Set Higher Column Start Address for Page Addressing Mode
0 08h Set Lower Column Start Address for Page Addressing Mode
1 - Write data to fill the RAM
5 i=i+1 - - Go to next "For loop"
Delay timing - - Set time interval between each scroll step if necessary
End
Solo
Hav
Col 8 Col 120
Page 5
Page 2
Solomon Sys
Have a nice
Col 8 Col 120
Page 2
Page 5
Solomon Systech
Have a nice da
Col 8 Col 120
Page 2
Page 5
End
column
Beginning
column
Scrolling
window
Left horizontal scroll
```

## Page 60

```text
Solomon Systech Dec 2016 P 28/34 Rev 1.0 SSD1315
There are 3 different memory addressing mode in SSD1315: page addressing mode, horizontal addressing
mode and vertical addressing mode and it is selected by command 20h. Table 1-7 is an example of content
scrolling software flow under page addressing mode, while vertical addressing mode example is shown in
below Table 1-8.
Table 1-8 : Content Scrolling setting example (Vertical addressing mode - command 20h, 01h)
Step Action D/C# Code Remarks
1 For i= 1 to n - - Create "For loop" for infinite content scrolling
2 Set Content scrolling command
(scrolling window : Page 2 to 5, Col
8 to Col 120)
0 2Dh Left Horizontal Scroll by one column
0 00h A[6:0] : Dummy byte (Set as 00h)
0 02h B[2:0] : Define start page address
0 01h C[2:0] : Dummy byte (Set as 01h)
0 05h D[2:0] : Define end page address
0 08h E[6:0] : Define start column address
0 78h F[6:0] : Define end column address
3 Add Delay time of
FrameFreq2 - - E.g. Delay 20ms if frame freq  100Hz
4 Write RAM on the beginning column
of the scrolling window (Page 2 to 5,
Col 120)
(Content update in beginning
column)
0 21h Set Column address
0 78h Set column start address for Vertical Addressing Mode
0 78h Set column end address for Vertical Addressing Mode
0 22h Set Page address
0 02h Set start page address for Vertical Addressing Mode
0 05h Set end page address for Vertical Addressing Mode
1 - Write data to fill the RAM
5 i=i+1 - - Go to next "For loop"
Delay timing - - Set time interval between each scroll step if necessary
End
```

## Page 61

```text
SSD1315 Rev 1.0 P 29/34 Dec 2016 Solomon Systech
1.4 Advance Graphic Command
1.4.1 Set Fade Out and Blinking (23h)
This command allows to set the fade mode and to adjust the time interval for each fade step. Below figures
show the example of Fade Out mode and Blinking mode.
Figure 1-12 : Example of Fade Out mode
Figure 1-13 : Example of Blinking mode
1.4.2 Set Zoom In (D6h)
Under Zoom in mode, one row of display contents is expanded into two rows on the display. That is, contents
of row0~31 fill the whole display panel of 64 rows. It should be notice that the panel must be in alternative
COM pin configuration (command DAh A[4] =1) for zoom in function.
Figure 1-14 : Example of Zoom In
```

## Page 62

```text
Solomon Systech Dec 2016 P 30/34 Rev 1.0 SSD1315
Appendix V: DC Characteristics for Internal Charge Pump Regulator
Condition (Unless otherwise specified):
Voltage referenced to VSS
VDD = 1.65V to 3.5V
TA = 25C
Table 0-1 : DC Characteristics
Symbol Parameter Test Condition Min Typ Max Unit
VBAT Charge Pump Regulator
Supply Voltage - 3.0 - 4.5 V
Charge
Pump
VCC
Charge Pump Output
Voltage
ITO resistance <3ohm for
charge pump related pins (1)
VBAT = 3.0V~4.5V, 7.5V mode
Maximum output loading = 8mA 7 7.5 - V
VBAT = 3.6V~4.5V, 8.5V mode
Maximum output loading = 12mA 8 8.5 - V
VBAT = 3.8V~4.5V, 9V mode
Maximum output loading = 12mA 8.5 9 - V
Remarks: (1) Charge pump related pins include: VBAT, C1P, C1N, C2P, C2N, VLSS
```
