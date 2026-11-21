# ASCII camera appliance for the Raspberry Pi Zero W

## 1. What this project is

This project makes an appliance from these parts:

1. A Raspberry Pi Zero W. This is the original model from 2018, with the ARMv6 BCM2835 chip.
2. A Raspberry Pi camera module, version 1.3 or version 2.1, on the camera connector.
3. An IBM MDA monochrome text monitor with a 9-pin DE-9 connector.
4. Optionally, an HDMI screen on the mini-HDMI port.

When you switch on the power, the Pi starts one program. The program reads the camera picture. It changes the picture into text characters, in the style of aalib. It shows the text on the MDA monitor and on HDMI at the same time. It also records the text picture to the SD card.

The appliance has three operating modes. Mode 1 shows the picture in shade blocks. Mode 2 shows it in letters. Mode 3 shows the real colour camera picture on HDMI, while the MDA monitor shows shade blocks. A push button steps through the modes. You can also set the mode in settings.txt.

The appliance has no desktop and no login. Wi-Fi and SSH are in every image, but they are off until you switch them on in settings.txt. A debug build can add a serial console.

Other documents in this folder:

1. docs/WIRING.md tells you how to connect the buffer chip and the monitor connector.
2. docs/SAFETY.md is the safety procedure. Do it before you connect the MDA monitor for the first time.
3. docs/RECORDING-FORMAT.md describes the recording files completely.

## 2. Status of the project

### 2.1 What was built

1. A Buildroot external tree, pinned to Buildroot 2026.02.3.
2. A default configuration, named asciicam_defconfig.
3. The firmware settings file config.txt and the kernel command line cmdline.txt.
4. A small device tree overlay, mda-dpi-pins. It gives only four GPIO pins to the display.
5. The start-up script /etc/init.d/rcS and the init table /etc/inittab.
6. The C program, named asciicam, with its Makefile.
7. The PC replay tool, tools/replay.py.
8. Tests that run on the build computer.
9. The wiring guide, the safety guide and the recording format description.
10. The SD card image, output/images/sdcard.img. It is about 200 megabytes.

### 2.2 What was verified on the build computer

1. Seventy-three checks of the C code pass. They use test images and a framebuffer in memory.
2. Characters mode gives 19 tones, from a space to a bright at sign, in the correct order.
3. Shade blocks mode gives all 9 tones in the correct order.
4. Half blocks mode shows an 80 by 50 grid.
5. Shape matching mode picks left and upper half blocks for those patterns, and a diagonal character for a diagonal pattern.
6. Dithering mixes two neighbouring tones.
7. The 40 by 25 grid draws every dot twice as wide.
8. A frame that did not change redraws no cells. One changed cell redraws one cell.
9. The test pattern shows its border and its counter.
10. The HDMI gray view shows 16 gray levels.
11. The settings file is written with defaults and reads back with no change. A bad value falls back to its default.
12. A recording is written and decoded. Identical frames are skipped. Timestamps are correct. A record torn by a power cut is ignored. A new file starts after 30 minutes. Cleanup keeps the newest file.
13. The timing test reads config.txt and applies the clock rules of the Pi kernel. It finds a clean pixel clock of 33.333 megahertz. The horizontal frequency is 18,436.6 hertz and the vertical frequency is 49.83 hertz.
14. The replay tool describes a recording, exports 30 frames as plain text with the correct content, and plays all frames.
15. The SD card image was built and examined. The partitions start on 4 megabyte boundaries. The partition labels are BOOT and REC. The boot partition holds the firmware, the kernel, the device tree, the overlays, config.txt and cmdline.txt. The root filesystem holds the program and all the necessary kernel modules. The program is built for the ARMv6 core with hardware floating point. It needs only the C library and the maths library.
16. The button order is mode 1, mode 2, mode 3, then mode 1 again. A button press rewrites only the mode line of settings.txt and keeps all comments. The wifi and ssh lines are read correctly. In colour mode, the MDA text has the nine shade tones. The large mode banner is drawn correctly.
17. The start-up script reads wifi and ssh from settings.txt correctly. It ignores comment lines, and it accepts on, On and yes.
18. The image contains the Wi-Fi driver firmware, wpa_supplicant and the Dropbear SSH server. No start-up script runs them unless settings.txt switches them on.
19. Every one of the 18 settings in settings.txt has a "Possible values" line and a "Default" line. Values that are not the defaults survive writing and reading back. An old settings.txt without the version line is recognised.

### 2.3 Confirmed on the real Pi

The owner tested the appliance on the real Pi Zero W with an HDMI screen. These items work:

1. The boot, the enlarging of REC on the first boot, and the program start.
2. The picture on HDMI.
3. The camera. The log shows it running at 320 by 240, at 640 by 480 in colour mode, and at 1280 by 720 with video recording on.
4. Colour mode, set in settings.txt.
5. Wi-Fi with a WPA2 network on 2.4 gigahertz.
6. SSH with a key, and the name asciicam.local.
7. The automatic update of an older settings.txt.

The log also shows that the program starts about 7 to 9 seconds after power-on.

### 2.4 Still to check on the real hardware

1. The sync frequencies, measured with a multimeter as the safety guide tells you, and then the picture on the MDA monitor.
2. The DPI output and the HDMI output running at the same time.
3. The doubling of the picture width by the display hardware for the MDA monitor. If this fails, the program draws a double-width picture itself and writes a line in the log.
4. The mode button.
5. The files of the video recording, played on a PC.
6. Plugging in and removing an HDMI screen while the appliance runs.
7. The serial console.

### 2.5 Faults found on the real Pi and their fixes

Three faults appeared only on the real Pi. All three are fixed, and the build now checks for the first two.

1. Black screen on the first boot. The kernel modules are compressed with xz, but the module loader was built without xz support, so no driver loaded: not the display and not the camera. Fix: xz support is in the configuration, and the build stops if it is missing.
2. Wi-Fi did not start. The start-up script used pauses of a fraction of a second, such as sleep 0.5. BusyBox in this image accepts whole seconds only, so these pauses returned at once. Wi-Fi was started before the Wi-Fi chip was ready, and the wait for an address ended at once. Fix: whole-second pauses, and the build stops if a fractional pause appears in a start-up script.
3. Wi-Fi still did not join the network. The settings file that the script writes for wpa_supplicant started with a ctrl_interface line. This build of wpa_supplicant has no control socket, so it rejected the whole file. The Wi-Fi log on REC showed this. Fix: the line is removed.

## 3. GPIO and header pins

The display uses only four GPIO pins:

1. Horizontal sync is GPIO 3, on header pin 5. It is active high.
2. Vertical sync is GPIO 2, on header pin 3. It is active low.
3. Video is GPIO 19, on header pin 35.
4. Intensity is GPIO 18, on header pin 12.
5. The 5 volt supply for the buffer chip is header pin 2.
6. Ground is header pin 6.

The mode button uses one more pin:

1. The button connects GPIO 26, on header pin 37, to ground, on header pin 39.

All other GPIO pins stay plain inputs. GPIO 14 and GPIO 15, on header pins 8 and 10, stay free for the optional serial console.

The buffer chip connections are these:

1. Chip pin 2 receives horizontal sync. Chip pin 3 sends it through a 47 ohm resistor to DE-9 pin 8.
2. Chip pin 5 receives vertical sync. Chip pin 6 sends it through a 47 ohm resistor to DE-9 pin 9.
3. Chip pin 9 receives video. Chip pin 8 sends it through a 47 ohm resistor to DE-9 pin 7.
4. Chip pin 12 receives intensity. Chip pin 11 sends it through a 47 ohm resistor to DE-9 pin 6.
5. Chip pin 14 receives 5 volts. Chip pin 7 is ground.
6. Chip pins 1, 4, 10 and 13 connect to ground.
7. DE-9 pins 1 and 2 connect to ground.

The full step-by-step wiring guide is in docs/WIRING.md.

## 4. Parts list

1. A Raspberry Pi Zero W, with the 40-pin header fitted.
2. A Raspberry Pi camera module, version 1.3 or version 2.1, with the Pi Zero camera cable.
3. A micro SD card of 4 gigabytes or more. A larger card holds more recordings.
4. A 5 volt micro USB power supply of 2 amperes or more.
5. One SN74AHCT125N quad buffer chip, in the 14-pin DIP package.
6. Four resistors of 47 ohms.
7. One ceramic capacitor of 100 nanofarads. It is strongly recommended.
8. A solderless breadboard and jumper wires, female to male and male to male.
9. One DE-9 female socket, with solder cups or screw terminals. The monitor cable has the male plug, so the adapter must have the female socket.
10. A multimeter with a frequency function.
11. For HDMI: a mini-HDMI adapter or cable.
12. For the mode button: one normally-open push button and two female jumper wires.

## 5. Design decisions

Each decision has a reason. The decisions marked "user decision" were made by the owner during the planning interview, or later during the work.

### 5.1 Base system

1. The build system is Buildroot 2026.02.3. This is a long-term support release, so it gets fixes for one year. The version is recorded here and in the top-level Makefile.
2. The configuration starts from the Buildroot raspberrypi0w defconfig. It uses the Raspberry Pi kernel fork, version 6.12.61, at commit 21b410140c47ffab5668399f6f143c7d7b935c8b. It uses the Raspberry Pi firmware at commit 063bcab6c8a90efb0d19f69d88cbbc7ec79cab68. These are the versions that Buildroot 2026.02.3 pins for this board.
3. The firmware is the extended variant, start_x.elf, because the legacy camera stack needs it.
4. The root filesystem is ext4, and it is always mounted read-only. A power cut cannot damage it.
5. The REC partition is FAT32 and is mounted read-write. Any PC can read it. It holds the settings, the log and the recordings.
6. The image holds a small REC partition of 64 megabytes. On the first boot, REC grows to fill the whole card and is formatted. User decision. Reason: the image stays small, and you get all the space of the card.
7. At each boot, the start-up script checks REC with fsck.fat and repairs it if necessary. Reason: FAT has no journal, so a power cut can leave small errors.
8. BusyBox init runs one start-up script. Then it starts the program, and it starts the program again if it stops. A wrapper script waits one second between restarts and writes a line to the log.
9. No other services run. The start-up scripts of other packages are removed during the build. There is no login prompt, no system log daemon, no udev and no network.
10. The kernel drivers for the display, the camera and the video encoder are loaded by name in the start-up script. Reason: without a hotplug daemon, the kernel does not load them automatically. The kernel modules are compressed with xz, so the module loader is built with xz support. A check in the build stops the build if this support is missing. Reason: without it, no driver loads and the screen stays black. This fault happened on the first real boot and is now fixed.
11. The display drivers load in the background, so the REC check runs at the same time. Reason: a faster boot.
12. Bluetooth is switched off, both in config.txt and in the kernel. Sound support stays in the kernel, because the vc4 display driver needs it.
13. The firmware splash screen, the boot delay and automatic camera and display detection are all off. Reason: a faster boot.
14. The kernel messages go to a hidden console, tty3, and the text cursor is hidden. Reason: no text appears over the picture.

### 5.2 Camera

1. The appliance uses Option A: the legacy firmware camera stack with the bcm2835 V4L2 driver. Reason: the pinned kernel still contains this driver, and the firmware still contains start_x.elf. The GPU does all the camera work, so the slow ARMv6 core stays free for the text conversion. Option B, libcamera, runs its control loops on the ARM core and needs a C++ runtime. That is heavy for this chip.
2. Both camera boards are supported, version 1.3 with the OV5647 sensor and version 2.1 with the IMX219 sensor. The firmware detects the sensor automatically. User decision.
3. The camera delivers 320 by 240 pixels in YUV 4:2:0 format. The program uses only the brightness plane.
4. When video recording is on, the camera delivers 1280 by 720 pixels. The same picture feeds the text conversion and the video encoder. User decision. Reason: real HD video, with an acceptable load. The text picture can slow toward 10 frames per second while video is on.
5. In colour mode, the camera delivers 640 by 480 pixels, or 1280 by 720 when video recording is on. Reason: the colour picture needs more detail than the text. When the mode changes to or from colour, the program closes and opens the camera with the new size. This takes less than one second.
6. Two settings, upside_down and mirror, turn the picture. The camera does the turning itself when it can, so video recordings are turned too. User decision. Reason: a camera is often mounted upside down, and a camera that faces the viewer looks more natural as a mirror image.
7. If no camera is found, the program shows the test pattern with a status line. It writes a line to the log and tries the camera again every 10 seconds.

### 5.3 Display and the DPI port

1. The vc4 KMS display driver is used. Reason: it can drive the DPI port and HDMI at the same time. The old firmware display path can drive only one screen.
2. The DPI port is set up with the stock overlay, vc4-kms-dpi-generic, in config.txt. The timing values are on that line.
3. The DPI port uses the 16-bit RGB565 format. Reason: this format puts two data bits on GPIO 18 and GPIO 19. These pins have pull-down resistors, so they stay low, which is black, during boot. These pins also do not touch the serial console pins.
4. A second small overlay, mda-dpi-pins, gives only GPIO 2, 3, 18 and 19 to the display. User decision. Reason: the stock overlay would take GPIO 0 to 19, which includes the serial console pins. The MDA monitor does not need the DPI clock pin or the data-enable pin.
5. Video is red bit 7 on GPIO 19. Intensity is red bit 6 on GPIO 18.
6. The Pi cannot make the MDA dot clock of 16.257 megahertz exactly. Its DPI clock divider takes a 19.2 megahertz crystal or a 500 megahertz source, and the divider is at most 15 plus a fraction. A fractional divider makes the dots uneven in width. So the DPI port runs at 33.333 megahertz, which is 500 divided by 15, a clean whole-number divider. Every MDA dot is sent as two DPI pixels. The DPI mode is therefore 1440 by 350. User decision. Reason: clean, even dots and a horizontal frequency only 0.036 percent from the MDA value. The cost is a picture about 2.5 percent narrower than from a real MDA card.
7. The program still draws each frame only once, into a buffer of 720 by 350 dots. The display hardware doubles the width for the DPI port, with sharp nearest-neighbour scaling.
8. The buffer holds three levels per dot. Off is 0. Normal is 128, which sets only the video bit. Bright is 255, which sets the video bit and the intensity bit.
9. The DPI port keeps its sync signals running in all output modes. In the hdmi and emulation outputs, the MDA picture is black, but the sync continues. Reason: the MDA monitor is protected even if the settings are wrong.
10. The timing is in config.txt and in the device tree, so the kernel console uses the same timing. Reason: the sync continues while init restarts a stopped program.

### 5.4 MDA timing and its source

The timing comes from the IBM PC BIOS video parameter table for mode 7, the MDA text mode. The IBM Personal Computer Technical Reference lists it. Emulators such as 86Box and MAME use the same values. The 6845 display controller of the MDA card gets these values: horizontal total 97, horizontal displayed 80, horizontal sync position 82, sync width 15, vertical total 25, vertical adjust 6, vertical displayed 25, vertical sync position 25. A character is 9 dots wide and 14 lines high.

1. Horizontally, the total is 98 characters, which is 882 dots. The visible part is 720 dots. The front porch is 18 dots. The sync pulse is 135 dots, about 8.3 microseconds. The back porch is 9 dots. At 16.257 megahertz this gives 18,432 hertz.
2. Vertically, the total is 26 rows of 14 lines plus 6 lines, which is 370 lines. The visible part is 350 lines. The sync pulse is 16 lines. The back porch is 4 lines. This gives 49.82 hertz.
3. Horizontal sync is active high. Vertical sync is active low.

The appliance uses these values in DPI pixels:

1. Horizontally: 1440 visible, front porch 58, sync 270, back porch 40, total 1808. The sync pulse lasts 8.1 microseconds. The extra 44 pixels make the frequency correct with the 33.333 megahertz clock. They are divided equally between the two porches.
2. Vertically: 350 visible, front porch 1, sync 16, back porch 3, total 370. The front porch is 1 line, not 0, because the display driver needs at least one line there.
3. The horizontal frequency is 18,436.6 hertz. The vertical frequency is 49.83 hertz.

### 5.5 Safety during boot

1. From power-on until the display driver starts, about four to six seconds, no sync pulses are sent. The Pi board has fixed pull-up resistors on GPIO 2 and GPIO 3. So horizontal sync and vertical sync both stay high. Video and intensity stay low, which is black. Nothing random is sent. User decision: accept this and document it. Reason: the monitor sees the same as a PC whose video card has not started yet.
2. The four output-enable pins of the buffer chip stay connected to ground, as in the original specification.
3. Advice: switch on the monitor about ten seconds after the Pi, or use one switch for both.
4. The full procedure is in docs/SAFETY.md.

### 5.6 HDMI

1. HDMI shows the same 720 by 350 buffer as the DPI port. The display hardware scales it to a centred area with 4 by 3 proportions, with black bars at the sides. User decision. Reason: this matches the shape of the picture on the MDA monitor. Square dots would give a flat, wide picture.
2. The scaling is sharp nearest-neighbour scaling.
3. A colour table in the display hardware colours the text. Normal becomes 65 percent of the colour, and bright becomes the full colour. The colours are green, amber, white and red, always on black. Reason: the processor does no extra drawing.
4. The appliance prefers the 1280 by 720 mode. If the screen does not offer it, the appliance uses the preferred mode of the screen, up to 1920 by 1200.
5. The program checks for an HDMI screen every 2 seconds. It starts or stops HDMI automatically. The MDA output and the recording continue without a pause. User decision. Reason: a TV can be switched on after the Pi.
6. If nothing is connected to HDMI, the program skips HDMI with no error.
7. In colour mode, HDMI shows the camera picture in colour. The program copies each camera frame, in YUV 4:2:0 format, into a buffer. The display hardware changes it to RGB and scales it to the HDMI screen, in the proportions of the camera picture. No colour table is used. Reason: the processor does only one copy per frame.
8. The optional gray view shows 16 real gray levels on HDMI. The MDA monitor keeps its three levels. The gray view uses a second buffer, which the program draws separately. It is off by default.

### 5.7 MDA emulation on HDMI

1. The emulation output is for a second Pi that has only an HDMI screen. The same SD card image runs on both Pis. Only the settings are different.
2. In emulation, HDMI shows the same 720 by 350 buffer with only the three MDA levels. The picture fills a centred 4 by 3 area, stretched vertically.
3. The phosphor colour is green, amber or white.
4. Scan lines are optional and off by default. They are a separate layer, drawn once, that the display hardware places over the picture.
5. Glow is optional and off by default. It is a small copy of the picture, a quarter of the size in each direction. The display hardware enlarges it with smooth filtering under the sharp text.
6. The second Pi needs nothing on the GPIO header. The mode button is optional there too. If you want, you can switch the DPI port off with one line in config.txt. See section 10.

### 5.8 Conversion and drawing

1. For each frame, the program averages the brightness picture down to the grid. The grid has one value per cell for characters and shade blocks, two values per cell for half blocks, and four values per cell for shape matching.
2. Contrast stretching then spreads the values over the full range. It uses the darkest 2 percent and brightest 2 percent of the cells as limits. The limits move only 5 percent toward the new values in each frame. Reason: the picture does not flicker, and a new scene settles in one or two seconds.
3. The contrast setting then makes the picture softer or harder.
4. Dithering is optional and off by default. It spreads rounding errors to the neighbouring cells. It has no effect in shape matching mode.
5. The four drawing modes are these:
   1. Shade blocks is the default. It uses empty, light shade, medium shade, dark shade and full block, each in normal and bright. That gives 9 tones.
   2. Characters uses a ramp of characters. The default ramp is space, dot, colon, dash, equals, plus, star, hash, percent and at sign. Every character except the space also appears in bright. That gives 19 tones.
   3. Half blocks divides each cell into a top half and a bottom half. That gives an 80 by 50 grid. There are fewer tones, because the intensity bit applies to the whole cell.
   4. Shape matching looks at the four quarters of each cell. It picks the character and the intensity whose shape fits the light and dark pattern best, as aalib does. The results are kept in a table, so each pattern is calculated only once.
6. The grid is 80 by 25, or 40 by 25 with characters drawn twice as wide. User decision.
7. The font is 9 by 14 dots. The 80 columns of 9 dots fill 720 dots, and the 25 rows of 14 lines fill 350 lines.
8. The letters come from Terminus Font 8x14, which has the SIL Open Font License 1.1. A ninth blank column is added, as on a real MDA card. The IBM ROM font is not used.
9. The program draws the block, shade and line characters itself. They do not come from the font.
10. The program draws only the cells that changed since the last frame.

### 5.9 Recording

1. Every displayed frame goes into a simple file on REC. The file has a 64-byte header and fixed-size frame records. Each record holds the 2,000 characters in code page 437, the 2,000 intensity bits, a timestamp in milliseconds and a CRC-32 checksum.
2. The Pi has no clock battery, and the network is off. So the files are named by boot number and part number, for example boot00042-part0003.acr. Timestamps count from the start of each file. A debug build with Wi-Fi also writes the real start time into the header. User decision.
3. The program writes a frame only when it is different from the last frame written. But it writes one frame at least every 5 seconds. User decision. Reason: a quiet scene does not fill the card, and a reader can still see that the recorder was working.
4. A new file starts every 30 minutes.
5. The program pushes the data to the card every 5 seconds.
6. When REC is more than 90 percent full, the oldest files are deleted. The newest files are never deleted.
7. A power cut can leave an incomplete record at the end of a file. Readers ignore it, because of its size and its checksum.
8. Optional video recording, off by default, uses the hardware H.264 encoder. It writes raw H.264 files that VLC can play. The rate is about 2 megabits per second, which is about 1 gigabyte per hour. The stream headers repeat before each key frame, every 2 seconds, so any part of a file can be played.
9. The test pattern is never recorded.
10. The full file format is in docs/RECORDING-FORMAT.md.

### 5.10 Settings, test mode and log

1. The settings are in settings.txt on REC. Each setting is on one line. A comment in plain words explains each setting. Each comment ends with a line that lists all possible values and a line that gives the default. Reason: you can change a setting without other documents.
2. The program reads the settings when it starts. If settings.txt is missing, the program writes a new one with the default values.
3. A bad line causes a sentence in the log, and the program uses the default for that setting.
4. If a file named test.txt is on REC, the program shows the test pattern. The pattern has a border, all tones of all modes and a large counter. The program checks for test.txt every 2 seconds.
5. The log is log.txt on REC. Each line is a plain sentence. It starts with the boot number and the seconds since power-on. At one megabyte, the log becomes log-old.txt and a new log starts.
6. There is no configuration tool for the PC. You edit the text files on REC by hand. User decision.
7. The first line of settings.txt is "# Settings file version 2". The program writes the file from one template, filled with the current values. When the program finds a settings.txt without this line, it writes the file again with the new comments and keeps every value. It writes one sentence about this in log.txt. Reason: older cards get the new comments without hand work. Lines with unknown names are reported in the log and are not kept.

### 5.11 Wi-Fi, SSH and the serial console

1. Wi-Fi and SSH are in every image. They are off by default. The lines wifi = on and ssh = on in settings.txt switch them on. No rebuild is necessary. User decision. This replaces the earlier decision that Wi-Fi and SSH exist only in a special debug build.
2. While Wi-Fi and SSH are off, no network software runs, and the boot time does not change.
3. ssh = on also switches on Wi-Fi.
4. Nothing secret is built into the image. The Wi-Fi name and password come from wifi.txt on REC. The SSH keys come from authorized_keys.txt on REC. User decision.
5. At every boot, the Pi writes a template wifi.txt and a template authorized_keys.txt to REC if they are missing. So you can fill them in at any time. User decision.
6. SSH accepts only key logins for root. It refuses passwords.
7. The SSH host key is kept on REC, so it stays the same after a restart.
8. When Wi-Fi connects, the Pi writes its network address to log.txt. Reason: you can find the address without a screen.
9. The network name of the Pi is asciicam. The Pi sends this name to the router when it asks for an address, so the router's list of devices shows it by name.
10. The small mDNS service mdnsd answers to the name asciicam.local, so you can use the name instead of the address. It runs only while Wi-Fi is on. It announces SSH only when ssh = on. It always announces a small device information service, because mdnsd answers to the name only when it has at least one service to announce.
11. If Wi-Fi or SSH is switched on but its file still holds the example text, the log says so.
12. The serial console stays a build option, off by default. It uses GPIO 14 and GPIO 15 and does not conflict with the display.
13. The messages of wpa_supplicant, the program that joins the network, go to system/wifi-log.txt on REC. The file is renewed at every boot. The password is not written there. Reason: you can see why Wi-Fi fails without a screen.
14. The settings file for wpa_supplicant holds only the network name and password. It has no ctrl_interface line, because this build of wpa_supplicant has no control socket and rejects the file if the line is there.
15. If Wi-Fi fails, log.txt says which step failed: the Wi-Fi chip did not appear, the Pi could not join the network, or it joined but got no address from the router.
16. All pauses in the start-up script are whole seconds, because BusyBox sleep here does not accept fractions.

### 5.12 Operating modes and the mode button

1. There are three operating modes. User decision.
   1. Mode 1 is shade blocks. It is the default.
   2. Mode 2 is letters, which is the characters drawing mode with the ramp of characters. User decision. The shape matching mode and the half blocks mode stay available in settings.txt, but they are not on the button.
   3. Mode 3 is colour. HDMI shows the real colour camera picture. The MDA monitor shows the text picture in shade blocks at the same time. Text recording continues. User decision. Reason: the MDA monitor can show only three brightness levels.
2. In settings.txt, the mode line takes the values shade, characters and colour, or the numbers 1, 2 and 3. The values halfblocks and shapes are also allowed.
3. A push button steps through the three modes while the appliance runs. No restart is necessary. User decision.
4. The button connects header pin 37, GPIO 26, to header pin 39, ground. These are the last two pins of the near row, so they are easy to find by touch. The Pi's own pull-up resistor holds the pin high. Presses closer together than 300 milliseconds count as one press.
5. A button press rewrites only the mode line of settings.txt. All comments and other lines stay the same. The new file is written next to the old one and then renamed, so a power cut leaves one complete file. Reason: settings.txt is the only place that holds the mode, so the button and your hand edits always agree.
6. After a press, the word MODE and the mode number appear in large block letters on both screens for 2 seconds. The banner is not recorded.
7. After a mode change, a new recording file starts, so that its header names the new mode. In colour mode, the header names shade blocks, because that is what the MDA monitor shows.
8. Every mode change is written to log.txt.
9. The program reads the button through the kernel GPIO character device. It uses only the C library. The compiler's kernel headers are older than this interface, so the program carries a copy of the necessary definitions from the Linux 6.12 headers, in src/gpio_v2_compat.h.

## 6. How the software works

### 6.1 The boot sequence

1. The GPU firmware starts from the BOOT partition. It reads config.txt, loads the device tree and the overlays, and starts the kernel.
2. The kernel mounts the root filesystem read-only and starts BusyBox init.
3. Init runs /etc/init.d/rcS. The script does these steps:
   1. It mounts the temporary filesystems.
   2. It loads the display drivers in the background: i2c_bcm2835 for the HDMI screen data, panel_simple for the DPI port, and vc4.
   3. It loads the camera driver and the video encoder driver in the background.
   4. On the first boot only, it grows REC and formats it.
   5. It checks REC and mounts it.
   6. It adds one to the boot count in system/bootcount.txt on REC.
   7. It writes the Wi-Fi and SSH templates if they are missing. If settings.txt says so, it starts Wi-Fi and SSH. When the Pi has an address, it starts mdnsd for the name asciicam.local.
   8. Thirty seconds after boot, it saves the kernel log and the list of loaded drivers to REC.
4. Init starts the program through the wrapper /usr/bin/asciicam-run. If the program stops, init starts it again.

### 6.2 The program

1. The program reads the boot number and the settings.
2. It opens the KMS display device. It waits up to 10 seconds for the driver.
3. It sets up the DPI output and the HDMI output with one atomic commit. First it tests whether the display hardware accepts the width doubling. If not, it uses a double-width buffer.
4. It opens the camera.
5. Then it repeats these steps for each frame:
   1. It checks the mode button. After a press, it changes the mode, rewrites settings.txt and shows the banner.
   2. It checks for test.txt every 2 seconds.
   3. It takes a camera frame. When video is on, it gives the frame to the encoder.
   4. It applies the frame rate limit.
   5. It converts the frame to the grid. In colour mode, it also copies the frame for HDMI.
   6. It draws the changed cells.
   7. It records the frame.
   8. Every 2 seconds, it checks the HDMI connection.
6. The program uses only the C library and the maths library. It talks to the display and the camera directly through the kernel interfaces.

### 6.3 Source files

1. src/main.c is the main loop.
2. src/convert.c changes the camera picture into the grid, for all four modes.
3. src/render.c draws the cells into the buffer.
4. src/font.c makes the glyphs. src/font_data.c holds the letters of the font.
5. src/testpattern.c makes the test pattern.
6. src/settings.c reads and writes settings.txt.
7. src/recorder.c writes the recordings and deletes old files.
8. src/log.c writes the log.
9. src/kms.c controls the displays.
10. src/camera.c controls the camera.
11. src/video.c controls the video encoder.
12. src/button.c reads the mode button. src/gpio_v2_compat.h holds the GPIO interface definitions for older compiler headers.
13. tests/test_core.c and tests/test_replay.py are the tests.

### 6.4 The project folders

1. br2-external is the Buildroot external tree.
   1. configs/asciicam_defconfig is the configuration.
   2. board/asciicam holds config.txt, cmdline.txt, the overlay source, the kernel configuration fragment, the image layout genimage.cfg, the post-build script, the post-image script and the root filesystem overlay with rcS, inittab and the wrapper.
   3. package/asciicam holds the package for the program and the serial console option.
2. src holds the C program.
3. tests holds the tests.
4. tools holds replay.py and psf2c.py. The second tool made the font file.
5. docs holds the wiring guide, the safety guide and the recording format.
6. buildroot holds the pinned Buildroot source. The Makefile gets it.
7. output holds the build. The finished image is output/images/sdcard.img.

## 7. Building the image

### 7.1 Requirements of the build computer

1. Use a 64-bit Linux PC. Ubuntu 22.04, Ubuntu 24.04 and Debian 12 work.
2. Keep at least 20 gigabytes of free disk space.
3. Use at least 4 gigabytes of memory.
4. Make sure the computer can connect to the internet. The first build downloads about 600 megabytes.
5. On the development computer, with 8 processor cores, the first build took about 54 minutes. On a typical PC, expect one to three hours. A rebuild after a change to the program takes about one minute.

### 7.2 Build steps

1. Install the packages. Type this command as one line:
   sudo apt install build-essential git unzip rsync bc cpio file wget perl python3 libncurses-dev
2. Open a terminal in the project folder.
3. Type make image and press Enter. The Makefile gets Buildroot 2026.02.3, configures it and builds everything.
4. Wait until the message says: The SD card image is output/images/sdcard.img.
5. Type make test and press Enter. The output should say that all checks passed.

Note: the Makefile clears the LD_LIBRARY_PATH variable during the build. Buildroot does not build when that variable has an empty entry.

### 7.3 Rebuild after a change to the program only

1. Type make program and press Enter.
2. The new image is output/images/sdcard.img.

## 8. Writing the image to the SD card

Warning: writing the image erases everything on the card, including old recordings. Copy the recordings to your PC first.

1. Put the SD card in the PC.
2. Type lsblk and press Enter.
3. Find the disk whose size matches the card, for example sdb. Make sure that you have the correct name. If you write to the wrong disk, you destroy its data.
4. If the PC mounted partitions of the card, unmount them. For example, type: sudo umount /dev/sdb1 /dev/sdb2 /dev/sdb3
5. Type this command as one line. Replace sdX with the name of the card:
   sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M conv=fsync status=progress
6. Wait until the command ends.
7. Type sync and press Enter.
8. Remove the card from the PC.

## 9. First boot and first test

### 9.1 Test on a TV first, without the MDA monitor

1. Make sure nothing is connected to the GPIO header.
2. Connect the camera to the camera connector of the Pi.
3. Connect a TV or a computer monitor to the mini-HDMI port.
4. Put the SD card in the Pi.
5. Connect the power supply.
6. Wait. The first boot takes longer than usual, because REC grows to fill the card and is formatted.
7. After the first boot, the TV shows the camera picture as green text, in the shade blocks mode, in a 4 by 3 area with black bars at the sides.
8. If the TV shows the test pattern with the line "No camera found", check that the camera cable is fully inserted at both ends and that both connector clips are closed.
9. To show the test pattern, do these steps:
   1. Disconnect the power.
   2. Put the SD card in your PC.
   3. Make an empty file named test.txt on the REC partition.
   4. Put the card back in the Pi and connect the power.
   5. To stop the test pattern, delete test.txt.
10. To see on the TV exactly what the MDA monitor will show, set output = emulation in settings.txt. See section 11.

### 9.2 Connect the MDA monitor

1. Wire the buffer chip as docs/WIRING.md tells you.
2. Do all the steps of docs/SAFETY.md. Measure the frequencies before you connect the monitor. The horizontal sync must be about eighteen thousand four hundred and thirty-seven hertz. The vertical sync must be about fifty hertz.
3. With the Pi and the monitor both off, connect the monitor plug to the DE-9 socket.
4. Switch on the Pi.
5. Wait about ten seconds. Then switch on the monitor.
6. Make sure that settings.txt has output = both or output = mda. Both is the default.

### 9.3 Normal use

1. Connect the power. The picture appears after the boot.
2. To stop the appliance, disconnect the power. You do not need to shut it down. The root filesystem is read-only, and the recordings are pushed to the card every 5 seconds.
3. At most the last 5 seconds of recording can be lost.

## 10. The second Pi, with only an HDMI screen

1. Write the same image to a second SD card. See section 8.
2. Boot the second Pi once with the card, so that REC grows and settings.txt is written.
3. Disconnect the power and put the card in your PC.
4. Open settings.txt on REC.
5. Change the output line to: output = emulation
6. Choose the phosphor colour. For example: phosphor = amber
7. If you want scan lines, write: scanlines = on
8. If you want glow, write: glow = on
9. Save the file. Put the card back in the Pi and connect the power.
10. Optional: you can switch off the DPI port completely. Open config.txt on the BOOT partition. Put a hash sign at the start of the line that begins with dtoverlay=vc4-kms-dpi-generic. Save the file. Warning: never do this on a Pi that drives an MDA monitor.

## 11. Changing the settings

1. Disconnect the power.
2. Put the SD card in your PC.
3. Open settings.txt on the REC partition with a text editor.
4. Find the setting. Read the lines above it, which start with "Possible values" and "Default". Change the value to one of the possible values. Keep the name, the equals sign and the value on one line.
5. Save the file.
6. Put the card back in the Pi and connect the power. The program reads the settings when it starts.

The settings are these. Each item gives all possible values and the default. The same lists are in the comments of settings.txt.

1. mode: the operating mode.
   1. shade, or 1, is mode 1, shade blocks.
   2. characters, or 2, is mode 2, letters.
   3. colour, or 3, is mode 3, the colour camera video on HDMI. color is also accepted.
   4. halfblocks and shapes are two more drawing modes. They are not on the button.
   5. The default is shade. The mode button rewrites this line.
2. output: where the picture goes. Possible values: both, mda, hdmi, emulation. The default is both.
3. hdmi_colour: the text colour on HDMI. The name hdmi_color is also accepted. Possible values: green, amber, white, red. The default is green.
4. hdmi_gray: 16 real gray levels on HDMI. The name hdmi_grey is also accepted. Possible values: on, off. The default is off.
5. phosphor: the colour in the emulation output. Possible values: green, amber, white. The default is green.
6. scanlines: faint scan lines in the emulation output. Possible values: on, off. The default is off.
7. glow: a soft glow in the emulation output. Possible values: on, off. The default is off.
8. grid: the grid size. Possible values: 80x25, 40x25. The default is 80x25.
9. ramp: the characters for mode 2, darkest first, between double quotes. Possible values: 2 to 60 printable characters. The default is " .:-=+*#%@".
10. dithering: spreads rounding errors to neighbouring cells. Possible values: on, off. The default is off.
11. contrast: the contrast in percent. Possible values: any whole number from 50 to 200. The default is 100.
12. frame_rate: the highest number of frames per second. Possible values: any whole number from 1 to 30. The default is 15.
13. recording: the text recording. Possible values: on, off. The default is on.
14. video: the video recording at 1280 by 720. Possible values: on, off. The default is off.
15. upside_down: for a camera mounted upside down. Possible values: on, off. The default is off.
16. mirror: shows the picture mirrored left to right. Possible values: on, off. The default is off.
17. wifi: Wi-Fi. Possible values: on, off. The default is off.
18. ssh: SSH. It also switches on Wi-Fi. Possible values: on, off. The default is off.

For all settings with on and off, the program also accepts yes, no, 1 and 0.

If you delete settings.txt, the program writes a new one with the default values at the next start.

If settings.txt comes from an older version of the program, the program updates its comments at the next start and keeps all values. A missing setting gets its default value.

## 12. Recordings and the replay tool

### 12.1 Where the recordings are

1. Text recordings are in the folder recordings on REC. Their names end in .acr.
2. Video recordings, if switched on, are in the same folder. Their names end in .h264.
3. A busy scene at 15 frames per second uses up to about 120 megabytes per hour for text. Video uses about 1 gigabyte per hour.

### 12.2 Playing a recording on a PC

The replay tool needs only Python 3. It uses no other packages.

1. Copy the recording from the card to your PC, or use it directly from the card.
2. To play a recording at its original speed, type this as one line:
   python3 tools/replay.py boot00042-part0003.acr
3. While it plays, use these keys:
   1. The space bar pauses and continues.
   2. The plus key doubles the speed.
   3. The minus key halves the speed.
   4. The n key shows the next frame while paused.
   5. The q key stops the tool.
4. To play four times faster, add --speed 4 before the file name.
5. To get a short description of the recording, add --info before the file name. The tool says how many frames there are, how long the recording is and which mode it used.
6. To write every frame as a plain text file, add --export followed by a folder name. For example:
   python3 tools/replay.py --export frames boot00042-part0003.acr
   A screen reader can read these files line by line.
7. To play video files, use VLC.

## 13. Operating modes and the mode button

### 13.1 Using the button

1. Connect the button as docs/WIRING.md tells you, in the section "The mode button".
2. With the appliance running, press the button once. The mode changes to the next one: from mode 1 to mode 2, from mode 2 to mode 3, and from mode 3 back to mode 1.
3. The word MODE and the new number appear in large block letters on the screens for 2 seconds.
4. The new mode is written into settings.txt, so the appliance starts in this mode next time.
5. If the mode was halfblocks or shapes, one press goes to mode 1.

### 13.2 Setting the mode by hand

1. Disconnect the power and put the SD card in your PC.
2. Open settings.txt on REC.
3. Change the mode line. For example, for colour, write: mode = colour
4. Save the file. Put the card back in the Pi and connect the power.

### 13.3 What each mode shows

1. Mode 1, shade blocks: both screens show the picture in shade block characters, with nine tones.
2. Mode 2, characters: both screens show the picture in letters and signs, from the ramp in settings.txt, with 19 tones.
3. Mode 3, colour: this is the camera video mode. HDMI shows the real colour camera video, not text. The MDA monitor shows shade blocks at the same time. To get mode 3, press the button until the banner says MODE 3, or write mode = colour or mode = 3 in settings.txt. If output is set to mda, colour mode looks the same as mode 1.

### 13.4 Which outputs are on

1. The DPI output on the GPIO header sends its sync signals all the time. They start about five seconds after power-on and run in every mode and with every output setting. Reason: the MDA monitor is protected.
2. The MDA monitor shows the text picture when output is both or mda. It shows a black picture, with the sync still running, when output is hdmi or emulation.
3. The DPI output is fully off only when the line that begins with dtoverlay=vc4-kms-dpi-generic in config.txt has a hash sign in front of it. Do this only on a Pi without an MDA monitor.
4. HDMI runs only while a screen is connected and output is not mda. The program checks for a screen every 2 seconds and starts or stops HDMI by itself.
5. When output is mda, mode 3 shows no extra picture, because HDMI is off.

## 14. Wi-Fi, SSH and the serial console

### 14.1 Switching on Wi-Fi

No rebuild is necessary.

1. Boot the Pi at least once with the card, so that REC is enlarged and the template files are written. Important: the first boot formats REC, which erases any file that you put there before.
2. Disconnect the power. Put the card in your PC.
3. Open wifi.txt on REC.
4. Change the network line to the name of your network. For example: network = MyHomeNetwork
5. Change the password line to the password of your network. For example: password = my secret password
6. Save the file.
7. Open settings.txt on REC.
8. Find the line wifi = off and change it to: wifi = on
   If there is no wifi line, add the line at the end of the file.
9. Save the file. Put the card back in the Pi and connect the power. Wait one minute.
10. Disconnect the power and put the card in your PC.
11. Open log.txt on REC. Find the line with the words "Wi-Fi is connected". It gives the network address of the Pi, for example 192.168.1.57.
    You can also use the name asciicam.local instead of the address, from any PC that understands mDNS names. Ubuntu, Debian with Avahi, macOS and Windows 10 or newer do. In your router's list of devices, the Pi appears with the name asciicam.
12. If the log says that Wi-Fi could not join the network, check the network name and the password in wifi.txt. The Pi Zero W can use only 2.4 gigahertz networks. The file system/wifi-log.txt on REC holds the messages of the Wi-Fi program, with the reason. See section 16.

When Wi-Fi is connected, the Pi also sets its clock from the internet. Then new recording files get their real start time.

### 14.2 Switching on SSH

No rebuild is necessary. SSH also switches on Wi-Fi.

1. Fill in wifi.txt, as in section 14.1, steps 1 to 6.
2. If you do not have an SSH key on your PC, make one. Type: ssh-keygen -t ed25519
   Press Enter at each question to accept the default.
3. Show your public key. Type: cat ~/.ssh/id_ed25519.pub
4. Open authorized_keys.txt on REC.
5. Delete the example line, which contains the words example-key.
6. Paste your public key as one line.
7. Save the file.
8. Open settings.txt on REC. Change the line ssh = off to: ssh = on
   If there is no ssh line, add the line at the end of the file.
9. Save the file. Put the card back in the Pi and connect the power. Wait one minute.
10. Disconnect the power and put the card in your PC. Find the network address in log.txt, as in section 14.1, step 11.
11. Put the card back in the Pi and connect the power. Wait one minute.
12. On your PC, type: ssh root@asciicam.local
    If the name does not work, type ssh root@ followed by the address from log.txt. For example: ssh root@192.168.1.57
13. The first time, SSH asks if you trust the host key. Type yes and press Enter.

Note: the root filesystem is read-only. To change files on it, change the project on the build computer and build a new image.

### 14.3 Switching Wi-Fi and SSH off

1. Open settings.txt on REC.
2. Change the lines to wifi = off and ssh = off.
3. Save the file. At the next start, no network software runs.

### 14.4 Switching on the serial console

The serial console is a build option.

1. Open br2-external/configs/asciicam_defconfig in a text editor.
2. Find the line: # BR2_PACKAGE_ASCIICAM_SERIAL_CONSOLE is not set
3. Replace it with: BR2_PACKAGE_ASCIICAM_SERIAL_CONSOLE=y
4. Save the file. Type make image and press Enter. Write the new image to the card.
5. Use a USB serial adapter for 3.3 volts. Do not use a 5 volt adapter.
6. Connect the receive wire of the adapter to header pin 8. This is GPIO 14, the transmit pin of the Pi.
7. Connect the transmit wire of the adapter to header pin 10. This is GPIO 15, the receive pin of the Pi.
8. Connect the ground wire of the adapter to header pin 6.
9. On the PC, open the serial port at 115200 baud. For example, type: screen /dev/ttyUSB0 115200
10. Connect the power of the Pi. A login prompt appears.
11. Type root and press Enter. The serial console needs no password.

To switch the serial console off again, change the line back to "is not set", delete the output folder with rm -rf output, and type make image. Reason: Buildroot does not remove a package from an existing build.

## 15. Updating a card that is already in use

When only the program or the root filesystem changed, you can update the card without losing REC, its settings and its recordings.

1. Build the new image with make image or make program.
2. Put the card in the PC. Type lsblk and find the card, for example sdd.
3. Make sure no partition of the card is mounted.
4. Open a normal terminal window, outside any program that cannot ask for a password.
5. Type this as one line, with the name of your card, and press Enter:
   sudo dd if=output/images/rootfs.ext2 of=/dev/sdd2 bs=4M conv=fsync status=progress
   Type your password when asked.
6. Type sync and press Enter. Remove the card.

This writes only partition 2, the root filesystem. If config.txt or cmdline.txt also changed, write the whole image instead, as in section 8. That erases REC.

## 16. Fault finding

1. First, read log.txt on REC. Each line is a plain sentence.
2. Thirty seconds after each boot, the Pi saves the kernel log to system/kernel-log.txt and the list of loaded drivers to system/loaded-drivers.txt on REC. If the display driver or the camera driver did not load, log.txt says so.
3. If the log says "No camera was found", check that the camera cable is fully inserted at both ends and that both connector clips are closed.
4. If the log says "The display driver could not be opened", the image or config.txt may be damaged. Write the image again.
5. If the log says that a line of settings.txt has a bad value, correct that line.
6. If the log says that the REC check repaired problems, this is normal after a power cut.
7. If the MDA monitor shows no picture, do the measurements in docs/SAFETY.md again.
8. If the HDMI screen shows no picture, make sure that output is not set to mda.
9. If Wi-Fi does not work, read the Wi-Fi line in log.txt. Then read system/wifi-log.txt on REC, which holds the messages of the Wi-Fi program. A message with WRONG_KEY means a wrong password. A message with NETWORK-NOT-FOUND means the Pi does not see the network: check the name, and make sure the network has a 2.4 gigahertz band.
10. If the name asciicam.local does not work, use the address from log.txt. The name needs a PC that understands mDNS names.

## 17. Licences

1. Terminus Font has the SIL Open Font License 1.1. The licence text is in src/FONT-LICENSE.txt. The font in this project is a modified version and does not use the reserved name Terminus Font.
2. Buildroot, the Linux kernel and BusyBox have their own licences. To list them, type make legal-info in the output folder.
