# The recording file format

The appliance records the text picture it displays into files in the folder named recordings on the REC partition. This document describes those files completely, so that other programs can read them. The replay tool, tools/replay.py, is a working reader.

## File names

1. Each file is named after the boot it was made in and its part number within that boot, for example boot00042-part0003.acr. The boot number has five digits and the part number four, both with leading zeros, so sorting the names alphabetically also sorts the files from oldest to newest.
2. The boot number is counted up by one at every power-up and kept in the file system/bootcount.txt on REC. The appliance has no clock battery, so the names cannot carry a date.
3. A new part starts every 30 minutes, and also whenever the program restarts during the same boot.
4. Optional video recordings use the same naming with the ending .h264. They hold a raw H.264 video stream that VLC and most video players can open.
5. When the REC partition is more than 90 percent full, the oldest files are deleted, one by one, until it is below that again. The files currently being written are never deleted.

## General rules

1. All numbers are unsigned whole numbers in little-endian byte order, which means the lowest byte comes first.
2. A file is a 64-byte header followed by any number of frame records. All records in one file have the same size, which the header gives.
3. Checksums are the standard CRC-32, the same one used by ZIP files and by Python's zlib.crc32 function.
4. Data is pushed out to the card about every 5 seconds. After a power cut, the end of the file can hold an incomplete record. Readers must ignore a final record that is too short, and any record whose checksum does not match.

## The header, 64 bytes

1. Bytes 0 to 7 hold the eight letters ASCIIREC.
2. Bytes 8 and 9 hold the format version, currently 1.
3. Bytes 10 and 11 hold the header size, 64.
4. Bytes 12 and 13 hold the number of columns, 80 or 40.
5. Bytes 14 and 15 hold the number of rows, 25.
6. Bytes 16 to 19 hold the boot number.
7. Bytes 20 to 23 hold the part number.
8. Bytes 24 to 31 hold the real start time of the file, in milliseconds since the first of January 1970, Coordinated Universal Time. It is 0 when the real time is unknown, which is normal, because only a debug build with Wi-Fi can set the clock.
9. Byte 32 holds the drawing mode: 0 for characters, 1 for shade blocks, 2 for half blocks, 3 for shape matching.
10. Byte 33 holds the character set: 0 means IBM code page 437.
11. Bytes 34 and 35 hold the size of one frame record in bytes.
12. Bytes 36 to 59 are reserved and hold zeros.
13. Bytes 60 to 63 hold the CRC-32 checksum of bytes 0 to 59.

## A frame record

Here C stands for the number of character cells, columns times rows, which is 2,000 for an 80 by 25 grid and 1,000 for 40 by 25.

1. The first 4 bytes hold the four letters FRAM.
2. The next 4 bytes hold the time of the frame, in milliseconds since the file started.
3. The next C bytes hold the characters, one byte per cell, in code page 437. The cells run along the top row from left to right, then along the second row, and so on.
4. The next C divided by 8 bytes, rounded up, hold the intensity bits, one bit per cell in the same order. The lowest bit of the first byte belongs to the first cell. A bit set to 1 means the cell is drawn bright.
5. The last 4 bytes hold the CRC-32 checksum of everything before them in the record.

So an 80 by 25 record is 2,262 bytes long, and a 40 by 25 record 1,137 bytes long.

## Which frames are stored

1. A frame is stored only when it differs from the frame stored before it. The timestamps keep playback at the original speed.
2. If nothing changes, a frame is still stored at least every 5 seconds, so a reader can tell that the recorder was alive.
3. The test pattern is never recorded.

## Characters used

Besides the printable ASCII characters, codes 32 to 126, the appliance uses these code page 437 characters. Each is given with its code in decimal and its Unicode equivalent.

1. Light shade, 176, Unicode 2591.
2. Medium shade, 177, Unicode 2592.
3. Dark shade, 178, Unicode 2593.
4. Full block, 219, Unicode 2588.
5. Lower half block, 220, Unicode 2584.
6. Left half block, 221, Unicode 258C.
7. Right half block, 222, Unicode 2590.
8. Upper half block, 223, Unicode 2580.
9. The line-drawing characters 179, 196, 218, 191, 192 and 217, used only by the test pattern.
