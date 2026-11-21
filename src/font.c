/* Glyphs for the 9 by 14 character cell. Printable ASCII comes from the
 * font; shade, block and line characters are generated here. A row is
 * 9 bits wide, bit 8 being the leftmost dot. */
#include "asciicam.h"
#include "font.h"

#define ROW_ALL 0x1FF

void glyph_rows(uint8_t ch, uint16_t rows[CELL_H])
{
	int y;

	for (y = 0; y < CELL_H; y++) {
		uint16_t r = 0;

		switch (ch) {
		case CH_FULL:
			r = ROW_ALL;
			break;
		case CH_UPPER:
			r = y < CELL_H / 2 ? ROW_ALL : 0;
			break;
		case CH_LOWER:
			r = y >= CELL_H / 2 ? ROW_ALL : 0;
			break;
		case CH_LEFT:
			r = 0x1F0;            /* left five dots */
			break;
		case CH_RIGHT:
			r = 0x00F;            /* right four dots */
			break;
		case CH_LIGHT:            /* about one dot in four */
			r = (y & 1) ? 0x044 : 0x111;
			break;
		case CH_MEDIUM:           /* checkerboard, one dot in two */
			r = (y & 1) ? 0x0AA : 0x155;
			break;
		case CH_DARK:             /* about three dots in four */
			r = ROW_ALL & ~((y & 1) ? 0x044 : 0x111);
			break;
		case CH_HLINE:
			r = y == 6 ? ROW_ALL : 0;
			break;
		case CH_VLINE:
			r = 0x010;
			break;
		case CH_TL:
			r = y == 6 ? 0x01F : (y > 6 ? 0x010 : 0);
			break;
		case CH_TR:
			r = y == 6 ? 0x1F0 : (y > 6 ? 0x010 : 0);
			break;
		case CH_BL:
			r = y == 6 ? 0x01F : (y < 6 ? 0x010 : 0);
			break;
		case CH_BR:
			r = y == 6 ? 0x1F0 : (y < 6 ? 0x010 : 0);
			break;
		default:
			if (ch >= 32 && ch <= 126)
				r = (uint16_t)font_ascii[ch - 32][y] << 1;
			break;
		}
		rows[y] = r;
	}
}
