# Morse Communicator

A two-station embedded messaging prototype built with Arduino Uno boards. Each station lets a user enter Morse code with physical buttons, decodes it into text locally, sends the text over an infrared link, and displays recent messages on a 16 × 2 LCD. The same firmware runs on both stations.

**Project by Nour Waleed and Hagar Ali** · Embedded Systems, May 2026

## How it works

1. Press **DOT** and **DASH** to compose a Morse sequence.
2. Press **END LETTER** to decode a letter or digit; **BACKSPACE** edits input and **SPACE** inserts a word gap.
3. Press **SEND** to transmit the composed text. The firmware sends each character as an NEC IR command and a newline command as the message terminator.
4. The receiving station reconstructs the text, plays an alert, and displays it in its recent chat log. A reply follows the same process in reverse.

The Morse symbols are decoded **before transmission**. The IR link carries text characters, rather than Morse timing pulses.

## Hardware (two stations)

- 2 × Arduino Uno, VS1838B IR receiver, IR LED with a series resistor, 16 × 2 I²C LCD (address `0x27`), and buzzer
- 12 push buttons total (six per station), external pulldown resistors, breadboards, jumpers, and power connections

| Function | Arduino Uno pin |
| --- | --- |
| Dot / dash / end letter | D4 / D5 / D6 |
| Backspace / space / send | D7 / D8 / D9 |
| Buzzer / IR transmitter / IR receiver | D10 / D3 / D11 |
| LCD SDA / SCL | A4 / A5 |

The sketch uses `INPUT` for the buttons, so the documented **external pulldown resistors are required** for stable readings.

## Build and run

1. Install the Arduino IDE and the `IRremote` and `LiquidCrystal_I2C` libraries. `Wire` ships with the Arduino core. Library versions were not supplied with the project.
2. Open [`firmware/MorseArduinoCode.ino`](firmware/MorseArduinoCode.ino) in the Arduino IDE and select an Arduino Uno.
3. Wire each station using the pin map above and the project documentation. Check the IR LED polarity and its current-limiting resistor.
4. Upload the same sketch to both boards. Point the IR transmitter of each station toward the other station's receiver.

## Project files

| File | Contents |
| --- | --- |
| [`firmware/MorseArduinoCode.ino`](firmware/MorseArduinoCode.ino) | Arduino firmware, Morse lookup, buttons, LCD, audio, and NEC IR handling |
| [`hardware/Our-Morse.brd`](hardware/Our-Morse.brd) | Autodesk EAGLE board layout supplied with the project |
| [`docs/Project-Documentation.docx`](docs/Project-Documentation.docx) | Hardware, architecture, test notes, and limitations |
| [`docs/Project-Presentation.pptx`](docs/Project-Presentation.pptx) | Project presentation |

The documentation reports successful two-way exchanges and button/LCD validation on physical hardware. This repository packages the supplied work; I did not independently compile or test it on the boards.

## Limits

- Infrared requires alignment and a clear line of sight.
- The LCD displays two rows; the sketch retains six chat entries in memory but shows only the two newest in receive mode.
- Morse input supports A–Z and 0–9. Punctuation is not part of the lookup table.
- Dynamic `String` usage and chat history consume limited Uno RAM; long messages have no explicit length bound.
