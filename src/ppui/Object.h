/*
 *  ppui/Object.h
 *
 *  Copyright 2009 Peter Barth
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

#ifndef OBJECT__H
#define OBJECT__H

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
#include <exec/types.h>
#else
#define BOOL bool
#define TRUE true
#define FALSE false
#define ULONG unsigned long
#define Printf printf
#define Object void
#endif

class PPObject
{

};

#endif
