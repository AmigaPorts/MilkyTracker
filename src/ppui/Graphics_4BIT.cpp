/*
 *  ppui/Graphics_4BIT.cpp
 *
 *  Copyright 2022 neoman/titan
 *
 *  This file is part of Milkytracker.
 *
 *  Milkytracker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Milkytracker is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Milkytracker.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "Graphics.h"
#include "Font.h"
#include "fastfill.h"

PPGraphics_4BIT::PPGraphics_4BIT(pp_int32 w, pp_int32 h, pp_int32 p, void* buff)
: PPGraphicsFrameBuffer(w, h, p, buff)
{
	for(int i = 0; i < 4096; i++) {
		paletteIndexCache[i] = -1;
	}
}

pp_uint8 PPGraphics_4BIT::lookupPaletteIndex(const PPColor& color)
{
	pp_int32 search = color.getRGB444();
	pp_int8 cachedIndex = paletteIndexCache[search];
	if(cachedIndex >= 0) {
		return cachedIndex;
	}
	if(cachedIndex == -2) {
		return 0;
	}

	for(int i = 0; i < 16; i++) {
		pp_int32 candidate = currentPalette[i].getRGB444();
		if(search == candidate) {
			paletteIndexCache[search] = i;
			printf("Caching %03x as %02d\n", search, i);
			return i;
		}
	}
	paletteIndexCache[search] = -2;
	printf("Not found: %03x\n", search);

	return 0;
}

void PPGraphics_4BIT::setPixel(pp_int32 x, pp_int32 y)
{
	if (y >= currentClipRect.y1 && y < currentClipRect.y2 &&
		x >= currentClipRect.x1 && x < currentClipRect.x2)
	{
		pp_uint8 * d = buffer + pitch * y + x;
		*d = currentColorIndex;
	}
}

void PPGraphics_4BIT::setPixel(pp_int32 x, pp_int32 y, const PPColor& color)
{
	if (y >= currentClipRect.y1 && y < currentClipRect.y2 &&
		x >= currentClipRect.x1 && x < currentClipRect.x2)
	{
		pp_uint8 * d = buffer + pitch * y + x;
		*d = lookupPaletteIndex(color);
	}
}

void PPGraphics_4BIT::setColor(pp_int32 r, pp_int32 g, pp_int32 b)
{
	currentColor.r = r;
	currentColor.g = g;
	currentColor.b = b;

	currentColorIndex = lookupPaletteIndex(currentColor);
}

void PPGraphics_4BIT::setColor(const PPColor& color)
{
	currentColor = color;

	currentColorIndex = lookupPaletteIndex(currentColor);
}

void PPGraphics_4BIT::setSafeColor(pp_int32 r, pp_int32 g, pp_int32 b)
{
	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;

	setColor(r, g, b);
}

void PPGraphics_4BIT::fill(PPRect rect)
{
	pp_int32 y, len;
	pp_uint8 * d;

	if (rect.y1 < currentClipRect.y1)
		rect.y1 = currentClipRect.y1;
	if (rect.x1 < currentClipRect.x1)
		rect.x1 = currentClipRect.x1;
	if (rect.y2 > currentClipRect.y2)
		rect.y2 = currentClipRect.y2;
	if (rect.x2 > currentClipRect.x2)
		rect.x2 = currentClipRect.x2;

	len = (rect.x2 - rect.x1);

	if (len <= 0)
		return;

	d = (pp_uint8 *) buffer + pitch * rect.y1 + rect.x1;

	// @todo optimize via fusing
	// @todo optimize via fullscreen fill

	for (y = rect.y1; y < rect.y2; y++)
	{
		memset(d, currentColorIndex, len);
		d += pitch;
	}
}

void PPGraphics_4BIT::fill()
{
	fill(currentClipRect);
}

void PPGraphics_4BIT::drawHLine(pp_int32 x1, pp_int32 x2, pp_int32 y)
{
	pp_int32 len;
	pp_uint8 * d;

	if (x1 > x2) {
		pp_int32 h = x2;
		x2 = x1;
		x1 = h;
	}

	if (x1 < currentClipRect.x1)
		x1 = currentClipRect.x1;
	if (x2 > currentClipRect.x2)
		x2 = currentClipRect.x2;
	if (y < currentClipRect.y1)
		return;
	if (y >= currentClipRect.y2)
		return;

	d = (pp_uint8 *) buffer + pitch * y + x1;
	len = x2 - x1;

	if (len <= 0)
		return;

	memset(d, currentColorIndex, len);
}

void PPGraphics_4BIT::drawVLine(pp_int32 y1, pp_int32 y2, pp_int32 x)
{
	pp_uint8 * d;

	if (y1 > y2) {
		pp_int32 h = y2;
		y2 = y1;
		y1 = h;
	}

	if (y1 < currentClipRect.y1)
		y1 = currentClipRect.y1;
	if (y2 > currentClipRect.y2)
		y2 = currentClipRect.y2;
	if (x < currentClipRect.x1)
		return;
	if (x >= currentClipRect.x2)
		return;

	d = (pp_uint8 *) buffer + pitch * y1 + x;

	for (pp_int32 y = y1; y < y2; y++) {
		*d = currentColorIndex;
		d += pitch;
	}
}

void PPGraphics_4BIT::drawLine(pp_int32 x1, pp_int32 y1, pp_int32 x2, pp_int32 y2)
{
	__PPGRAPHICSLINETEMPLATE
}

void PPGraphics_4BIT::drawAntialiasedLine(pp_int32 x1, pp_int32 y1, pp_int32 x2, pp_int32 y2)
{
	__PPGRAPHICSAALINETEMPLATE
}

void PPGraphics_4BIT::blit(const pp_uint8* src, const PPPoint& p, const PPSize& size, pp_uint32 pitch, pp_uint32 bpp, pp_int32 intensity/* = 256*/)
{
}

void PPGraphics_4BIT::drawChar(pp_uint8 chr, pp_int32 x, pp_int32 y, bool underlined)
{
	if (currentFont == NULL)
		return;

	pp_int32 charWidth = (signed)currentFont->getCharWidth();
	pp_int32 charHeight = (signed)currentFont->getCharHeight();
	pp_int32 charDim = currentFont->charDim;

	if (x + (signed)charWidth < currentClipRect.x1 ||
		x > currentClipRect.x2 ||
		y + (signed)charHeight < currentClipRect.y1 ||
		y > currentClipRect.y2)
		return;

	Bitstream* bitstream = currentFont->bitstream;
	pp_uint8* fontbuffer = bitstream->buffer;

	pp_uint8 * d = buffer + pitch * y + x;

	const pp_uint32 cchrDim = chr * charDim;
	const pp_uint32 incr = pitch - charWidth;

	if (x>= currentClipRect.x1 && x + charWidth < currentClipRect.x2 &&
		y>= currentClipRect.y1 && y + charHeight < currentClipRect.y2)
	{
		pp_uint32 yChr = cchrDim;
		for (pp_uint32 i = 0; i < (unsigned)charHeight; i++) {
			pp_uint32 xChr = yChr;
			for (pp_uint32 j = 0; j < (unsigned)charWidth; j++) {
				if ((fontbuffer[xChr>>3]>>(xChr&7)&1)) {
					*d = currentColorIndex;
				}
				d++;
				xChr++;
			}

			d+=incr;
			yChr+=charWidth;
		}
	}
	else
	{
		pp_uint32 yChr = cchrDim;
		for (pp_uint32 i = 0; i < (unsigned)charHeight; i++) {
			pp_uint32 xChr = yChr;
			for (pp_uint32 j = 0; j < (unsigned)charWidth; j++) {
				if (y+(signed)i >= currentClipRect.y1 && y+(signed)i < currentClipRect.y2 &&
					x+(signed)j >= currentClipRect.x1 && x+(signed)j < currentClipRect.x2 &&
					(fontbuffer[xChr>>3]>>(xChr&7)&1))
				{
					pp_uint8 * d = buffer + pitch * (y+i) + (x+j);
					*d = currentColorIndex;
				}

				xChr++;
			}
			yChr+=charWidth;
		}
	}

	if (underlined)
		drawHLine(x, x+charWidth, y+charHeight);

}

void PPGraphics_4BIT::drawString(const char* str, pp_int32 x, pp_int32 y, bool underlined/* = false*/)
{
	if (currentFont == NULL)
		return;

	pp_int32 charWidth = (signed)currentFont->getCharWidth();
	pp_int32 charHeight = (signed)currentFont->getCharHeight();

	pp_int32 sx = x;

    while (*str)
	{
		switch (*str)
		{
			case '\xf4':
				setPixel(x+(charWidth>>1), y+(charHeight>>1));
				break;
			case '\n':
				y+=charHeight;
				x=sx-charWidth;
				break;
			default:
				drawChar(*str, x, y, underlined);
		}
        x += charWidth;
        str++;
    }
}

void PPGraphics_4BIT::drawStringVertical(const char* str, pp_int32 x, pp_int32 y, bool underlined/* = false*/)
{
	if (currentFont == NULL)
		return;

	pp_int32 charWidth = (signed)currentFont->getCharWidth();
	pp_int32 charHeight = (signed)currentFont->getCharHeight();

    while (*str)
	{
		switch (*str)
		{
			case '\xf4':
				setPixel(x+(charWidth>>1), y+(charHeight>>1));
				break;
			default:
				drawChar(*str, x, y, underlined);
		}
        y += charHeight;
        str++;
    }
}

void PPGraphics_4BIT::fillVerticalShaded(PPRect r, const PPColor& colSrc, const PPColor& colDst, bool invertShading, const PPColor& colOriginal)
{
	// @todo invertShading
	setColor(colOriginal);
	PPRect old = getRect();
	setRect(r);
	fill();
	setRect(old);
}

void PPGraphics_4BIT::fillVerticalShaded(const PPColor& colSrc, const PPColor& colDst, bool invertShading, const PPColor& colOriginal)
{
	// @todo invertShading
	setColor(colOriginal);
	fill();
}
