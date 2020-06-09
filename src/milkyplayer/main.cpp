#define MILKYTRACKER 1
#include <SDL.h>
#include <XModule.h>

#include "project.h"
#include "amigaversion.h"
#include "PlayerMaster.h"
#include "PlayerController.h"

#ifdef __AMIGA__
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *MUIMasterBase;
#endif

int main(int argc,char *argv[])
{
#ifdef __AMIGA__
	Object *app,*win1, *rootObj;
	ULONG signals;
#endif
	BOOL running = TRUE;
	
	if (!Open_Libs())
	{
		Printf("Cannot open libs\n");
		return(0);
	}
	
#ifdef __AMIGA__
	rootObj = MUI_NewObject(MUIC_Group,
		MUIA_Group_Child, (ULONG)(MUI_NewObject(MUIC_Text,
			MUIA_Text_Contents, (ULONG)"MilkyPlayer",
			TAG_END)),
		MUIA_Group_Child, (ULONG)(MUI_NewObject(MUIC_Framedisplay,
			MUIA_FrameTitle, (ULONG)"MilkyPlayer",
			TAG_END)),
		TAG_END);
	
	win1 = MUI_NewObject(MUIC_Window,
						 MUIA_Window_Title, (ULONG)"MilkyPlayer",
						 MUIA_Window_SizeGadget, FALSE,
						 MUIA_Window_RootObject, (ULONG)(rootObj),
						 TAG_END);
	
	app = MUI_NewObject(MUIC_Application,
						MUIA_Application_Author, (ULONG)"AmigaDev Team",
						MUIA_Application_Base, (ULONG)"MilkyPlayer",
						MUIA_Application_Copyright, (ULONG)"© 2020 Marlon Beijer",
						MUIA_Application_Description, (ULONG)"MilkyPlayer in MUI.",
						MUIA_Application_Title, (ULONG)"MilkyPlayer",
						MUIA_Application_Version, (ULONG)amiga_ver,
						MUIA_Application_Window, (ULONG)(win1),
						TAG_END);

	if (!app)
	{
		printf("Cannot create application.\n");
		return(0);
	}
#endif
	
	SDL_Init( SDL_INIT_EVERYTHING);
	atexit(SDL_Quit);

	XModule *module = new XModule();
	PlayerMaster *master = new PlayerMaster();
	Printf("Driver: %s\n", (ULONG)master->getCurrentDriverName());
	master->setCurrentDriverByName(master->getFirstDriverName());
	Printf("Driver: %s\n", (ULONG)master->getCurrentDriverName());
	//master->reallocateChannels(32, 0);
	PlayerController *controller = master->createPlayerController(true);
	bool* muteChannels = new bool[34];
	muteChannels[0] = muteChannels[1] = muteChannels[2] = muteChannels[3] = false;

	Printf("Controller acquired\n");
	
	module->loadModule("test.mod");
	while (!module->isModuleLoaded()) {
		Printf("test2\n");
	}
	
	controller->attachModuleEditor(module);
	controller->playSong(0,0, muteChannels);
	controller->continuePlaying();
	controller->resumePlayer(true);
#ifdef __AMIGA__
	DoMethod(win1, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

	SetAttrs(win1, MUIA_Window_Open, TRUE, TAG_DONE);

	while(running)
	{
		ULONG id = DoMethod(app,MUIM_Application_Input,&signals);

		switch(id)
		{
			case MUIV_Application_ReturnID_Quit:


				if((MUI_RequestA(app,0,0,"Quit?","_Yes|_No","\33cAre you sure?",0)) == 1)
					running = FALSE;
				break;
		}
		if(running && signals) Wait(signals);
	}

	SetAttrs(win1,MUIA_Window_Open,FALSE);

	if(app) MUI_DisposeObject(app);
#else
	while (controller->isPlaying());
#endif
	delete master;
	Close_Libs();
	exit(EXIT_SUCCESS);
}

BOOL Open_Libs()
{
#ifdef __AMIGA__
	if ( !(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39)) )
		return(FALSE);

	if ( !(GfxBase=(struct GfxBase *) OpenLibrary("graphics.library",0)) )
	{
		CloseLibrary((struct Library *)IntuitionBase);
		return(FALSE);
	}

	if ( !(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,19)) )
	{
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return(FALSE);
	}
#endif

	return(TRUE);
}

void Close_Libs()
{
#ifdef __AMIGA__
if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);

	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);

	if (MUIMasterBase)
		CloseLibrary(MUIMasterBase);
#endif
}