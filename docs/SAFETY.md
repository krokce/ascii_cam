# Safety guide for the MDA monitor

An MDA monitor such as the IBM 5151 can be damaged if its horizontal sync signal has the wrong frequency, because its deflection circuit is built for about 18,430 hertz only. This guide makes sure the signals are right before the monitor is ever connected. You need a multimeter with a frequency function, set to measure frequency, with its black lead on ground. Chip pin 7 is a handy ground point.

## What the appliance sends, and when

1. From the moment the power is switched on until the display driver starts, about four to six seconds later, no sync pulses are sent. During that time the Pi board's own fixed pull-up resistors hold GPIO 2 and GPIO 3 high. So the chip's horizontal sync output, pin 3, sits steady at about 5 volts, and its vertical sync output, pin 6, also sits steady at about 5 volts. Vertical sync is active low, so this level means no vertical pulse. GPIO 18 and GPIO 19 have pull-down resistors inside the Pi's processor, so video and intensity, chip pins 8 and 11, sit at 0 volts, which is black. Nothing toggles and nothing random is sent. The monitor sees the same thing as a PC whose video card has not started yet.
2. When the display driver starts, the correct MDA timing begins at once and does not stop until the power goes off. The kernel's console takes over the same timing if the camera program ever stops, and init restarts the program within about a second. So the sync signals keep running even through a program crash. This is also true in the hdmi and emulation outputs, where the MDA picture is simply black.
3. The appliance never switches the display to any other timing. The only timing it knows is the one in config.txt.
4. The one way to stop the sync signals is to put a hash sign in front of the vc4-kms-dpi-generic line in config.txt. Only do that on a Pi that has no MDA monitor attached.

For best protection, switch the monitor on after the Pi has been running for ten seconds, or power both from one switched outlet so they start together. Either way, the monitor spends at most a few seconds without sync.

## The first power-up, without the monitor

1. Make sure the monitor is not connected to the DE-9 socket.
2. Do not plug in the chip's power wire yet. Remove the jumper from Pi header pin 2.
3. Put the SD card in the Pi and switch it on. Wait thirty seconds.
4. Set the multimeter to frequency. Put the red lead on Pi header pin 5, horizontal sync, which is GPIO 3. The reading should be about eighteen thousand four hundred and thirty-seven hertz. Anything between eighteen thousand three hundred and eighteen thousand five hundred and fifty hertz is fine.
5. Put the red lead on Pi header pin 3, vertical sync, which is GPIO 2. The reading should be about forty-nine point eight hertz, so about fifty hertz. Anything between forty-nine point five and fifty point two hertz is fine.
6. If either reading is zero, or far from these values, stop. Do not connect the monitor. Check config.txt and the log file on REC, as described in the README.
7. Switch the Pi off and unplug its power. Put the jumper back on Pi header pin 2, so the chip gets its 5 volts.

## The second power-up, with the buffer chip, still without the monitor

1. Make sure the monitor is still not connected.
2. Switch the Pi on and wait thirty seconds.
3. Set the multimeter to DC volts. Put the red lead on chip pin 14. The reading should be about five volts: anything from four point seven five to five point two five volts is fine.
4. Set the multimeter to frequency. Put the red lead on chip pin 3, the horizontal sync output. The reading should be about eighteen thousand four hundred and thirty-seven hertz, as before.
5. Put the red lead on DE-9 pin 8, after the resistor. The reading should be the same, about eighteen thousand four hundred and thirty-seven hertz.
6. Put the red lead on chip pin 6, the vertical sync output. The reading should be about fifty hertz, as before.
7. Put the red lead on DE-9 pin 9. The reading should again be about fifty hertz.
8. Set the multimeter to DC volts. Put the red lead on DE-9 pin 8. A meter averages the short sync pulses, so it should read about three quarters of a volt: anything between half a volt and one and a quarter volts is fine. A reading of zero or of five volts means the pulses are missing.
9. Put the red lead on DE-9 pin 9. It should read close to five volts, a little below, about four point eight volts, because vertical sync is high except for its short pulses.
10. Optional check of video. Put an empty file named test.txt on the REC partition and restart the Pi, so the test pattern is shown. Put the red lead on DE-9 pin 7 with the meter on DC volts. It should read a steady value between a few tenths of a volt and about two volts, since the pattern is partly lit. Then delete test.txt again.
11. Switch the Pi off.

## Connecting the monitor

1. With the Pi and the monitor both switched off, plug the monitor's DE-9 plug into the socket.
2. Switch the Pi on. After about ten seconds, switch the monitor on.
3. Within a few seconds the monitor should show a steady picture. If it shows a rolling or torn picture, or you hear a loud high-pitched whine that differs from its usual faint tone, switch the monitor off at once and repeat the measurements above.
4. Adjust the monitor's brightness and contrast knobs for a comfortable picture. If the picture sits a little to one side, use the monitor's horizontal hold or position control, if it has one. The picture is about two and a half percent narrower than from a real MDA card, which is normal.

## Things never to do

1. Never connect the MDA monitor to a Pi whose config.txt has the vc4-kms-dpi-generic line switched off, or changed in any way.
2. Never connect the monitor to the Pi's GPIO pins directly, without the buffer chip.
3. Never connect a VGA monitor or a CGA monitor to this DE-9 socket. Their timing is different.
