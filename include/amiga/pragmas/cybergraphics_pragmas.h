/*
**	$VER: cybergraphics_pragmas.h 41.18 (21.02.1998)
**
**	SAS/C format pragma files for cybergraphics.library
**
**	Copyright © 1996-1998 by phase5 digital products
**      All Rights reserved.
**
*/

#ifndef PRAGMAS_CYBERGRAPHICS_H
#define PRAGMAS_CYBERGRAPHICS_H 1

#pragma libcall CyberGfxBase IsCyberModeID 36 001
#pragma libcall CyberGfxBase BestCModeIDTagList 3c 801
#pragma libcall CyberGfxBase CModeRequestTagList 42 9802
#pragma libcall CyberGfxBase AllocCModeListTagList 48 901
#pragma libcall CyberGfxBase FreeCModeList 4e 801
#pragma libcall CyberGfxBase ScalePixelArray 5a 76543921080A
#pragma libcall CyberGfxBase GetCyberMapAttr 60 0802
#pragma libcall CyberGfxBase GetCyberIDAttr 66 1002
#pragma libcall CyberGfxBase ReadRGBPixel 6c 10903
#pragma libcall CyberGfxBase WriteRGBPixel 72 210904
#pragma libcall CyberGfxBase ReadPixelArray 78 76543921080A
#pragma libcall CyberGfxBase WritePixelArray 7e 76543921080A
#pragma libcall CyberGfxBase MovePixelArray 84 543291007
#pragma libcall CyberGfxBase InvertPixelArray 90 3210905
#pragma libcall CyberGfxBase FillPixelArray 96 43210906
#pragma libcall CyberGfxBase DoCDrawMethodTagList 9c A9803
#pragma libcall CyberGfxBase CVideoCtrlTagList a2 9802
#pragma libcall CyberGfxBase LockBitMapTagList a8 9802
#pragma libcall CyberGfxBase UnLockBitMap ae 801
#pragma libcall CyberGfxBase UnLockBitMapTagList b4 9802
#pragma libcall CyberGfxBase ExtractColor 432109807
#pragma libcall CyberGfxBase WriteLUTPixelArray 76543A921080B

#ifdef __SASC_60
#pragma tagcall CyberGfxBase BestCModeIDTags 3c 801
#pragma tagcall CyberGfxBase CModeRequestTags 42 9802
#pragma tagcall CyberGfxBase AllocCModeListTags 48 901
#pragma tagcall CyberGfxBase DoCDrawMethodTags 9c A9803
#pragma tagcall CyberGfxBase CVideoCtrlTags a2 9802
#pragma tagcall CyberGfxBase LockBitMapTags a8 9802
#pragma tagcall CyberGfxBase UnLockBitMapTagList b4 9802
#endif

#endif /* !PRAGMAS_CYBERGRAPHICS_H */
