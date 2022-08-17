/*
 *  ppui/osinterface/amiga/PPMutex.h
 *
 *  Copyright 2020 neoman
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

#ifndef PPMUTEX__H
#define PPMUTEX__H

#include <exec/semaphores.h>
#include <proto/exec.h>

#ifdef __amigaos4__
#define AllocMem IExec->AllocMem
#define FreeMem IExec->FreeMem
#define InitSemaphore IExec->InitSemaphore
#define ObtainSemaphore IExec->ObtainSemaphore
#define ReleaseSemaphore IExec->ReleaseSemaphore
#endif

class PPMutex
{
private:
	struct SignalSemaphore * semaphore;

public:
	PPMutex();
	~PPMutex();

	void lock();
	void unlock();
};

#endif /* PPMUTEX__H */
