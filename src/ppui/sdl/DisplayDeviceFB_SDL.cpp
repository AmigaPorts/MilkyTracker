/*
 *  ppui/sdl/DisplayDeviceFB_SDL.cpp
 *
 *  Copyright 2009 Peter Barth, Christopher O'Neill, Dale Whinham
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
 *  12/5/14 - Dale Whinham
 *    - Port to SDL2
 *    - Resizable window which renders to a scaled texture
 *    - Experimental, buggy Retina support (potential problems with mouse coordinates if letterboxing happens)
 *
 *    TODO: - Test under Linux (only tested under OSX)
 *          - Test/fix/remove scale factor and orientation code
 *          - Look at the OpenGL stuff
 */

#include "DisplayDeviceFB_SDL.h"
#include "Graphics.h"

#include <unistd.h>

#if defined(AMIGA_SAGA_PIP)
#	include <exec/exec.h>
#	include <intuition/intuitionbase.h>
#	include <vampire/saga.h>
#	include <vampire/vampire.h>

#	include <proto/exec.h>
#	include <proto/intuition.h>
#	include <proto/vampire.h>

#	include <hardware/custom.h>
#	include <hardware/dmabits.h>
#	include <hardware/intbits.h>
#	include <hardware/cia.h>

extern struct ExecBase *SysBase;
extern struct IntuitionBase* IntuitionBase;

struct Library* VampireBase = NULL;
#endif

PPDisplayDeviceFB::PPDisplayDeviceFB(
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface*& screen,
#endif
	pp_int32 width,
	pp_int32 height,
	pp_int32 scaleFactor,
	pp_int32 bpp,
	bool fullScreen,
	Orientations theOrientation/* = ORIENTATION_NORMAL*/,
	bool swapRedBlue/* = false*/) :
	PPDisplayDevice(
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	screen,
#endif
	width, height, scaleFactor, bpp, fullScreen, theOrientation),
	needsTemporaryBuffer((orientation != ORIENTATION_NORMAL) || (scaleFactor != 1)),
	temporaryBuffer(NULL)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Create an SDL window and surface
	theWindow = CreateWindow(realWidth, realHeight, bpp,
#ifdef HIDPI_SUPPORT
							  SDL_WINDOW_ALLOW_HIGHDPI |							// Support for 'Retina'/Hi-DPI displays
#endif
							  SDL_WINDOW_RESIZABLE	 |								// MilkyTracker's window is resizable
							  (bFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));	// Use 'fake fullscreen' because we can scale

	if (theWindow == NULL)
	{
		fprintf(stderr, "SDL: Could not create window.\n");
		exit(EXIT_FAILURE);
	}

	// Create renderer for the window
	theRenderer = SDL_CreateRenderer(theWindow, drv_index, 0);
	if (theRenderer == NULL)
	{
		fprintf(stderr, "SDL: SDL_CreateRenderer failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

#ifdef HIDPI_SUPPORT
	// Feed SDL_RenderSetLogicalSize() with output size, not GUI surface size, otherwise mouse coordinates will be wrong for Hi-DPI
	int rendererW, rendererH;
	SDL_GetRendererOutputSize(theRenderer, &rendererW, &rendererH);
#endif

	// Log renderer capabilities
	SDL_RendererInfo theRendererInfo;
	if (!SDL_GetRendererInfo(theRenderer, &theRendererInfo))
	{
		if (theRendererInfo.flags & SDL_RENDERER_SOFTWARE) printf("SDL: Using software renderer.\n");
		if (theRendererInfo.flags & SDL_RENDERER_ACCELERATED) printf("SDL: Using accelerated renderer.\n");
		if (theRendererInfo.flags & SDL_RENDERER_PRESENTVSYNC) printf("SDL: Vsync enabled.\n");
		if (theRendererInfo.flags & SDL_RENDERER_TARGETTEXTURE) printf("SDL: Renderer supports rendering to texture.\n");
	}

	// Lock aspect ratio and scale the UI up to fit the window
#ifdef HIDPI_SUPPORT
	SDL_RenderSetLogicalSize(theRenderer, rendererW, rendererH);
#else
	SDL_RenderSetLogicalSize(theRenderer, realWidth, realHeight);
#endif

	// Use linear filtering for the scaling (make this optional eventually)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	// Create surface for rendering graphics
	theSurface = SDL_CreateRGBSurface(0, realWidth, realHeight, bpp == -1 ? 32 : bpp, 0, 0, 0, 0);
	if (theSurface == NULL)
	{
		fprintf(stderr, "SDL: SDL_CreateSurface failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Streaming texture for rendering the UI
	theTexture = SDL_CreateTexture(theRenderer, theSurface->format->format, SDL_TEXTUREACCESS_STREAMING, realWidth, realHeight);
	if (theTexture == NULL)
	{
		fprintf(stderr, "SDL: SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// We got a surface: update bpp value
	bpp = theSurface->format->BitsPerPixel;
#else
	const SDL_VideoInfo* videoinfo;

	/* Some SDL to get display format */
	videoinfo = SDL_GetVideoInfo();
	if (bpp == -1) {
		bpp = videoinfo->vfmt->BitsPerPixel > 15 ? videoinfo->vfmt->BitsPerPixel : 15;
	}
	this->bpp = bpp;

	/* Set a video mode */
	theSurface = screen = CreateScreen(realWidth, realHeight, bpp,
		SDL_SWSURFACE | (bFullScreen==true ? SDL_FULLSCREEN : 0));
	if ( screen == NULL )
	{
		fprintf(stderr, "Could not set video mode: %s\n", SDL_GetError());
		exit(2);
	}

	// Now we know the final BPP, init SAGA PiP
#	if defined(AMIGA_SAGA_PIP)
	if(bpp == 16) {
		struct Screen* screen = NULL;

		if (!(SysBase->AttnFlags & (1 << 10))) {
			fprintf(stderr, "No AC68080 processor!");
			exit(1);
		}

		if (!(VampireBase = OpenResource(V_VAMPIRENAME))) {
			fprintf(stderr, "Could not find vampire.resource!\n");
			exit(2);
		}

		if (VampireBase->lib_Version < 45) {
			fprintf(stderr, "Vampire.resource version needs to be 45 or higher!\n");
			exit(3);
		}

		if (V_EnableAMMX(V_AMMX_V2) == VRES_ERROR) {
			fprintf(stderr, "Cannot enable AMMX V2+!\n");
			exit(4);
		}

		if (screen = LockPubScreen(NULL)) {
			APTR allocator;
			pp_uint32 i;

			if (allocator = V_AllocExpansionPort(V_PIP, "MilkyTracker")) {
				fprintf(stderr, "SAGA PiP already in use! (by: %s)\n", allocator);
				exit(6);
			}
			fprintf(stdout, "Initialized SAGA PiP with %ld bpp\n", bpp);

			pubScreen = screen;

			// Allocate screen buffers for SAGA PiP pages
			for(i = 0; i < SAGA_PAGES; i++) {
				void * b = (void *) malloc((theSurface->pitch * getSize().height * 2) + 16);
				if(!b) {
					fprintf(stderr, "Could not allocate enough memory for SAGA buffer %ld\n", i);
					exit(8);
				}

				unalignedSAGABuffers[i] = b;

				// And align
				b = (void *) (((pp_uint32) b + 15) & ~ (pp_uint32) 0xf);
				memset(b, 0, theSurface->pitch * getSize().height * 2);
				alignedSAGABuffers[i] = b;
			}

			// Initialize PIP output
			currentSAGAPage = 0;
			WRITE16(SAGA_PIP_COLORKEY, 0);
			WRITE16(SAGA_PIP_PIXFMT, SAGAF_RGB16);
			WRITE32(SAGA_PIP_BPLPTR, (ULONG) alignedSAGABuffers[SAGA_PAGES - 1]);

			UnlockPubScreen(NULL, screen);
		}
	} else {
		fprintf(stderr, "Could not initialize SAGA PiP: Only 16 bpp are supported\n");
		exit(7);
	}
#	endif

#endif

	// Create a PPGraphics context based on bpp
	switch (bpp)
	{
		case 8:
			currentGraphics = new PPGraphics_8BIT(width, height, 0, NULL);
			break;
		case 15:
			currentGraphics = new PPGraphics_15BIT(width, height, 0, NULL);
			break;
		case 16:
			currentGraphics = new PPGraphics_16BIT(width, height, 0, NULL);
			break;
		case 24:
		{
			PPGraphics_24bpp_generic* g = new PPGraphics_24bpp_generic(width, height, 0, NULL);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			if (swapRedBlue)
			{
				g->setComponentBitpositions(theSurface->format->Bshift,
											theSurface->format->Gshift,
											theSurface->format->Rshift);
			}
			else
			{
				g->setComponentBitpositions(theSurface->format->Rshift,
											theSurface->format->Gshift,
											theSurface->format->Bshift);
			}
#else
			if (swapRedBlue)
			{
				g->setComponentBitpositions(videoinfo->vfmt->Bshift,
											videoinfo->vfmt->Gshift,
											videoinfo->vfmt->Rshift);
			}
			else
			{
				g->setComponentBitpositions(videoinfo->vfmt->Rshift,
											videoinfo->vfmt->Gshift,
											videoinfo->vfmt->Bshift);
			}
#endif
			currentGraphics = static_cast<PPGraphicsAbstract*>(g);
			break;
		}

		case 32:
		{
			PPGraphics_32bpp_generic* g = new PPGraphics_32bpp_generic(width, height, 0, NULL);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			if (swapRedBlue)
			{
				g->setComponentBitpositions(theSurface->format->Bshift,
											theSurface->format->Gshift,
											theSurface->format->Rshift);
			}
			else
			{
				g->setComponentBitpositions(theSurface->format->Rshift,
											theSurface->format->Gshift,
											theSurface->format->Bshift);
			}
#else
			if (swapRedBlue)
			{
				g->setComponentBitpositions(videoinfo->vfmt->Bshift,
											videoinfo->vfmt->Gshift,
											videoinfo->vfmt->Rshift);
			}
			else
			{
				g->setComponentBitpositions(videoinfo->vfmt->Rshift,
											videoinfo->vfmt->Gshift,
											videoinfo->vfmt->Bshift);
			}
#endif
			currentGraphics = static_cast<PPGraphicsAbstract*>(g);
			break;
		}

		default:
			fprintf(stderr, "SDL: Unsupported color depth (%i), try either 16, 24 or 32", bpp);
			exit(EXIT_FAILURE);
	}

	if (needsTemporaryBuffer)
	{
		temporaryBufferPitch = (width*bpp)/8;
		temporaryBufferBPP = bpp;
		temporaryBuffer = new pp_uint8[getSize().width*getSize().height*(bpp/8)];
	}

	currentGraphics->lock = true;
}

PPDisplayDeviceFB::~PPDisplayDeviceFB()
{
	pp_uint32 i;

	SDL_FreeSurface(theSurface);

#if defined(AMIGA_SAGA_PIP)
	WRITE16(SAGA_PIP_PIXFMT,   0); // DMA OFF
	WRITE16(SAGA_PIP_X0,       0);
	WRITE16(SAGA_PIP_Y0,       0);
	WRITE16(SAGA_PIP_X1,       0);
	WRITE16(SAGA_PIP_Y1,       0);
	WRITE16(SAGA_PIP_COLORKEY, 0);
	WRITE32(SAGA_PIP_BPLPTR,   0);

	V_FreeExpansionPort(V_PIP);

	for(i = 0; i < SAGA_PAGES; i++) {
		free(unalignedSAGABuffers[i]);
	}
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyRenderer(theRenderer);
	SDL_DestroyWindow(theWindow);
#endif

	delete[] temporaryBuffer;
	// base class is responsible for deleting currentGraphics
}

#if defined(AMIGA_SAGA_PIP)
struct Window * SDL_AmigaWindowAddr(void);

void PPDisplayDeviceFB::setSAGAPiPSize()
{
	ULONG x0 = 0, y0 = 0;
	ULONG x1 = 0, y1 = 0;

	struct Window * window = SDL_AmigaWindowAddr();

	if (window && pubScreen == IntuitionBase->FirstScreen) {
		x0 = SAGA_PIP_DELTAX + window->LeftEdge + pubScreen->LeftEdge + 2;
		y0 = SAGA_PIP_DELTAY + window->BorderTop + window->TopEdge + pubScreen->TopEdge + 0;

		if ((x0 + window->GZZWidth - 16 - 64) < pubScreen->Width) {
			x1 = x0 + window->GZZWidth;
			y1 = y0 + window->GZZHeight;
		} else {
			x0 = 0;
			y0 = 0;
		}
	}

	WRITE16(SAGA_PIP_X0, x0);
	WRITE16(SAGA_PIP_Y0, y0);
	WRITE16(SAGA_PIP_X1, x1);
	WRITE16(SAGA_PIP_Y1, y1);

	// Only do that if we're not writing
	if(currentGraphics->lock) {
		// Display new page
		WRITE32(SAGA_PIP_BPLPTR, (ULONG) alignedSAGABuffers[currentSAGAPage]);

		// Select new page
		pp_uint32 nextSAGAPage = currentSAGAPage + 1;
		if(nextSAGAPage == SAGA_PAGES)
			nextSAGAPage = 0;

		// And copy over data
		memcpy(alignedSAGABuffers[nextSAGAPage], alignedSAGABuffers[currentSAGAPage], theSurface->pitch * getSize().height * 2);
		currentSAGAPage = nextSAGAPage;
	}
}
#endif

PPGraphicsAbstract* PPDisplayDeviceFB::open()
{
	if (!isEnabled())
		return NULL;

	if (currentGraphics->lock)
	{
		if (SDL_LockSurface(theSurface) < 0)
			return NULL;

		currentGraphics->lock = false;

		if (needsTemporaryBuffer) {
			static_cast<PPGraphicsFrameBuffer*>(currentGraphics)->setBufferProperties(temporaryBufferPitch, (pp_uint8*)temporaryBuffer);
		} else {
#if defined(AMIGA_SAGA_PIP)
			static_cast<PPGraphicsFrameBuffer*>(currentGraphics)->setBufferProperties(theSurface->pitch, (pp_uint8 *) alignedSAGABuffers[currentSAGAPage]);
#else
			static_cast<PPGraphicsFrameBuffer*>(currentGraphics)->setBufferProperties(theSurface->pitch, (pp_uint8 *)theSurface->pixels);
#endif
		}

		return currentGraphics;
	}

	return NULL;
}

void PPDisplayDeviceFB::close()
{
	SDL_UnlockSurface(theSurface);

	currentGraphics->lock = true;
}

void PPDisplayDeviceFB::setPalette(PPColor * pppal)
{
	int i;

	if(!currentGraphics->needsPalette())
		return;

	// Pass palette to graphics context
	currentGraphics->setPalette(pppal);

	// Pass palette to SDL
	for(i = 0; i < 256; i++) {
		palette[i].r = pppal[i].r;
		palette[i].g = pppal[i].g;
		palette[i].b = pppal[i].b;
	}

	SDL_SetColors(theSurface, palette, 0, 256);
}

void PPDisplayDeviceFB::update()
{
#if defined(AMIGA_SAGA_PIP)
	return;
#endif

	if (!isUpdateAllowed() || !isEnabled())
		return;

	if (theSurface->locked)
		return;

	PPRect r(0, 0, getSize().width, getSize().height);
	postProcess(r);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Update entire texture and copy to renderer
	SDL_UpdateTexture(theTexture, NULL, theSurface->pixels, theSurface->pitch);
	SDL_RenderClear(theRenderer);
	SDL_RenderCopy(theRenderer, theTexture, NULL, NULL);
	SDL_RenderPresent(theRenderer);
#else
	SDL_UpdateRect(theSurface, 0, 0, 0, 0);
#endif
}

void PPDisplayDeviceFB::update(const PPRect& r)
{
#if defined(AMIGA_SAGA_PIP)
	return;
#endif

	if (!isUpdateAllowed() || !isEnabled())
		return;

	if (theSurface->locked)
		return;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	postProcess(r);

	PPRect r2(r);
	r2.scale(scaleFactor);

	transformInverse(r2);

	SDL_Rect r3 = { r2.x1, r2.y1, r2.width(), r2.height() };

	// Calculate destination pixel data offset based on row pitch and x coordinate
	void* surfaceOffset = (char*) theSurface->pixels + r2.y1 * theSurface->pitch + r2.x1 * theSurface->format->BytesPerPixel;

	// Update dirty area of texture and copy to renderer
	SDL_UpdateTexture(theTexture, &r3, surfaceOffset, theSurface->pitch);
	SDL_RenderClear(theRenderer);
	SDL_RenderCopy(theRenderer, theTexture, NULL, NULL);
	SDL_RenderPresent(theRenderer);
#else
	PPRect r2(r);
	postProcess(r2);

	PPRect r3(r);
	r3.scale(scaleFactor);

	transformInverse(r3);

	SDL_UpdateRect(theSurface, r3.x1, r3.y1, (r3.x2-r3.x1), (r3.y2-r3.y1));
#endif
}

void PPDisplayDeviceFB::postProcess(const PPRect& r2)
{
	PPRect r(r2);
	pp_int32 h;
	if (r.x2 < r.x1)
	{
		h = r.x1; r.x1 = r.x2; r.x2 = h;
	}
	if (r.y2 < r.y1)
	{
		h = r.y1; r.y1 = r.y2; r.y2 = h;
	}

	switch (orientation)
	{
		case ORIENTATION_NORMAL:
		{
			if (!needsTemporaryBuffer)
				return;

			if (SDL_LockSurface(theSurface) < 0)
				return;

			const pp_uint32 srcBPP = temporaryBufferBPP/8;
			const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

			PPRect destRect(r);
			destRect.scale(scaleFactor);

			const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
			const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

			switch (temporaryBufferBPP)
			{
				case 16:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					pp_uint32 v = r.y1 * 65536;
					for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
					{
						pp_uint32 u = r.x1 * 65536;
						pp_uint16* dstPtr = (pp_uint16*)(dst + y*dstPitch + destRect.x1*dstBPP);
						pp_uint8* srcPtr = src + (v>>16)*srcPitch;
						for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
						{
							*dstPtr++ = *(pp_uint16*)(srcPtr + (u>>16) * srcBPP);
							u += stepU;
						}
						v += stepV;
					}


					break;
				}

				case 24:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					pp_uint32 v = r.y1 * 65536;
					for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
					{
						pp_uint32 u = r.x1 * 65536;
						pp_uint8* dstPtr = (pp_uint8*)(dst + y*dstPitch + destRect.x1*dstBPP);
						pp_uint8* srcPtr = src + (v>>16)*srcPitch;
						for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
						{
							*dstPtr = *(pp_uint8*)(srcPtr + (u>>16) * srcBPP);
							*(dstPtr+1) = *(pp_uint8*)(srcPtr + (u>>16) * srcBPP + 1);
							*(dstPtr+2) = *(pp_uint8*)(srcPtr + (u>>16) * srcBPP + 2);
							dstPtr+=3;
							u += stepU;
						}
						v += stepV;
					}

					break;
				}

				case 32:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					pp_uint32 v = r.y1 * 65536;
					for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
					{
						pp_uint32 u = r.x1 * 65536;
						pp_uint32* dstPtr = (pp_uint32*)(dst + y*dstPitch + destRect.x1*dstBPP);
						pp_uint8* srcPtr = src + (v>>16)*srcPitch;
						for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
						{
							*dstPtr++ = *(pp_uint32*)(srcPtr + (u>>16) * srcBPP);
							u += stepU;
						}
						v += stepV;
					}

					break;
				}

				default:
					fprintf(stderr, "SDL: Unsupported color depth for requested orientation");
					exit(2);
			}

			SDL_UnlockSurface(theSurface);

			break;
		}

		case ORIENTATION_ROTATE90CCW:
		{
			if (SDL_LockSurface(theSurface) < 0)
				return;

			switch (temporaryBufferBPP)
			{
				case 16:
				{
					pp_uint32 srcPitch = temporaryBufferPitch >> 1;
					pp_uint32 dstPitch = theSurface->pitch >> 1;

					pp_uint16* src = (pp_uint16*)temporaryBuffer;
					pp_uint16* dst = (pp_uint16*)theSurface->pixels;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint16* srcPtr = src + (v>>16)*srcPitch;
							pp_uint16* dstPtr = dst + y + (realHeight-destRect.x1)*dstPitch;
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								*(dstPtr-=dstPitch) = *(srcPtr+(u>>16));

								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint16* srcPtr = src + y*srcPitch + r.x1;
							pp_uint16* dstPtr = dst + y + (realHeight-r.x1)*dstPitch;
							for (pp_uint32 x = r.x1; x < r.x2; x++)
								*(dstPtr-=dstPitch) = *srcPtr++;
						}
					}

					break;
				}

				case 24:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint8* srcPtr = src + (v>>16)*srcPitch;
							pp_uint8* dstPtr = dst + y*dstBPP + dstPitch*(realHeight-1-destRect.x1);
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								dstPtr[0] = *(srcPtr+(u>>16) * srcBPP);
								dstPtr[1] = *(srcPtr+(u>>16) * srcBPP + 1);
								dstPtr[2] = *(srcPtr+(u>>16) * srcBPP + 2);
								dstPtr-=dstPitch;

								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint8* srcPtr = src + y*srcPitch + r.x1*srcBPP;
							pp_uint8* dstPtr = dst + y*dstBPP + dstPitch*(realHeight-1-r.x1);
							for (pp_uint32 x = r.x1; x < r.x2; x++)
							{
								dstPtr[0] = srcPtr[0];
								dstPtr[1] = srcPtr[1];
								dstPtr[2] = srcPtr[2];
								srcPtr+=srcBPP;
								dstPtr-=dstPitch;
							}
						}
					}

					break;
				}

				case 32:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint8* srcPtr = src + (v>>16)*srcPitch;
							pp_uint32* dstPtr = (pp_uint32*)(dst + y*dstBPP + dstPitch*(realHeight-1-destRect.x1));
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								*(dstPtr-=(dstPitch>>2)) = *(pp_uint32*)(srcPtr + (u>>16) * srcBPP);
								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint32* srcPtr = (pp_uint32*)(src + y*srcPitch + r.x1*srcBPP);
							pp_uint32* dstPtr = (pp_uint32*)(dst + y*dstBPP + dstPitch*(realHeight-1-r.x1));
							for (pp_uint32 x = r.x1; x < r.x2; x++)
								*(dstPtr-=(dstPitch>>2)) = *srcPtr++;
						}
					}

					break;
				}

				default:
					fprintf(stderr, "SDL: Unsupported color depth for requested orientation");
					exit(2);
			}

			SDL_UnlockSurface(theSurface);
			break;
		}

		case ORIENTATION_ROTATE90CW:
		{
			if (SDL_LockSurface(theSurface) < 0)
				return;

			switch (temporaryBufferBPP)
			{
				case 16:
				{
					pp_uint32 srcPitch = temporaryBufferPitch >> 1;
					pp_uint32 dstPitch = theSurface->pitch >> 1;

					pp_uint16* src = (pp_uint16*)temporaryBuffer;
					pp_uint16* dst = (pp_uint16*)theSurface->pixels;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint16* srcPtr = src + (v>>16)*srcPitch;
							pp_uint16* dstPtr = dst + (realWidth-1-y) + (dstPitch*(destRect.x1));
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								*(dstPtr+=dstPitch) = *(srcPtr+(u>>16));

								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint16* srcPtr = src + y*srcPitch + r.x1;
							pp_uint16* dstPtr = dst + (realWidth-1-y) + (dstPitch*r.x1);
							for (pp_uint32 x = r.x1; x < r.x2; x++)
								*(dstPtr+=dstPitch) = *srcPtr++;
						}
					}

					break;
				}

				case 24:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint8* srcPtr = src + (v>>16)*srcPitch;
							pp_uint8* dstPtr = dst + (realWidth-1-y)*dstBPP + (dstPitch*(destRect.x1));
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								dstPtr[0] = *(srcPtr+(u>>16) * srcBPP);
								dstPtr[1] = *(srcPtr+(u>>16) * srcBPP + 1);
								dstPtr[2] = *(srcPtr+(u>>16) * srcBPP + 2);
								dstPtr+=dstPitch;

								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint8* srcPtr = src + y*srcPitch + r.x1*srcBPP;
							pp_uint8* dstPtr = dst + (realWidth-1-y)*dstBPP + (dstPitch*r.x1);
							for (pp_uint32 x = r.x1; x < r.x2; x++)
							{
								dstPtr[0] = srcPtr[0];
								dstPtr[1] = srcPtr[1];
								dstPtr[2] = srcPtr[2];
								srcPtr+=srcBPP;
								dstPtr+=dstPitch;
							}
						}
					}

					break;
				}

				case 32:
				{
					pp_uint32 srcPitch = temporaryBufferPitch;
					pp_uint32 dstPitch = theSurface->pitch;

					pp_uint8* src = (pp_uint8*)temporaryBuffer;
					pp_uint8* dst = (pp_uint8*)theSurface->pixels;

					const pp_uint32 srcBPP = temporaryBufferBPP/8;
					const pp_uint32 dstBPP = theSurface->format->BytesPerPixel;

					if (scaleFactor != 1)
					{
						PPRect destRect(r);
						destRect.scale(scaleFactor);

						const pp_uint32 stepU = (r.x2 - r.x1) * 65536 / (destRect.x2 - destRect.x1);
						const pp_uint32 stepV = (r.y2 - r.y1) * 65536 / (destRect.y2 - destRect.y1);

						pp_uint32 v = r.y1 * 65536;
						for (pp_uint32 y = destRect.y1; y < destRect.y2; y++)
						{
							pp_uint32 u = r.x1 * 65536;
							pp_uint8* srcPtr = src + (v>>16)*srcPitch;
							pp_uint32* dstPtr = (pp_uint32*)(dst + (realWidth-1-y)*dstBPP + (dstPitch*(destRect.x1)));
							for (pp_uint32 x = destRect.x1; x < destRect.x2; x++)
							{
								*(dstPtr+=(dstPitch>>2)) = *(pp_uint32*)(srcPtr + (u>>16) * srcBPP);
								u += stepU;
							}
							v += stepV;
						}
					}
					else
					{
						for (pp_uint32 y = r.y1; y < r.y2; y++)
						{
							pp_uint32* srcPtr = (pp_uint32*)(src + y*srcPitch + r.x1*srcBPP);
							pp_uint32* dstPtr = (pp_uint32*)(dst + (realWidth-1-y)*dstBPP + (dstPitch*r.x1));
							for (pp_uint32 x = r.x1; x < r.x2; x++)
								*(dstPtr+=(dstPitch>>2)) = *srcPtr++;
						}
					}

					break;
				}

				default:
					fprintf(stderr, "SDL: Unsupported color depth for requested orientation");
					exit(EXIT_FAILURE);
			}

			SDL_UnlockSurface(theSurface);
			break;
		}
	}

}
#if SDL_VERSION_ATLEAST(2, 0, 0)
// This is unused at the moment, could be useful if we manage to get the GUI resizable in the future.
void PPDisplayDeviceFB::setSize(const PPSize& size)
{
	this->size = size;
	theSurface = SDL_CreateRGBSurface(0, size.width, size.height, theSurface->format->BitsPerPixel, 0, 0, 0, 0);
	theTexture = SDL_CreateTextureFromSurface(theRenderer, theSurface);
	theRenderer = SDL_GetRenderer(theWindow);
}
#endif
