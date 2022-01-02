#include "project.h"

#ifdef __AMIGA__
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *MUIMasterBase;
#endif

enum {
	BTN_OPEN = 55,
	BTN_PLAY = 56,
	BTN_STOP = 57
};

int main(int argc,char *argv[])
{
#ifdef __AMIGA__
	Object *app, *win1, *rootObj, *btnOpen, *btnPlay, *btnStop;
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
		Child, (ULONG)(MUI_MakeObject(MUIO_Label,(ULONG)"MilkyPlayer",TAG_END)),
		Child, (ULONG)(MUI_MakeObject(MUIO_HBar,4,TAG_END)),
		Child, (ULONG)(MUI_NewObject(MUIC_Group,
			MUIA_Group_Horiz, TRUE,
				Child, (ULONG)(btnOpen = MUI_MakeObject(MUIO_Button, (ULONG)"_Open", TAG_END)),
				Child, (ULONG)(btnPlay = MUI_MakeObject(MUIO_Button, (ULONG)"_Play", TAG_END)),
				Child, (ULONG)(btnStop = MUI_MakeObject(MUIO_Button, (ULONG)"_Stop", TAG_END)),
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
	
	//Printf("Driver: %s\n", (ULONG)master->getCurrentDriverName());
	master->setCurrentDriverByName(master->getFirstDriverName());
	Printf("Driver: %s\n", (ULONG)master->getCurrentDriverName());
	
	PlayerController *controller = master->createPlayerController(true);
	bool* muteChannels = new bool[34];
	muteChannels[0] = muteChannels[1] = muteChannels[2] = muteChannels[3] = FALSE;
	
#ifdef __AMIGA__
	
	DoMethod(win1, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod(btnOpen, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_OPEN);
	DoMethod(btnPlay, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_PLAY);
	DoMethod(btnStop, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_STOP);

	SetAttrs(win1, MUIA_Window_Open, TRUE, TAG_DONE);
	const char* file;
	bool play = false;
	while(running)
	{
		ULONG id = DoMethod(app,MUIM_Application_Input,&signals);

		switch(id)
		{
			case MUIV_Application_ReturnID_Quit:
				if((MUI_RequestA(app,0,0,"Quit?","_Yes|_No","\33cAre you sure?",0)) == 1)
					running = FALSE;
				break;
			case BTN_OPEN:
				play = false;
				controller->stop();
				file = GetFileName("Load mod");
				
				Printf("Loading file %s\n",(ULONG)file);
				module->loadModule(file);
				controller->attachModuleEditor(module);

				play = true;
				break;
			case BTN_PLAY:
				play = true;
				if (module->isModuleLoaded()) {
					//controller->playSong(0,0, muteChannels);
					controller->restartPlaying();
					controller->resumePlayer(true);
				}

				break;
			case BTN_STOP:
				play = false;
				controller->stop();
				break;
		}
		
		if (module->isModuleLoaded() && !controller->isPlaying() && play) {
			controller->playSong(0,0, muteChannels);
			controller->resumePlayer(true);
		}
		
			
		if(running && signals) Wait(signals);
	}

	SetAttrs(win1,MUIA_Window_Open,FALSE);

	if(btnStop) MUI_DisposeObject(btnStop);
	if(btnPlay) MUI_DisposeObject(btnPlay);
	if(btnOpen) MUI_DisposeObject(btnOpen);
	if(rootObj) MUI_DisposeObject(rootObj);
	if(win1) MUI_DisposeObject(win1);
	if(app) MUI_DisposeObject(app);
#else
	while (controller->isPlaying());
#endif
	delete module;
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