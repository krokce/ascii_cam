# Wiring guide

This guide connects the Raspberry Pi Zero W to an IBM MDA monochrome monitor through one SN74AHCT125N buffer chip on a solderless breadboard. Read the safety guide, SAFETY.md, before you connect the monitor itself. Every step can be done by touch, counting pins and holes. Nothing in this guide needs reading small print.

## What the four signals are

The Pi drives four signals, all at 3.3 volts. The buffer chip turns them into the 5 volt signals the monitor expects.

1. Horizontal sync comes from GPIO 3, on Pi header pin 5. It goes into chip pin 2, out of chip pin 3, and through a resistor to pin 8 of the monitor connector.
2. Vertical sync comes from GPIO 2, on Pi header pin 3. It goes into chip pin 5, out of chip pin 6, and through a resistor to pin 9 of the monitor connector.
3. Video comes from GPIO 19, on Pi header pin 35. It goes into chip pin 9, out of chip pin 8, and through a resistor to pin 7 of the monitor connector.
4. Intensity comes from GPIO 18, on Pi header pin 12. It goes into chip pin 12, out of chip pin 11, and through a resistor to pin 6 of the monitor connector.
5. Power for the chip comes from the Pi's 5 volt supply on header pin 2. Ground is Pi header pin 6. The ground is shared by the Pi, the chip and the monitor connector pins 1 and 2.

The display uses no other GPIO pins. Every other pin on the header stays a plain input. GPIO 14 and 15, on header pins 8 and 10, stay free for the optional serial console. GPIO 26, on header pin 37, is used by the mode button, described at the end of this guide.

## Parts

1. One SN74AHCT125N quad buffer chip, in the 14-pin DIP package.
2. One solderless breadboard.
3. Four resistors of 47 ohms. Any value from 33 to 100 ohms works. They protect the chip outputs and soften the signal edges on the cable.
4. One ceramic capacitor of 100 nanofarads, also written 0.1 microfarad. It is strongly recommended. It steadies the chip's supply.
5. Six female-to-male jumper wires to run from the Pi header to the breadboard. You also need a few male-to-male wires or short pieces of solid wire on the breadboard.
6. One DE-9 female connector, also called a socket, with solder cups or screw terminals. The monitor's own cable ends in a male DE-9 plug with pins, so the adapter needs the female socket to accept it. An MDA card in a PC has the same female socket.

## Finding pins by touch

1. The Pi header. Lay the Pi on the table, components up, so that the row of 40 header pins runs along the edge farthest from you. The micro SD card slot should then be at your left. The header has two rows of 20 pins. The row along the very edge of the board, farthest from you, holds the even-numbered pins. The row nearer you holds the odd-numbered pins. Numbering starts at the left end, nearest the SD card slot. So the leftmost pin of the far row is pin 2, and the leftmost pin of the near row is pin 1.
2. To find an odd pin, count along the near row from the left. Pin 1 is the first pin, pin 3 the second, pin 5 the third, and pin 35 the eighteenth. To find an even pin, count along the far row from the left. Pin 2 is the first pin, pin 6 the third, and pin 12 the sixth.
3. The chip. The chip has seven legs down each long side. One short end has a small notch, a half-round dip you can feel with a fingernail. Hold the chip legs down with the notch pointing away from you. Pin 1 is then the leg nearest the notch on the left side. Pins 1 to 7 run down the left side towards you. Pin 8 is the leg nearest you on the right side. Pins 8 to 14 run up the right side away from you, so pin 14 is next to the notch on the right.
4. The DE-9 socket. Hold the socket with its solder cups or screw terminals facing you and its wider row of five on top. Seen from this back side, pins 1 to 5 run from left to right along the top row, and pins 6 to 9 run from left to right along the bottom row. The top-left cup is pin 1 and the bottom-right cup is pin 9.

## Wiring steps

Switch the Pi off and unplug its power before you start. Do not connect the monitor yet.

1. Press the chip into the middle of the breadboard so that it straddles the centre gap, with the notch pointing away from you. Each chip leg now has its own short row of holes on its side of the gap. In these steps, "connect to chip pin N" means plug the wire into a hole in the row of that leg.
2. Chip pin 14, VCC. Connect it with a jumper wire to Pi header pin 2, the 5 volt supply.
3. Chip pin 7, ground. Connect it with a jumper wire to Pi header pin 6, ground.
4. The capacitor. Put one of its legs in the row of chip pin 14 and the other leg in the row of chip pin 7, keeping the legs short. A ceramic capacitor has no polarity, so either way round is fine.
5. Chip pin 1, the first output enable. Connect it to chip pin 7, ground.
6. Chip pin 4, the second output enable. Connect it to chip pin 7, ground.
7. Chip pin 10, the third output enable. Connect it to chip pin 7, ground.
8. Chip pin 13, the fourth output enable. Connect it to chip pin 7, ground. All four enable pins are now on ground, so all four buffers always pass their signals.
9. Chip pin 2, the horizontal sync input. Connect it with a jumper wire to Pi header pin 5, GPIO 3.
10. Chip pin 5, the vertical sync input. Connect it with a jumper wire to Pi header pin 3, GPIO 2.
11. Chip pin 9, the video input. Connect it with a jumper wire to Pi header pin 35, GPIO 19.
12. Chip pin 12, the intensity input. Connect it with a jumper wire to Pi header pin 12, GPIO 18.
13. Chip pin 3, the horizontal sync output. Put one leg of a 47 ohm resistor in its row. The other leg of that resistor will go to DE-9 pin 8.
14. Chip pin 6, the vertical sync output. Put one leg of a second 47 ohm resistor in its row. The other leg will go to DE-9 pin 9.
15. Chip pin 8, the video output. Put one leg of a third 47 ohm resistor in its row. The other leg will go to DE-9 pin 7.
16. Chip pin 11, the intensity output. Put one leg of the fourth 47 ohm resistor in its row. The other leg will go to DE-9 pin 6.
17. Connect the free leg of the resistor from chip pin 3 to DE-9 pin 8, horizontal sync.
18. Connect the free leg of the resistor from chip pin 6 to DE-9 pin 9, vertical sync.
19. Connect the free leg of the resistor from chip pin 8 to DE-9 pin 7, video.
20. Connect the free leg of the resistor from chip pin 11 to DE-9 pin 6, intensity.
21. Connect DE-9 pin 1 to chip pin 7, ground.
22. Connect DE-9 pin 2 to chip pin 7, ground.
23. Leave DE-9 pins 3, 4 and 5 unconnected.

## Checking the wiring before power

1. Set the multimeter to continuity or to its lowest resistance range.
2. Between Pi header pin 2 and Pi header pin 6, you should not hear a beep, and the reading should be more than a thousand ohms. A beep means 5 volts is shorted to ground. Find the fault before you go on.
3. Between chip pin 7 and DE-9 pin 1, you should hear a beep. Do the same for DE-9 pin 2.
4. Between chip pin 3 and DE-9 pin 8, you should read about 47 ohms. Do the same from chip pin 6 to DE-9 pin 9, chip pin 8 to DE-9 pin 7, and chip pin 11 to DE-9 pin 6.
5. Between chip pin 1 and chip pin 7, you should hear a beep. Do the same for chip pins 4, 10 and 13.

Now follow the safety guide, SAFETY.md, for the first power-up and the frequency checks.

## The same pins, listed once more

1. Horizontal sync: GPIO 3, Pi header pin 5, chip input pin 2, chip output pin 3, DE-9 pin 8. It is active high.
2. Vertical sync: GPIO 2, Pi header pin 3, chip input pin 5, chip output pin 6, DE-9 pin 9. It is active low.
3. Video: GPIO 19, Pi header pin 35, chip input pin 9, chip output pin 8, DE-9 pin 7.
4. Intensity: GPIO 18, Pi header pin 12, chip input pin 12, chip output pin 11, DE-9 pin 6.
5. 5 volts: Pi header pin 2, to chip pin 14.
6. Ground: Pi header pin 6, to chip pin 7 and to DE-9 pins 1 and 2. The other ground pins on the header, 9, 14, 20, 25, 30, 34 and 39, are the same ground and may be used instead.

## The mode button

A push button steps through the three operating modes while the appliance runs: mode 1, shade blocks; mode 2, characters; mode 3, colour. You need one normally-open push button, the kind that connects only while pressed, and two female jumper wires, or a button with two wire leads ending in female connectors.

1. Switch the Pi off and unplug its power.
2. Find header pin 39. It is the last pin of the near row, the twentieth pin counting from the left, at the far end from the SD card slot. Pin 39 is ground.
3. Find header pin 37. It is the pin just before it in the same row, the nineteenth pin. Pin 37 is GPIO 26.
4. Connect one leg of the button to header pin 37.
5. Connect the other leg of the button to header pin 39.
6. Nothing else is needed. The Pi's own pull-up resistor holds GPIO 26 high, and a press connects it to ground.

The button does not affect the MDA monitor signals. The display uses only GPIO 2, 3, 18 and 19.

To check the button with the multimeter, set it to DC volts, put the black lead on pin 39 and the red lead on pin 37, with the appliance running. It reads about 3.3 volts when the button is released, and 0 volts while it is pressed.
