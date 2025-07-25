
/* *  tracker/amiga/CGX_Main.cpp
 *
 *  Copyright 2017 Marlon Beijer
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

/*
 *  CGX_Main.cpp
 *  MilkyTracker AGA/RTG Amiga front end
 *
 *  Created by Marlon Beijer on 17.09.17.
 *  Rewritten by neoman as full AGA/RTG port without SDL in 2020
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#if defined(__AMIGA__) || defined(__amigaos4__)
#	ifndef AFB_68080
#		define AFB_68080 10
#	endif
#	ifndef AFF_68080
#		define AFF_68080 (1<<AFB_68080)
#	endif
#	if defined(WARPOS) && !defined(AFF68060)
#		define AFF_68060 1
#	endif
#endif
#if defined(__AMIGA__) || defined(WARPUP) || defined(__WARPOS__) || defined(AROS) || defined(__amigaos4__) || defined(__morphos__)
#	include "amigaversion.h"
#endif

#include "timestamp.h"
#include "AmigaApplication.h"
#include "Log.h"
#include "PPUI.h"
#include "XMFile.h"
#include "TrackerSettingsDatabase.h"
#include "PPSystem_Amiga.h"

#define MAX_DISPLAY_MODES 		128

#define GID_BASE				10000
#define GID_SCREEN_MODE 		(GID_BASE + 1)
#define GID_AUDIO_DRV			(GID_BASE + 2)
#define GID_AUDIO_DRV_DESC		(GID_BASE + 3)
#define GID_AUDIO_MIXER			(GID_BASE + 4)
#define GID_AUDIO_MIXER_DESC	(GID_BASE + 5)
#define GID_RUN					(GID_BASE + 6)
#define GID_QUIT				(GID_BASE + 7)
#define GID_DETECTED			(GID_BASE + 8)

#define STR2(a) #a
#define STR(a) STR2(a)

extern struct ExecBase * SysBase;

struct Library * IntuitionBase = NULL;
struct Library * KeymapBase = NULL;
struct Library * GfxBase = NULL;
struct Library * P96Base = NULL;
struct Library * CyberGfxBase = NULL;
struct Library * VampireBase = NULL;
struct Library * IconBase = NULL;

static int cpuType = 0;
static bool hasFPU = false;
static bool hasAMMX = false;
static bool useSAGA = false;
static bool isV4Core = false;
static BPTR programDirLock = 0;

struct Device * TimerBase = NULL;
static struct IORequest timereq;
static struct timeval startTime;

AmigaApplication * app = NULL;

static AmigaApplication::AudioDriver drivers[] = {
	AmigaApplication::Paula,
	AmigaApplication::Arne
};

static const char *driverNames[] = {
	"Paula",
	NULL,
	NULL
};

static const char *driverDescs[] = {
	"4-ch/8-bit",
	NULL,
	NULL
};

static AmigaApplication::AudioMixer mixTypes[] = {
	AmigaApplication::ResampleHW,
	AmigaApplication::DirectOut,
	AmigaApplication::MixDown
};

static const char *mixTypeNames[] = {
	"ResampleHW",
	"DirectOut",
	"MixDown",
	NULL
};

static const char *mixTypeDescs[] = {
	"Fastest with DMA Resampler",
	"Slow with CPU Resampler",
	"Slowest with CPU Resampler",
	NULL
};

static LONG displayCount = -1;
static LONG * displayModeIDs = NULL;
static PPSize * displayModeSizes = NULL;
static char ** displayModeNames = NULL;
static struct List * displayModeLabels; 
static UWORD * displayModeDepths = NULL;
static AudioDriverInterface * audioDriver = NULL;

static struct {
	long setup;
} args;

APTR AllocSample(ULONG size) {
	if(app && app->isSAGA() && app->isV4()) {
		return AllocVec(size, MEMF_FAST | MEMF_CLEAR);
	}
	return AllocVec(size, MEMF_CHIP | MEMF_CLEAR);
}

void FreeSample(APTR mem) {
	FreeVec(mem);
}

bool ForceLogPeriod() {
	if(!app)
		return false;
	return app->getAudioMixer() == AmigaApplication::ResampleHW;
}

AudioDriverInterface * CreateAudioDriver() {
	if(!app)
		return NULL;

	return app->createAudioDriver();
}

int GetAudioDriverResolution() {
	if(!app)
		return 8;

	return (app->isSAGA() && app->isV4()) ? 16 : 8;
}

BPTR GetProgramDirLock() {
    return programDirLock;
}

void PrintStackSize() {
	struct Task * task = FindTask(NULL);
	ULONG currentstack = (ULONG) task->tc_SPUpper - (ULONG) task->tc_SPLower;
	INFO("Current stack: %lu", currentstack);
}

void QueryKeyModifiers() {
	if(!app)
		return;

	if(app->isShiftPressed())
		setKeyModifier(KeyModifierSHIFT);
	else
		clearKeyModifier(KeyModifierSHIFT);

	if(app->isCtrlPressed())
		setKeyModifier(KeyModifierCTRL);
	else
		clearKeyModifier(KeyModifierCTRL);

	if(app->isAltPressed())
		setKeyModifier(KeyModifierALT);
	else
		clearKeyModifier(KeyModifierALT);
}

pp_uint32 PPGetTickCount() {
    struct timeval endTime;

	if(!TimerBase) {
		return 0;
	}

	GetSysTime(&endTime);
    SubTime(&endTime, &startTime);

    return endTime.tv_secs * 1000 + endTime.tv_micro / 1000;
}

bool QueryClassicBrowser(bool currentSetting) {
	// For SAGA PIP and fullscreen mode, always use the classic browser on Amiga
	if(app && ((app->isSAGA() && app->isV4()) || app->isFullScreen())) {
		return true;
	}
	return currentSetting;
}

static void loadBootConfig(AmigaApplication * app)
{
	TrackerSettingsDatabase * settingsDatabase = new TrackerSettingsDatabase();

	if (XMFile::exists(System::getConfigFileName())) {
		XMFile f(System::getConfigFileName());
		settingsDatabase->serialize(f);
	}

	if(settingsDatabase->hasKey("AMIGA_DISPLAY_ID")) {
		app->setDisplayID(settingsDatabase->restore("AMIGA_DISPLAY_ID")->getIntValue());
	} else {
		app->setDisplayID(-255);
		args.setup = 1;
	}

	if(settingsDatabase->hasKey("AMIGA_AUDIO_DRIVER")) {
		app->setAudioDriver(static_cast<AmigaApplication::AudioDriver>(settingsDatabase->restore("AMIGA_AUDIO_DRIVER")->getIntValue()));
	} else {
		app->setAudioDriver((cpuType == 68080) ? AmigaApplication::Arne : AmigaApplication::Paula);
	}

	if(settingsDatabase->hasKey("AMIGA_AUDIO_MIXER")) {
		app->setAudioMixer(static_cast<AmigaApplication::AudioMixer>(settingsDatabase->restore("AMIGA_AUDIO_MIXER")->getIntValue()));
	} else {
		app->setAudioMixer(AmigaApplication::ResampleHW);
	}

	delete settingsDatabase;
}

static void saveBootConfig(AmigaApplication * app)
{
	TrackerSettingsDatabase * settingsDatabase = new TrackerSettingsDatabase();
	XMFile f(System::getConfigFileName(), true);

	settingsDatabase->store("AMIGA_DISPLAY_ID", app->getDisplayID());
	settingsDatabase->store("AMIGA_AUDIO_DRIVER", app->getAudioDriver());
	settingsDatabase->store("AMIGA_AUDIO_MIXER", app->getAudioMixer());

	settingsDatabase->serialize(f);

	delete settingsDatabase;
}

static bool checkHardware()
{
	cpuType = 0;
	hasFPU = true;
	hasAMMX = false;

	if ((SysBase->AttnFlags & AFF_68080) != 0)
		cpuType = 68080;
	else if ((SysBase->AttnFlags & AFF_68060) != 0)
		cpuType = 68060;
	else if ((SysBase->AttnFlags & AFF_68040) != 0)
		cpuType = 68040;
	else if ((SysBase->AttnFlags & AFF_68030) != 0)
		cpuType = 68030;
	else if ((SysBase->AttnFlags & AFF_68020) != 0)
		cpuType = 68020;
	else if ((SysBase->AttnFlags & AFF_68010) != 0)
		cpuType = 68010;
	else
		cpuType = 68000;

	if ((SysBase->AttnFlags & AFF_FPU40) != 0)
		hasFPU = true;

	if (cpuType == 68080) {
		hasAMMX = true;
		useSAGA = true;

		driverNames[1] = "Arne (Apollo Core)";
		driverDescs[1] = "16-ch/16-bit";

		UWORD model = (*((UWORD *)0xdff3fc)) >> 8;
		TRACE("Apollo model: %08x", model);
		if(model == 0x03 || model == 0x05 || model == 0x08)
			isV4Core = true;
	}

	return hasFPU;
}

static void reallocDisplayModes(BOOL renew)
{	
	struct Node * c, * n;

	if(displayCount >= 0) {
		if(c = displayModeLabels->lh_Head) {
			while(displayModeLabels->lh_Head->ln_Succ) {
				n = c->ln_Succ;
				if(c->ln_Name) {
					free(c->ln_Name);
				}
				Remove(c);
				FreeMem(c, sizeof(struct Node));
				c = n;
			}
		}
		FreeMem(displayModeLabels, sizeof(struct List *));
		displayModeLabels = NULL;
	}
	
	if(renew) {
		displayCount = 0;
		displayModeLabels = (struct List *) AllocMem(sizeof(struct List), MEMF_CLEAR);
		NewList(displayModeLabels);
	} else {
		displayCount = -1;
	}
}

#define ADD_DISPLAY_MODE(ID, W, H, DEPTH)                             \
	do {                                                              \
		struct Node * n = NULL;                                       \
		displayModeIDs[displayCount] = ID;                            \
		displayModeSizes[displayCount] = PPSize(W, H);                \
		displayModeDepths[displayCount] = DEPTH;                      \
		if(n = (Node *) AllocMem(sizeof(struct Node), MEMF_CLEAR)) {  \
			n->ln_Name = displayModeNames[displayCount];              \
			AddTail(displayModeLabels, n);                           \
		}                                                             \
		displayCount++;                                               \
	} while(0);

static Screen * discoverDisplayModes()
{
	ULONG modeID, readID;
	ULONG result;
	bool firstRun = true;
	DisplayInfoHandle displayHandle;
	struct DisplayInfo displayInfo;
	struct DimensionInfo dimensionInfo;
	struct MonitorInfo monitorInfo;
	struct NameInfo nameInfo;
	struct Screen * pubScreen;

	if(!(pubScreen = LockPubScreen(NULL)))
		return NULL;
	
	reallocDisplayModes(TRUE);
	
	displayModeIDs = new LONG[MAX_DISPLAY_MODES];
	memset(displayModeIDs, 0, MAX_DISPLAY_MODES * sizeof(LONG));
	displayModeNames = new char *[MAX_DISPLAY_MODES];
	memset(displayModeNames, 0, MAX_DISPLAY_MODES * sizeof(char *));
	displayModeSizes = new PPSize[MAX_DISPLAY_MODES];
	memset(displayModeSizes, 0, MAX_DISPLAY_MODES * sizeof(PPSize));
	displayModeDepths = new UWORD[MAX_DISPLAY_MODES];
	memset(displayModeDepths, 0, MAX_DISPLAY_MODES * sizeof(UWORD));

	do {
		bool isWindowed = false;

		//
		// First run:
		// Get current screen and check if we could support it for windowed mode
		//
		if(firstRun) {
			modeID = GetVPModeID(&(pubScreen->ViewPort));
			isWindowed = true;
			firstRun = false;
		}

		// Get and check a lot of data :-P
		if(!(modeID & MONITOR_ID_MASK))
			continue;
		if(ModeNotAvailable(modeID))
			continue;
		if(!(displayHandle = FindDisplayInfo(modeID)))
			continue;

		readID = modeID;
		if(isWindowed)
			modeID = INVALID_ID;

		if(!(result = GetDisplayInfoData(displayHandle, (UBYTE *) &displayInfo, sizeof(struct DisplayInfo), DTAG_DISP, 0)))
			continue;
		if(!(result = GetDisplayInfoData(displayHandle, (UBYTE *) &dimensionInfo, sizeof(struct DimensionInfo), DTAG_DIMS, 0)))
			continue;
		if(!(result = GetDisplayInfoData(displayHandle, (UBYTE *) &monitorInfo, sizeof(struct MonitorInfo), DTAG_MNTR, 0)))
			continue;
		if(!(result = GetDisplayInfoData(displayHandle, (UBYTE *) &nameInfo, sizeof(struct NameInfo), DTAG_NAME, 0)))
			continue;

		// Requirement is 640x480x16 RTG for now
		if(dimensionInfo.Nominal.MaxX+1 < 640)
			continue;
		if(dimensionInfo.Nominal.MaxY+1 < 480)
			continue;
		// On V4 we can always use PiP modes in all depths so no need to check for depth here
		if(!(isWindowed && useSAGA && isV4Core) && dimensionInfo.MaxDepth != 8 && dimensionInfo.MaxDepth != 16)
			continue;
		if(P96Base && !p96GetModeIDAttr(readID, P96IDA_ISP96))
			continue;
		else if(CyberGfxBase && !IsCyberModeID(readID))
			continue;

		TRACE("%03ld: %08lx: %5ldx%5ld %2ldbpp Win: %c PAL: %c PF: %08lx Name: %s", 
			displayCount, 
			modeID, 
			dimensionInfo.Nominal.MaxX+1, dimensionInfo.Nominal.MaxY+1,
			dimensionInfo.MaxDepth,
			isWindowed ? 'Y' : 'N',
			displayInfo.PropertyFlags & DIPF_IS_PAL ? 'Y' : 'N',
			displayInfo.PropertyFlags, nameInfo.Name);

		// Insert display mode
		if(isWindowed) {
			if(useSAGA && isV4Core) {
				displayModeNames[displayCount] = new char[256];
				strcpy(displayModeNames[displayCount], "Win: 640x480 SAGA-PiP 16-bit");
				ADD_DISPLAY_MODE(-1, 640, 480, 16);

				displayModeNames[displayCount] = new char[256];
				strcpy(displayModeNames[displayCount], "Win: 640x480 SAGA-PiP 8-bit");
				ADD_DISPLAY_MODE(-2, 640, 480, 8);
			} else {
				displayModeNames[displayCount] = new char[256];
				strcpy(displayModeNames[displayCount], "Win: 640x480 4-bit");
				ADD_DISPLAY_MODE(-3, 640, 480, 4);

				displayModeNames[displayCount] = new char[256];
				sprintf(displayModeNames[displayCount], "Win: 640x480 %ld-bit", dimensionInfo.MaxDepth);
				ADD_DISPLAY_MODE(-4, 640, 480, dimensionInfo.MaxDepth);
			}
		} else {
			displayModeNames[displayCount] = new char[256];
			sprintf(displayModeNames[displayCount], "FS: %s", nameInfo.Name);
			ADD_DISPLAY_MODE(readID, dimensionInfo.Nominal.MaxX+1, dimensionInfo.Nominal.MaxY+1, dimensionInfo.MaxDepth);
		}

		// Bail out when we reached the max number of display modes
		if(displayCount == MAX_DISPLAY_MODES-1)
			break;

	} while((modeID = NextDisplayInfo(modeID)) != INVALID_ID);

	// If no screen mode has been detected, bail out
	if(displayCount == 0) {
		UnlockPubScreen(NULL, pubScreen);
		pubScreen = NULL;

		return NULL;
	}

	return pubScreen;
}

static const char * setupErrors[] = {
	"",
	"GadTools error",
	"Cannot create setup window",
	"Cannot detect valid screen modes",
};

static int setup(AmigaApplication * app)
{
	int ret = 0;
	struct Gadget * gadgetList = NULL, * gadget;
	struct NewGadget newGadget = {0};
	struct Window * window;
	struct Screen * pubScreen;
	long winWidth, winHeight;
	bool setupRunning = true;
	struct Gadget * driverDesc, * mixTypeDesc;
	char detected[256] = {0};
	char winTitle[256] = {0};
	bool foundDisplay = false;
	int displayIndex;

	INFO("Starting setup", NULL);

	if(!(pubScreen = discoverDisplayModes())) {
		ERROR("Error discovering display mode", NULL);
		return -4;
	}

	// Search selected display
	for(displayIndex = 0; displayIndex < displayCount; displayIndex++) {
		if(displayModeIDs[displayIndex] == app->getDisplayID()) {
			foundDisplay = true;
			break;
		}
	}

	// If not found, take first
	if(!foundDisplay) {
		displayIndex = 0;
	}

    app->setDisplayID(displayModeIDs[displayIndex]);
    app->setWindowSize(displayModeSizes[displayIndex]);
    app->setBpp(displayModeDepths[displayIndex]);

	// Show setup only if forced or if prefs have not been stored already
	if(args.setup) {
		// Create Gadtools UI
		gadget = CreateContext(&gadgetList);
		if(!gadget) {
			ERROR("Cannot create context!", NULL);
			return -2;
		}

		newGadget.ng_VisualInfo = GetVisualInfo(pubScreen, TAG_END);
		newGadget.ng_TextAttr 	= pubScreen->Font;
		newGadget.ng_Flags 		= 0;

		newGadget.ng_LeftEdge   = pubScreen->WBorLeft + 4;
		newGadget.ng_TopEdge    = pubScreen->WBorTop + pubScreen->RastPort.TxHeight + 5;
		newGadget.ng_Width      = 30 * pubScreen->RastPort.TxWidth + 20;
		newGadget.ng_Height     = pubScreen->RastPort.TxHeight + 6;

		sprintf(detected, "Specs: %ld, FPU: %s, SAGA: %s, AMMX: %s, V4: %s",
			cpuType,
			hasFPU ? "Y" : "N",
			useSAGA ? "Y" : "N",
			hasAMMX ? "Y" : "N",
			isV4Core ? "Y" : "N");

		INFO("%s", detected);

		newGadget.ng_GadgetText = NULL;
		newGadget.ng_GadgetID   = GID_DETECTED;
		gadget = CreateGadget(TEXT_KIND, gadget, &newGadget,
			GTTX_Text, detected,
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		newGadget.ng_LeftEdge   = pubScreen->WBorLeft + 4 + 14 * pubScreen->RastPort.TxWidth;
		newGadget.ng_TopEdge   += newGadget.ng_Height + 4;
		newGadget.ng_Height     = pubScreen->RastPort.TxHeight * 7;
		newGadget.ng_GadgetText = "Screen mode";
		newGadget.ng_GadgetID   = GID_SCREEN_MODE;
		newGadget.ng_Flags      = PLACETEXT_LEFT;
		gadget = CreateGadget(LISTVIEW_KIND, gadget, &newGadget,
			GTLV_Labels, displayModeLabels,
			GTLV_Selected, displayIndex,
			GTLV_MakeVisible, displayIndex,
			GTLV_ShowSelected, NULL,
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		newGadget.ng_TopEdge   += newGadget.ng_Height + 4;
		newGadget.ng_Height     = pubScreen->RastPort.TxHeight + 6;
		newGadget.ng_GadgetText = "Audio driver";
		newGadget.ng_GadgetID   = GID_AUDIO_DRV;
		gadget = CreateGadget(CYCLE_KIND, gadget, &newGadget,
			GTCY_Labels, driverNames,
			GTCY_Active, app->getAudioDriver(),
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		newGadget.ng_TopEdge   += newGadget.ng_Height;
		newGadget.ng_GadgetText = NULL;
		newGadget.ng_GadgetID   = GID_AUDIO_DRV_DESC;
		gadget = CreateGadget(TEXT_KIND, gadget, &newGadget,
			GTTX_Text, driverDescs[app->getAudioDriver()],
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}
		driverDesc = gadget;

		newGadget.ng_TopEdge   += newGadget.ng_Height + 4;
		newGadget.ng_GadgetText = "Mixer type";
		newGadget.ng_GadgetID   = GID_AUDIO_MIXER;
		gadget = CreateGadget(CYCLE_KIND, gadget, &newGadget,
			GTCY_Labels, mixTypeNames,
			GTCY_Active, app->getAudioMixer(),
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		newGadget.ng_TopEdge   += newGadget.ng_Height;
		newGadget.ng_GadgetText = NULL;
		newGadget.ng_GadgetID   = GID_AUDIO_MIXER_DESC;
		gadget = CreateGadget(TEXT_KIND, gadget, &newGadget,
			GTTX_Text, mixTypeDescs[app->getAudioMixer()],
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}
		mixTypeDesc = gadget;

		newGadget.ng_LeftEdge   = pubScreen->WBorLeft + 4;
		newGadget.ng_TopEdge   += newGadget.ng_Height + 8;
		newGadget.ng_Width      = 22 * pubScreen->RastPort.TxWidth + 8;
		newGadget.ng_GadgetText = "Run";
		newGadget.ng_GadgetID   = GID_RUN;
		newGadget.ng_Flags      = 0;
		gadget = CreateGadget(BUTTON_KIND, gadget, &newGadget,
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		newGadget.ng_LeftEdge  += newGadget.ng_Width + 4;
		newGadget.ng_GadgetText = "Quit";
		newGadget.ng_GadgetID   = GID_QUIT;
		gadget = CreateGadget (BUTTON_KIND, gadget, &newGadget,
			TAG_END);
		if(!gadget) {
			ERROR("Cannot create gadget %d!", newGadget.ng_GadgetID);
			return -2;
		}

		// Open setup dialog
		winWidth = newGadget.ng_LeftEdge + newGadget.ng_Width + 4 + pubScreen->WBorRight;
		winHeight = newGadget.ng_TopEdge + newGadget.ng_Height + 4 + pubScreen->WBorBottom;

		sprintf(winTitle, "Setup (Built for " STR(AMIGA_ARCH) " @ " STR(NOW) ")");

		window = OpenWindowTags(NULL,
			WA_Width,  		winWidth,
			WA_Height, 		winHeight,
			WA_Left,   		(pubScreen->Width - winWidth) >> 1,
			WA_Top,    		(pubScreen->Height - winHeight) >> 1,
			WA_PubScreen,	pubScreen,
			WA_Title,		winTitle,
			WA_Flags,		WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
			WA_IDCMP,		IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_REFRESHWINDOW | BUTTONIDCMP | CYCLEIDCMP | STRINGIDCMP | LISTVIEWIDCMP,
			WA_Gadgets,		gadgetList,
			TAG_END
		);

		if(window) {
			struct IntuiMessage *imsg;

			GT_RefreshWindow(window, NULL);
			UnlockPubScreen(NULL, pubScreen);
			pubScreen = NULL;

			INFO("Starting setup window loop", NULL);
			do {
				if(Wait((1L << window->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
					setupRunning = false;

				while(imsg = GT_GetIMsg(window->UserPort)) {
					switch(imsg->Class) {
					case IDCMP_GADGETUP:
						gadget = (struct Gadget *) imsg->IAddress;
						switch (gadget->GadgetID) {
						case GID_SCREEN_MODE: {
								long num;
								GT_GetGadgetAttrs(gadget, window, NULL, GTLV_Selected, &num, TAG_END);
								app->setDisplayID(displayModeIDs[num]);
								app->setWindowSize(displayModeSizes[num]);
								app->setBpp(displayModeDepths[num]);
							}
							break;
						case GID_AUDIO_DRV: {
								long num;
								GT_GetGadgetAttrs(gadget, window, NULL, GTCY_Active, &num, TAG_END);
								GT_SetGadgetAttrs(driverDesc, window, NULL, GTTX_Text, driverDescs[num], TAG_END);
								app->setAudioDriver(drivers[num]);
							}
							break;
						case GID_AUDIO_MIXER: {
								long num;
								GT_GetGadgetAttrs(gadget, window, NULL, GTCY_Active, &num, TAG_END);
								GT_SetGadgetAttrs(mixTypeDesc, window, NULL, GTTX_Text, mixTypeDescs[num], TAG_END);
								app->setAudioMixer(mixTypes[num]);
							}
							break;
						case GID_RUN:
							setupRunning = false;
							break;
						case GID_QUIT:
							setupRunning = false;
							ret = 1;
							break;
						}
						break;
					case IDCMP_VANILLAKEY:
						if(imsg->Code == 0x1b) {
							setupRunning = false;
							ret = -1;
						}
						break;
					case IDCMP_CLOSEWINDOW:
						setupRunning = false;
						ret = -1;
						break;
					case IDCMP_REFRESHWINDOW:
						GT_BeginRefresh(window);
						GT_EndRefresh(window, TRUE);
						break;
					}

					GT_ReplyIMsg (imsg);
				}
			} while(setupRunning);

			CloseWindow(window);
		} else {
			INFO("Could not open setup window", NULL);
			ret = -3;
		}

		FreeGadgets(gadgetList);
		FreeVisualInfo(newGadget.ng_VisualInfo);
	}

	if(pubScreen)
		UnlockPubScreen(NULL, pubScreen);

	delete[] displayModeNames;
	delete[] displayModeIDs;
	delete[] displayModeSizes;
	delete[] displayModeDepths;

	return ret;
}

static int boot(int argc, char * argv[])
{
	int ret = 0;
	EasyStruct easyStruct;
	bool fromWorkbench;
	char exePath[256] = {0};
	BPTR dirLock, oldDir;
	struct DiskObject * diskObj;

	INFO("Booting application", NULL);

	app = new AmigaApplication();

	INFO("Reading boot configuration", NULL);
	loadBootConfig(app);

	app->setCpuType(cpuType);
	app->setHasFPU(hasFPU);
	app->setHasAMMX(hasAMMX);
	app->setUseSAGA(useSAGA);
	app->setIsV4Core(isV4Core);
	app->setUseP96(P96Base != NULL);
	app->setUseCGX(CyberGfxBase != NULL);

	// Get program path from WBStartup/argv
   	fromWorkbench = argc == 0;
	if(fromWorkbench) {
   		struct WBArg * workbenchArgs = ((struct WBStartup *) argv)->sm_ArgList;

		INFO("Started from Workbench", NULL);

		dirLock = workbenchArgs->wa_Lock;
        programDirLock = workbenchArgs->wa_Lock;
		strncpy(exePath, (char *) workbenchArgs->wa_Name, 255);
		oldDir = CurrentDir(dirLock);
	} else {
		INFO("Started from CLI", NULL);

        programDirLock = GetProgramDir();
		strncpy(exePath, argv[0], 255);
	}

	if(fromWorkbench) {
		CurrentDir(oldDir);
	}

	// Show setup dialog
	ret = setup(app);
	if(ret < 0) {
		ERROR("Setup failed: %s", setupErrors[(-ret)-1]);
	} else if(ret == 0) {
		// @todo Add new splash screen
		app->setNoSplash(true);

		// And start
		if(ret = app->start()) {
			ERROR("Starting tracker failed! (ret = %ld)", ret);
			app->stop();
		} else {
			do {
				app->loop();
			} while(app->stop() > 0);

			// Only save boot config if application ran successfully
			saveBootConfig(app);
		}
	}
	
	reallocDisplayModes(FALSE);

	delete app;

	return ret == -1 ? 0 : ret;
}

int main2(int argc, char * argv[])
{
	struct RDArgs * rd;
	int ret = 0;

	rd = ReadArgs("SETUP/S", (LONG *) &args, NULL);

	INFO("Checking hardware", NULL);

	// Check hardware
	if(checkHardware()) {
		// Open libraries and boot application
		if(KeymapBase = OpenLibrary("keymap.library", 39)) {
			if(IntuitionBase = OpenLibrary("intuition.library", 39)) {
				if(IconBase = OpenLibrary("icon.library", 37)) {
					if(GfxBase = OpenLibrary("graphics.library", 39)) {
						P96Base = OpenLibrary(P96NAME, 2);
						CyberGfxBase = OpenLibrary("cybergraphics.library", 39);

						if(P96Base || CyberGfxBase) {
							BYTE err = OpenDevice("timer.device", 0, &timereq, 0);
							if(err == 0) {
								TimerBase = timereq.io_Device;
								GetSysTime(&startTime);

								if(hasAMMX) {
									INFO("AMMX detected, trying to load vampire.resource", NULL);

									if(!(VampireBase = (struct Library *) OpenResource(V_VAMPIRENAME))) {
										ERROR("Could not find vampire.resource!", NULL);
										ret = 2;
									} else if(VampireBase->lib_Version < 45) {
										ERROR("Vampire.resource version needs to be 45 or higher!", NULL);
										ret = 2;
									} else if(V_EnableAMMX(V_AMMX_V2) == VRES_ERROR) {
										ERROR("Cannot enable AMMX V2+!", NULL);
										ret = 2;
									}

									INFO("vampire.resource loaded", NULL);
								}

								if(!ret) {
									ret = boot(argc, argv);
								}

								CloseDevice(&timereq);
							} else {
								ERROR("Could not open timer.device! (err = %ld)", NULL);
								ret = 1;
							}

							if(P96Base)
								CloseLibrary(P96Base);
							if(CyberGfxBase)
								CloseLibrary(CyberGfxBase);
						} else {
							ERROR("Could not open %s V2! This program needs RTG installed.", P96NAME);
							ret = 1;
						}
						CloseLibrary(GfxBase);
					} else {
						ERROR("Could not open graphics.library V39!", NULL);
						ret = 1;
					}
					CloseLibrary(IconBase);
				} else {
					ERROR("Could not open icon.library V37!", NULL);
					ret = 1;
				}
				CloseLibrary(IntuitionBase);
			}  else {
				ERROR("Could not open intuition.library V39!", NULL);
				ret = 1;
			}
			CloseLibrary(KeymapBase);
		} else {
			ERROR("Could not open keymap.library V39!", NULL);
			ret = 1;
		}
	} else {
		ERROR("Hardware does not fulfil requirements!", NULL);
		ret = 1;
	}

	FreeArgs(rd);

	return ret;
}
