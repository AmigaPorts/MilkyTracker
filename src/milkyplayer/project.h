#ifndef MILKYPLAYER_PROJECT_H
#define MILKYPLAYER_PROJECT_H 1

#ifdef __AMIGA__
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <libraries/iffparse.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster_lib.h>

#include <clib/alib_protos.h>
#include <utility/tagitem.h>
#else
#define BOOL bool
#define TRUE true
#define FALSE false
#define ULONG unsigned long
#define Printf printf
#endif

#define MILKYTRACKER 1
#include <SDL.h>
#include <XModule.h>
#include "AslRequester.h"
#include "amigaversion.h"
#include "PlayerMaster.h"
#include "PlayerController.h"

#ifdef __AMIGA__
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))

#define IPTR ULONG

#endif

BOOL Open_Libs();

void Close_Libs();

#endif // MILKYPLAYER_PROJECT_H