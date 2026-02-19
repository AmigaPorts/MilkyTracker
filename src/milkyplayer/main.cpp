#include "project.h"
#include "Button.h"
#include "Container.h"
#include "tracker/ControlIDs.h"
#include "tracker/TrackerConfig.h"
#include "PPUI.h"
#include "tracker/SectionAbstract.h"
#include "tracker/SectionSamples.h"
#include "tracker/SectionInstruments.h"

#if defined(__AMIGA__)

/* MUI Objects */
static Object *app, *win1, *btnOpen, *btnPlay, *btnStop, *driverString, *modFileString, *bpmString, *speedString, *positionString, *rowString;
static PPScreen *screen;
//static Tracker* tracker = new Tracker();
#else
static bool app;
#endif

enum {
	BTN_OPEN = 55,
	BTN_PLAY = 56,
	BTN_STOP = 57
};

static mp_sint32 BPM, speed;
static char out[24];

/* MilkyPlay Objects */
static PlayerMaster *master;
static PlayerController *controller;
static XModule *module;

void initMilkyPlay() {
	module = new XModule();
	master = new PlayerMaster();
	controller = master->createPlayerController(true);
}

void initListboxesSection(pp_int32 x, pp_int32 y)
{
	pp_int32 size = (screen->getWidth()-x) / 2 - 4;

	if (size > 236)
		size = 236;

#ifndef __LOWRES__
	const pp_int32 tinyButtonHeight = 10;
	const pp_int32 tinyButtonOffset = -1;
	pp_int32 height = 118;
	pp_int32 dy = 4;
#else
	const pp_int32 tinyButtonHeight = 9;
	const pp_int32 tinyButtonOffset = -1;
	pp_int32 height = 64;
	pp_int32 dy = 3;
#endif

	PPButton* button;

	// Crippled main menu & jam menu
#ifdef __LOWRES__
	pp_int32 myDx = 55;
	{
		pp_int32 bHeight = 14;
		
		pp_int32 x2 = x+4;
		pp_int32 y2 = y+2+3;

		PPContainer* container = new PPContainer(CONTAINER_LOWRES_TINYMENU, screen, this, PPPoint(x, y), PPSize((size-myDx)+7,height), false);
		container->setColor(TrackerConfig::colorThemeMain);
		
		button = new PPButton(MAINMENU_PLAY_SONG, screen, this, PPPoint(x2, y2), PPSize(73, bHeight-1));
		button->setText("Play Sng");		
		container->addControl(button);
		
		button = new PPButton(MAINMENU_PLAY_PATTERN, screen, this, PPPoint(x2, y2 + 1*bHeight), PPSize(73, bHeight-1));
		button->setText("Play Pat");		
		container->addControl(button);
		
		/*button = new PPButton(MAINMENU_STOP, screen, this, PPPoint(x2, y2 + 2*bHeight), PPSize(73, bHeight-1));
		button->setText("Stop");		
		container->addControl(button);*/

		button = new PPButton(MAINMENU_STOP, screen, this, PPPoint(x2, y2 + 2*bHeight), PPSize((73>>1) - 1, bHeight-1));
		button->setText("Stop");
		container->addControl(button);
		
		button = new PPButton(MAINMENU_EDIT, screen, this, PPPoint(x2 + (73>>1), y2 + 2*bHeight), PPSize((73>>1)+1, bHeight-1), true, true, false);
		button->setText("Rec");
		container->addControl(button);
		
		button = new PPButton(BUTTON_MENU_ITEM_ADDCHANNELS, screen, this, PPPoint(x2, y2 + 3*bHeight), PPSize((73>>1) - 1, bHeight-1));
		button->setText("Add");
		container->addControl(button);
		
		button = new PPButton(BUTTON_MENU_ITEM_SUBCHANNELS, screen, this, PPPoint(x2 + (73>>1), y2 + 3*bHeight), PPSize((73>>1)+1, bHeight-1));
		button->setText("Sub");
		container->addControl(button);

		x2+=74;

		button = new PPButton(MAINMENU_INSEDIT, screen, this, PPPoint(x2, y2), PPSize(26, bHeight-1));
		button->setText("Ins");		
		container->addControl(button);
		
		button = new PPButton(MAINMENU_SMPEDIT, screen, this, PPPoint(x2, y2 + 1*bHeight), PPSize(26, bHeight-1));
		button->setText("Smp");		
		container->addControl(button);
		
		button = new PPButton(MAINMENU_ADVEDIT, screen, this, PPPoint(x2, y2 + 2*bHeight), PPSize(26, bHeight-1));
		button->setText("Adv");		
		container->addControl(button);

		button = new PPButton(MAINMENU_TRANSPOSE, screen, this, PPPoint(x2, y2 + 3*bHeight), PPSize(26, bHeight-1));
		button->setText("Trn");		
		container->addControl(button);
		
		// play pattern/position
		button = static_cast<PPButton*>(container->getControlByID(MAINMENU_PLAY_PATTERN));
		button->setText("Pat");
		button->setSize(PPSize((73>>1)-1, bHeight-1));
		
		PPPoint p = button->getLocation();
		p.x+=button->getSize().width+1;
		
		button = new PPButton(MAINMENU_PLAY_POSITION, screen, this, p, PPSize((73>>1)+1, bHeight-1));
		button->setText("Pos");
		container->addControl(button);
		
		screen->addControl(container);
		
		x+=(size-myDx)+7;
	}

	size-=4;

	{
		pp_int32 height2 = height + 39 + 14;
		pp_int32 y3 = y - 39 - 14;
		
		pp_int32 x2 = 0+4;

		PPContainer* container = new PPContainer(CONTAINER_LOWRES_JAMMENU, screen, this, PPPoint(0, y3), PPSize(screen->getWidth(),height2), false);
		container->setColor(TrackerConfig::colorThemeMain);

		pp_int32 bHeight = 12;
		pp_int32 y2 = y3 + 1;

		PianoControl* pianoControl = new PianoControl(PIANO_CONTROL, screen, inputControlListener, 
													  PPPoint(container->getLocation().x+2, y2), PPSize(screen->getWidth() - 4, 25*3+12), ModuleEditor::MAX_NOTE); 
		// show C-3
		pianoControl->setBorderColor(TrackerConfig::colorThemeMain);
		pianoControl->setMode(PianoControl::ModePlay);
		pianoControl->setxScale(6);
		pianoControl->setyScale(3);		
		pianoControl->assureNoteVisible(12*4);
		
		container->addControl(pianoControl);
		
		x2 = 0+4;
		y2+=pianoControl->getSize().height+1;
		
		PPStaticText* staticText = new PPStaticText(0, NULL, NULL, PPPoint(x2, y2+2), "Pos", true);
		container->addControl(staticText);	
		
		staticText = new PPStaticText(STATICTEXT_JAMMENU_CURORDER, NULL, NULL, PPPoint(x2 + 3*8+4, y2+2), "FF");
		container->addControl(staticText);	

		button = new PPButton(BUTTON_JAMMENU_NEXTORDERLIST, screen, this, PPPoint(x2 + 5*8+4 + 1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonPlus);
		container->addControl(button);
		
		button = new PPButton(BUTTON_JAMMENU_PREVORDERLIST, screen, this, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonMinus);
		container->addControl(button);
		
		pp_int32 x3 = button->getLocation().x + button->getSize().width+3;
		
		staticText = new PPStaticText(0, NULL, NULL, PPPoint(x3, y2+2), "Pat", true);
		container->addControl(staticText);	

		staticText = new PPStaticText(STATICTEXT_JAMMENU_CURPATTERN, NULL, NULL, PPPoint(x3 + 3*8+4, y2+2), "FF");
		container->addControl(staticText);	

		button = new PPButton(BUTTON_PATTERN_PLUS, screen, this, PPPoint(x3 + 5*8+4 + 1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonPlus);
		container->addControl(button);
		
		button = new PPButton(BUTTON_PATTERN_MINUS, screen, this, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonMinus);
		container->addControl(button);

		x3 = button->getLocation().x + button->getSize().width+3;
		
		staticText = new PPStaticText(0, NULL, NULL, PPPoint(x3, y2+2), "Ins", true);
		container->addControl(staticText);	

		staticText = new PPStaticText(STATICTEXT_JAMMENU_CURINSTRUMENT, NULL, NULL, PPPoint(x3 + 3*8+4, y2+2), "FF");
		container->addControl(staticText);	

		button = new PPButton(BUTTON_JAMMENU_NEXTINSTRUMENT, screen, this, PPPoint(x3 + 5*8+4 + 1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonUp);
		container->addControl(button);
		
		button = new PPButton(BUTTON_JAMMENU_PREVINSTRUMENT, screen, this, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(12, 11));
		button->setText(TrackerConfig::stringButtonDown);
		container->addControl(button);

		button = new PPButton(INPUT_BUTTON_INS, screen, inputControlListener, PPPoint(button->getLocation().x + button->getSize().width+1 + 2, y2), PPSize(17, 11));
		button->setFont(PPFont::getFont(PPFont::FONT_TINY));
		button->setText("Ins");
		container->addControl(button);

		button = new PPButton(INPUT_BUTTON_DEL, screen, inputControlListener, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(17, 11));
		button->setFont(PPFont::getFont(PPFont::FONT_TINY));
		button->setText("Del");
		container->addControl(button);

		button = new PPButton(INPUT_BUTTON_BACK, screen, inputControlListener, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(22, 11));
		button->setFont(PPFont::getFont(PPFont::FONT_TINY));
		button->setText("Back");
		container->addControl(button);

		button = new PPButton(INPUT_BUTTON_KEYOFF, screen, inputControlListener, PPPoint(button->getLocation().x + button->getSize().width+1, y2), PPSize(17, 11));
		button->setFont(PPFont::getFont(PPFont::FONT_TINY));
		button->setText("Off");
		container->addControl(button);

		button = new PPButton(BUTTON_JAMMENU_TOGGLEPIANOSIZE, screen, this, PPPoint(button->getLocation().x + button->getSize().width+3, y2+1), PPSize(12, 11), false);
		button->setColor(TrackerConfig::colorThemeMain);
		button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
		button->setText(TrackerConfig::stringButtonCollapsed);
		container->addControl(button);

		x2 = 0+4;
		y2+=14;
		
		button = new PPButton(MAINMENU_PLAY_SONG, screen, this, PPPoint(x2, y2), PPSize(77, bHeight-1));
		button->setText("Play Sng");		
		container->addControl(button);
		
		x2+=button->getSize().width+1;
		button = new PPButton(MAINMENU_PLAY_PATTERN, screen, this, PPPoint(x2, y2), PPSize((77>>1)-1, bHeight-1));
		button->setText("Pat");		
		container->addControl(button);

		x2+=button->getSize().width+1;
		button = new PPButton(MAINMENU_PLAY_POSITION, screen, this, PPPoint(x2, y2), PPSize((77>>1)+1, bHeight-1));
		button->setText("Pos");
		container->addControl(button);
		
		x2+=button->getSize().width+1;
		button = new PPButton(MAINMENU_STOP, screen, this, PPPoint(x2, y2), PPSize((77>>1), bHeight-1));
		button->setText("Stop");		
		container->addControl(button);
		
		// Add "Edit" button
		x2+=button->getSize().width+1;
		button = new PPButton(MAINMENU_EDIT, screen, this, PPPoint(x2, y2), PPSize((77>>1), bHeight-1), true, true, false);
		button->setText("Rec");
		container->addControl(button);	
		
		x2+=button->getSize().width+1;
		button = new PPButton(BUTTON_MENU_ITEM_ADDCHANNELS, screen, this, PPPoint(x2, y2), PPSize((77>>1) - 1, bHeight-1));
		button->setText("Add");
		container->addControl(button);
		
		x2+=button->getSize().width+1;
		button = new PPButton(BUTTON_MENU_ITEM_SUBCHANNELS, screen, this, PPPoint(x2, y2), PPSize((77>>1)+1, bHeight-1));
		button->setText("Sub");
		container->addControl(button);		

		screen->addControl(container);		
	}
#else
	pp_int32 myDx = 0;
#endif

	PPContainer* container = new PPContainer(CONTAINER_INSTRUMENTLIST, screen, nullptr, PPPoint(x, y), PPSize(screen->getWidth()-x,height), false);
	container->setColor(TrackerConfig::colorThemeMain);

	// Instruments
#ifndef __LOWRES__
	button = new PPButton(BUTTON_INSTRUMENT, screen, nullptr, PPPoint(x+2, y+dy-2), PPSize(screen->getWidth() < 800 ? 3*8+4 : 11*8+4, 12), false, true, false);
	button->setText(screen->getWidth() < 800 ? "Ins" : "Instruments");
	button->setColor(TrackerConfig::colorThemeMain);
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	button->setPressed(true);
	//PPStaticText* staticText = new PPStaticText(0, NULL, NULL, PPPoint(x+3, y+dy), screen->getWidth() < 800 ? "Ins" : "Instruments", true);
#else
	button = new PPButton(BUTTON_INSTRUMENT, screen, this, PPPoint(x+2, y+dy-2), PPSize(11*8+4, 11), false, true, false);
	button->setText("Instruments");
	button->setColor(TrackerConfig::colorThemeMain);
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	button->setPressed(true);
	{
		PPStaticText* staticText = new PPStaticText(STATICTEXT_INSTRUMENTS_ALTERNATIVEHEADER, screen, this, PPPoint(x+3, y+dy), "Samples / Ins:", true);
		staticText->hide(true);
		container->addControl(staticText);
		
		staticText = new PPStaticText(STATICTEXT_INSTRUMENTS_ALTERNATIVEHEADER2, screen, this, PPPoint(x+3 + 14*8, y+dy), "xx", false);
		staticText->hide(true);
		container->addControl(staticText);
	}
#endif
	container->addControl(button);

	button = new PPButton(BUTTON_INSTRUMENTS_PLUS, screen, nullptr, PPPoint(x+button->getSize().width+4, y+dy+tinyButtonOffset), PPSize(15, tinyButtonHeight));
	button->setText(TrackerConfig::stringButtonPlus);
	container->addControl(button);

	button = new PPButton(BUTTON_INSTRUMENTS_MINUS, screen, nullptr, PPPoint(button->getLocation().x + 16, y+dy+tinyButtonOffset), PPSize(15, tinyButtonHeight));
	button->setText(TrackerConfig::stringButtonMinus);
	container->addControl(button);

/*
	auto sectionInstruments = new SectionInstruments(nullptr);
	auto sectionSamples = new SectionSamples(nullptr);
*/

/*
#ifndef __LOWRES__
	button = new PPButton(BUTTON_INSTRUMENTEDITOR_CLEAR, screen, sectionInstruments, PPPoint(x+2 + size - 2 - 92, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Zap");
	container->addControl(button);

	button = new PPButton(BUTTON_INSTRUMENTEDITOR_LOAD, screen, sectionInstruments, PPPoint(x+2 + size - 2 - 61, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Load");
	container->addControl(button);

	button = new PPButton(BUTTON_INSTRUMENTEDITOR_SAVE, screen, sectionInstruments, PPPoint(x+2 + size - 2 - 30, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Save");
	container->addControl(button);
#else
	button = new PPButton(BUTTON_INSTRUMENTS_FLIP, screen, this, PPPoint(container->getLocation().x + container->getSize().width - 27, y+dy+tinyButtonOffset - 1), PPSize(24, 11), false);
	button->setText("Flip");
	button->setColor(TrackerConfig::colorThemeMain);
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	container->addControl(button);

	button = new PPButton(BUTTON_JAMMENU_PREVINSTRUMENT, screen, this, PPPoint(button->getLocation().x - 16, y+dy+tinyButtonOffset), PPSize(15, tinyButtonHeight));
	button->setText(TrackerConfig::stringButtonUp);
	container->addControl(button);
	
	button = new PPButton(BUTTON_JAMMENU_NEXTINSTRUMENT, screen, this, PPPoint(button->getLocation().x - 16, y+dy+tinyButtonOffset), PPSize(15, tinyButtonHeight));
	button->setText(TrackerConfig::stringButtonDown);
	container->addControl(button);
#endif

	listBoxInstruments = new PPListBox(LISTBOX_INSTRUMENTS, screen, this, PPPoint(x+2, y + 7 + dy + dy), PPSize(size+myDx,height-(10+2*dy)), true, true, true, true);
	listBoxInstruments->setBorderColor(TrackerConfig::colorThemeMain);
	listBoxInstruments->setShowIndex(true);
	listBoxInstruments->setMaxEditSize(ModuleEditor::MAX_INSTEXT);
	//listBoxInstruments->setSelectOnScroll(true);

	fillInstrumentListBox(listBoxInstruments);

	container->addControl(listBoxInstruments);

	// Samples
#ifndef __LOWRES__
	button = new PPButton(BUTTON_SAMPLE_EDIT_CLEAR, screen, sectionSamples, PPPoint(x+2 + size*2 + 4 - 2 - 92, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Clear");
	container->addControl(button);

	button = new PPButton(BUTTON_SAMPLE_LOAD, screen, sectionSamples, PPPoint(x+2 + size*2 + 4 - 2 - 61, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Load");
	container->addControl(button);

	button = new PPButton(BUTTON_SAMPLE_SAVE, screen, sectionSamples, PPPoint(x+2 + size*2 + 4 - 2 - 30, y+dy+tinyButtonOffset), PPSize(30, tinyButtonHeight));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Save");
	container->addControl(button);
#endif

	PPStaticText* staticText = new PPStaticText(STATICTEXT_SAMPLEHEADER, NULL, NULL, PPPoint(x+size+myDx+4+3, y+dy), "Samples", true);
	container->addControl(staticText);

	listBoxSamples = new PPListBox(LISTBOX_SAMPLES, screen, this, PPPoint(x+2 + size+4, y + 7 + dy + dy), PPSize(size,height-(10+2*dy)), true, true, true, true);
	listBoxSamples->setBorderColor(TrackerConfig::colorThemeMain);
	listBoxSamples->setShowIndex(true);
	listBoxSamples->setMaxEditSize(ModuleEditor::MAX_SMPTEXT);
	listBoxSamples->setIndexBaseCount(0);

	container->addControl(listBoxSamples);

#ifdef __LOWRES__
	listBoxSamples->hide(true);
#endif
*/
	screen->addControl(container);
}

void initSectionMainOptions(pp_int32 x, pp_int32 y)
{
	pp_int32 i,j;

#ifndef __LOWRES__
	pp_int32 bHeight = 12;
	PPSize size(320, 54);
#else
	pp_int32 bHeight = 14;
	PPSize size(320, 64);
#endif

	PPContainer* container = new PPContainer(CONTAINER_MENU, screen, nullptr, PPPoint(x, y), size, true);
	container->setRows(4);
	container->setColumns(4);
	container->setSpacing(0);
	container->setColor(TrackerConfig::colorThemeMain);

	PPContainer* saveCont = new PPContainer(CONTAINER_SAVE, screen, nullptr, PPPoint(x, y), size, false);
	saveCont->setRows(1);
	saveCont->setColumns(2);
	saveCont->setSpacing(0);

	PPContainer* patposCont = new PPContainer(CONTAINER_SAVE, screen, nullptr, PPPoint(x, y), size, false);
	patposCont->setRows(1);
	patposCont->setColumns(2);
	patposCont->setSpacing(0);
	
	PPContainer* stoprecCont = new PPContainer(CONTAINER_SAVE, screen, nullptr, PPPoint(x, y), size, false);
	stoprecCont->setRows(1);
	stoprecCont->setColumns(2);
	stoprecCont->setSpacing(0);

	PPContainer* addsubCont = new PPContainer(CONTAINER_SAVE, screen, nullptr, PPPoint(x, y), size, false);
	addsubCont->setRows(1);
	addsubCont->setColumns(2);
	addsubCont->setSpacing(0);

#ifdef __LOWRES__
	y+=2;
#endif

	PPButton* button;

	for (j = 0; j < 4; j++)
	{
		for (i = 0; i < 4; i++)
		{
			if (j * 4 + i < 15)
			{
				
				button = new PPButton(BUTTON_MENU_ITEM_0 + j*4+i, screen, nullptr, PPPoint(x+4 + i*78, y + 3 + j*bHeight), PPSize(77, bHeight-1));
				button->setText("Unused");

				switch (BUTTON_MENU_ITEM_0 + j*4+i) {
					case MAINMENU_SAVE:
						container->addControl(saveCont);
						saveCont->addControl(button);
						saveCont->setSize(PPSize(77, bHeight-1));
						break;
					case MAINMENU_PLAY_PATTERN:
						container->addControl(patposCont);
						patposCont->addControl(button);
						patposCont->setSize(PPSize(77, bHeight-1));
						break;
					case MAINMENU_STOP:
						container->addControl(stoprecCont);
						stoprecCont->addControl(button);
						stoprecCont->setSize(PPSize(77, bHeight-1));
						break;
					default:
						container->addControl(button);
				}
			}
		}

	}

#ifdef __AMIGA__
#define SMALL_BTN_SIZE 72
#else
#define SMALL_BTN_SIZE 77
#endif

	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_PLAY_SONG))->setText("Play Sng");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_PLAY_PATTERN))->setText("Play Pat");
	//static_cast<PPButton*>(container->getControlByID(MAINMENU_STOP))->setText("Stop");
	// Setup "Stop" PPButton
	button = dynamic_cast<PPButton*>(stoprecCont->getControlByID(MAINMENU_STOP));
	button->setText("Stop");
	button->setSize(PPSize(SMALL_BTN_SIZE>>1, bHeight-1));
	// Add "Edit" button
	button = new PPButton(MAINMENU_EDIT, screen, nullptr,
						  PPPoint(button->getLocation().x + button->getSize().width+1, button->getLocation().y),
						  PPSize((SMALL_BTN_SIZE>>1)-1, bHeight-1), true, true, false);

	button->setText("Rec");

	stoprecCont->addControl(button);

	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_ZAP))->setText("Zap");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_LOAD))->setText("Load");
	// Setup "Save" button
	button = dynamic_cast<PPButton*>(saveCont->getControlByID(MAINMENU_SAVE));
	button->setText("Save");
	button->setSize(PPSize((SMALL_BTN_SIZE>>1)-1, bHeight-1));
	// Add "Save As" button
	button = new PPButton(MAINMENU_SAVEAS, screen, nullptr,
						  PPPoint(button->getLocation().x + button->getSize().width+1, button->getLocation().y),
						  PPSize(SMALL_BTN_SIZE>>1, bHeight-1));

	button->setText("As" PPSTR_PERIODS);

	saveCont->addControl(button);

	//static_cast<PPButton*>(container->getControlByID(MAINMENU_SAVE));
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_DISKMENU))->setText("Disk Op.");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_INSEDIT))->setText("Ins. Ed.");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_SMPEDIT))->setText("Smp. Ed.");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_ADVEDIT))->setText("Adv. Edit");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_TRANSPOSE))->setText("Transpose");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_CONFIG))->setText("Config");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_QUICKOPTIONS))->setText("Options");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_OPTIMIZE))->setText("Optimize");
	dynamic_cast<PPButton*>(container->getControlByID(MAINMENU_ABOUT))->setText("About");

	// add/subtract channels
	button = new PPButton(BUTTON_MENU_ITEM_ADDCHANNELS, screen, nullptr, PPPoint(x+4 + 3*78, y + 3 + 3*bHeight), PPSize((SMALL_BTN_SIZE>>1) - 1, bHeight-1));
	button->setText("Add");
	addsubCont->addControl(button);

	button = new PPButton(BUTTON_MENU_ITEM_SUBCHANNELS, screen, nullptr, PPPoint(x+4 + 3*78 + (SMALL_BTN_SIZE>>1), y + 3 + 3*bHeight), PPSize((SMALL_BTN_SIZE>>1), bHeight-1));
	button->setText("Sub");
	addsubCont->addControl(button);
	
	container->addControl(addsubCont);

	button = dynamic_cast<PPButton*>(patposCont->getControlByID(MAINMENU_PLAY_PATTERN));
	button->setText("Pat");
	button->setSize(PPSize((SMALL_BTN_SIZE>>1)-1, bHeight-1));

	PPPoint p = button->getLocation();
	p.x+=button->getSize().width+1;

	button = new PPButton(MAINMENU_PLAY_POSITION, screen, nullptr, p, PPSize((SMALL_BTN_SIZE>>1), bHeight-1));
	button->setText("Pos");
	patposCont->addControl(button);

	screen->addControl(container);
}

void initApp() {
#if defined(__AMIGA__)
	Printf("Screen1!\n");
	screen = new PPScreen(nullptr, nullptr);
	Printf("Screen2!\n");
	/*MUI_NewObject(MUIC_Group,
		Child, (ULONG)(MUI_NewObject(MUIC_Group,
			MUIA_Group_Horiz, TRUE,
			MUIA_Group_SameSize, TRUE,
			Child, (ULONG)(MUI_NewObject(MUIC_Group,
				MUIA_Frame, MUIV_Frame_Group,
				MUIA_FrameTitle, (ULONG)"Info",
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"Mod:", TAG_END)),
					Child, (ULONG)(modFileString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"...", TAG_END)),
					TAG_END)),
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"BPM:", TAG_END)),
					Child, (ULONG)(bpmString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"", MUIA_String_InactiveContents, TRUE, TAG_END)),
					TAG_END)),
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"Speed:", TAG_END)),
					Child, (ULONG)(speedString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"", MUIA_String_InactiveContents, TRUE, TAG_END)),
					TAG_END)),
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"Pos:", TAG_END)),
					Child, (ULONG)(positionString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"", MUIA_String_InactiveContents, TRUE, TAG_END)),
					TAG_END)),
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"Row:", TAG_END)),
					Child, (ULONG)(rowString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"", MUIA_String_InactiveContents, TRUE, TAG_END)),
					TAG_END)),
				TAG_END)),
			Child, (ULONG)(MUI_NewObject(MUIC_Group,
				MUIA_Frame, MUIV_Frame_Group,
				MUIA_FrameTitle, (ULONG)"Settings",
				Child, (ULONG)(MUI_NewObject(MUIC_Group,
					MUIA_Group_Horiz, TRUE,
					Child, (ULONG)(MUI_MakeObject(MUIO_Label, (ULONG)"Driver:", TAG_END)),
					Child, (ULONG)(driverString = MUI_NewObject(MUIC_String, MUIA_String_Contents, (ULONG)"none", MUIA_String_InactiveContents, TRUE, TAG_END)),
					TAG_END)),
				TAG_END)),
			TAG_END)),
		Child, (ULONG)(MUI_NewObject(MUIC_Group,
			MUIA_Frame, MUIV_Frame_Group,
			MUIA_FrameTitle, (ULONG)"Controls",
			MUIA_Group_Horiz, TRUE,
				Child, (ULONG)(btnOpen = MUI_MakeObject(MUIO_Button, (ULONG)"_Open", TAG_END)),
				Child, (ULONG)(btnPlay = MUI_MakeObject(MUIO_Button, (ULONG)"_Play", TAG_END)),
				Child, (ULONG)(btnStop = MUI_MakeObject(MUIO_Button, (ULONG)"_Stop", TAG_END)),
			TAG_END)),
		TAG_END);*/

	/*
	struct Screen *myScreen;
	struct TagItem screenTags[] = {
		{ SA_Left, 0 },
		{ SA_Top, 0 },
		{ SA_Width, 640 },
		{ SA_Height, 480 },
		{ SA_Depth, 8 },
		{ SA_Title, (ULONG)"My New Screen" },
		{ SA_Type, PUBLICSCREEN },
		{ SA_SysFont, 1 },
		{ TAG_DONE, 0 }
	};
	myScreen = OpenScreenTagList(NULL, screenTags);
	*/

	win1 = MUI_NewObject(MUIC_Window,
						 MUIA_Window_Title, (ULONG)"MilkyTracker",
						 MUIA_Window_SizeGadget, FALSE,
						 MUIA_Window_RootObject, (ULONG)(screen->obj),
						 //MUIA_Window_Screen, (_sfdc_vararg)myScreen,
						 TAG_END);

	app = MUI_NewObject(MUIC_Application,
						MUIA_Application_Author, (ULONG)"AmigaDev Team",
						MUIA_Application_Base, (ULONG)"MilkyPlayer",
						MUIA_Application_Copyright, (ULONG)"© 2020-2021 Marlon Beijer",
						MUIA_Application_Description, (ULONG)"MilkyTracker in MUI.",
						MUIA_Application_Title, (ULONG)"MilkyTracker",
						MUIA_Application_Version, (ULONG)amiga_ver,
						MUIA_Application_Window, (ULONG)(win1),
						TAG_END);
	screen->app = app;
	Printf("Halloj1!\n");
	auto* containerAbout = new PPContainer(CONTAINER_ABOUT, screen, nullptr, PPPoint(116-2, 0), PPSize((306-116+2)+14,24), true);
	containerAbout->setColor(TrackerConfig::colorThemeMain);
	Printf("containerAbout\n");
	containerAbout->setRows(2);

	// Song title edit field
	PPListBox* listBox = new PPListBox(LISTBOX_SONGTITLE, screen, nullptr, PPPoint(116-2+2, 2+8), PPSize(200+2,12), true, true, false);
	listBox->showSelection(false);
	listBox->setSingleButtonClickEdit(true);
	listBox->setBorderColor(TrackerConfig::colorThemeMain);

	char str[MP_MAXTEXT+1] = "test";
	//moduleEditor->getTitle(str, ModuleEditor::MAX_TITLETEXT);

	listBox->addItem(str);
	//listBox->setMaxEditSize(ModuleEditor::MAX_TITLETEXT);

	containerAbout->addControl(listBox);

	PPStaticText* staticText /*= playTimeText*/ = new PPStaticText(STATICTEXT_ABOUT_TIME, screen, nullptr, PPPoint(116+2, 2+8+3), "", false, false);
	containerAbout->addControl(staticText);
	Printf("containerAbout2\n");
	
	PPButton* button = new PPButton(BUTTON_ABOUT_ESTIMATESONGLENGTH, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - 6*8-4, 2+8+2), PPSize(6*8, 9));
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("estimate");
	containerAbout->addControl(button);
/*
	peakLevelControl = new PeakLevelControl(PEAKLEVEL_CONTROL, screen, nullptr, PPPoint(116-2+2, 2+8), PPSize(200+2,12));
	peakLevelControl->setBorderColor(TrackerConfig::colorThemeMain);
	containerAbout->addControl(peakLevelControl);
*/

	staticText = new PPStaticText(STATICTEXT_ABOUT_HEADING, screen, nullptr, PPPoint(116, 3), "Song title:", true);
	staticText->setFont(PPFont::getFont(PPFont::FONT_TINY));
	containerAbout->addControl(staticText);
	
	// switch to Peak level
	pp_int32 aboutButtonOffset = 51;

	button = new PPButton(BUTTON_ABOUT_FOLLOWSONG, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset, 1), PPSize(12, 9), false, true, false);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("F");
	containerAbout->addControl(button);

	button = new PPButton(BUTTON_ABOUT_PROSPECTIVE, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset + 12, 1), PPSize(12, 9), false, true, false);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("P");
	button->setPressed(true);
	containerAbout->addControl(button);

	button = new PPButton(BUTTON_ABOUT_WRAPCURSOR, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset + 12*2, 1), PPSize(12, 9), false, true, false);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("W");
	containerAbout->addControl(button);

	button = new PPButton(BUTTON_ABOUT_LIVESWITCH, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset + 12*3, 1), PPSize(12, 9), false, true, false);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("L");
	containerAbout->addControl(button);

	aboutButtonOffset+=34;

	button = new PPButton(BUTTON_ABOUT_SHOWPEAK, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset, 1), PPSize(30, 9), false, true, false);
	button->setColor(TrackerConfig::colorThemeMain);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Peak");
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	containerAbout->addControl(button);

	aboutButtonOffset+=30;

	button = new PPButton(BUTTON_ABOUT_SHOWTIME, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset, 1), PPSize(30, 9), false, true, false);
	button->setColor(TrackerConfig::colorThemeMain);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Time");
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	containerAbout->addControl(button);

	aboutButtonOffset+=30;

	button = new PPButton(BUTTON_ABOUT_SHOWTITLE, screen, nullptr, PPPoint(containerAbout->getLocation().x + containerAbout->getSize().width - aboutButtonOffset, 1), PPSize(30, 9), false, true, false);
	button->setColor(TrackerConfig::colorThemeMain);
	button->setFont(PPFont::getFont(PPFont::FONT_TINY));
	button->setText("Title");
	button->setTextColor(PPUIConfig::getInstance()->getColor(PPUIConfig::ColorStaticText));
	containerAbout->addControl(button);
	Printf("Halloj2!\n");
	screen->addControl(containerAbout);
	Printf("Halloj3!\n");
	// Main options
	initSectionMainOptions(0, 64);

	// ---------- Instrument & Sample listboxes ---------- 
	//initListboxesSection(320, 0);
#else
	app = true;
#endif
}

#if !defined(__AMIGA__)
const char* GetFileName(const char* title) {
	return "test.mod";
}
#endif


#if defined(__AMIGA__)
ULONG xget(Object *obj,ULONG attribute)
{
	ULONG x;
	GetAttr(attribute, obj,&x);
	return(x);
}

void xset(Object *obj,ULONG attribute, ULONG str)
{
	SetAttrs(obj,attribute,str);
}

void setString(Object *obj, const char* string)
{
	xset(obj, MUIA_String_Contents, (ULONG)string);
}

char *getString(Object *obj) {
	return((char *)xget(obj,MUIA_String_Contents));
}
#endif

void setDriver(const char* driverName) {
	master->setCurrentDriverByName(driverName);
#if defined(__AMIGA__)
	setString(driverString, master->getCurrentDriverName());
#endif
}

int main(int argc,char *argv[]) {
#if defined(__AMIGA__)
	ULONG signals;
#endif
	BOOL running = TRUE;
	
	if (!Open_Libs())
	{
		Printf("Cannot open libs\n");
		return(0);
	}
	Printf("InitApp!\n");
	/* Init Application */
	initApp();

	if (!app)
	{
		Printf("Cannot create application.\n");
		return(0);
	}

	
	SDL_Init( SDL_INIT_AUDIO );
	atexit(SDL_Quit);

	/* Init MilkyPlay */
	initMilkyPlay();
	
	mp_sint32 i = 0;
	mp_sint32 selectedIndex = -1;
	const char* name = master->getFirstDriverName();
	setDriver(name);
	
	const char* curDrvName = master->getCurrentDriverName(); 
	while (name)
	{
		if (strcmp(name, curDrvName) == 0)
			selectedIndex = i;
		//listBox->addItem(name);
		name = master->getNextDriverName();
		i++;
	}
	
	bool* muteChannels = new bool[34];
	muteChannels[0] = muteChannels[1] = muteChannels[2] = muteChannels[3] = FALSE;
	
#if defined(__AMIGA__)
	DoMethod(win1, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod(btnOpen, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_OPEN);
	DoMethod(btnPlay, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_PLAY);
	DoMethod(btnStop, MUIM_Notify, MUIA_Pressed, FALSE,
			 app, 2, MUIM_Application_ReturnID, BTN_STOP);

	SetAttrs(win1, MUIA_Window_Open, TRUE, TAG_DONE);
#endif
	const char* file;
	bool play = false;

#if !defined(__AMIGA__)
	int id = 55;
#endif
	
	while(running) {
#if defined(__AMIGA__)
		ULONG id = DoMethod(app, MUIM_Application_Input, &signals);
#endif
		switch (id) {
#if defined(__AMIGA__)
			case MUIV_Application_ReturnID_Quit:
				if((MUI_RequestA(app,0,0,"Quit?","_Yes|_No","\33cAre you sure?",0)) == 1)
					running = FALSE;
				break;
#endif
			case BTN_OPEN:
			case MAINMENU_LOAD:
				play = false;
				controller->stop();
				file = GetFileName("Load mod");
				
				//Printf("Loading file %s\n",(ULONG)file);
				module->loadModule(file);
				controller->attachModuleEditor(module);
				//controller->resumePlayer(true);
				//controller->restartPlaying();
				play = true;
				break;
			case BTN_PLAY:
				play = true;
				if (module->isModuleLoaded()) {
					//controller->playSong(0,0, muteChannels);
					controller->resumePlayer(true);
					//controller->restartPlaying();
				}

				break;
			case BTN_STOP:
				play = false;
				controller->pause();
				break;
			default:
				//Printf("ID: %d\n", id);
				break;
		}
		
		if (module->isModuleLoaded() && !controller->isPlaying() && play) {
			controller->playSong(0,0, muteChannels);
			//controller->restartPlaying();
			controller->resumePlayer(true);
		}
#if defined(__AMIGA__)
/*
		if (module->isModuleLoaded() && controller->isPlaying()) {


			controller->getSpeed(BPM, speed);

			sprintf(out, "%d", BPM);
			setString(bpmString, out);

			sprintf(out, "%d", speed);
			setString(speedString, out);
			controller->getPosition(BPM, speed);
			
			sprintf(out, "%d", BPM);
			setString(positionString, out);
			
			sprintf(out, "%d", speed);
			setString(rowString, out);

			module->getTitle(out);
			setString(modFileString, out);

		}
*/

		if(running && signals) Wait(signals);
#endif
	}

#if defined(__AMIGA__)
	SetAttrs(win1, MUIA_Window_Open, FALSE);

	if(app) MUI_DisposeObject(app);
	Close_Libs();
#endif
	delete module;
	delete master;
	
	exit(EXIT_SUCCESS);
}

BOOL Open_Libs() {
#if defined(__AMIGA__)
	if ( !(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39)) )
		return(FALSE);

	if ( !(GfxBase=(struct GfxBase *) OpenLibrary("graphics.library",0)) ) {
		CloseLibrary((struct Library *)IntuitionBase);
		return(FALSE);
	}

	if ( !(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,19)) ) {
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return(FALSE);
	}
#endif

	return(TRUE);
}

void Close_Libs() {
#if defined(__AMIGA__)
if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);

	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);

	if (MUIMasterBase)
		CloseLibrary(MUIMasterBase);
#endif
}