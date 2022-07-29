/*
 *  ppui/CheckBox.cpp
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

#include "CheckBox.h"
#include "Button.h"
#include "GraphicsAbstract.h"
#include "Event.h"
#include "Screen.h"
#include "Font.h"

static const char* checked = "\xFF";
static const char* notChecked = "\x20";

PPCheckBox::PPCheckBox(pp_int32 id, PPScreen* parentScreen, EventListenerInterface* eventListener, 
					   const PPPoint& location, 
					   bool checked /* = true */) :
	PPControl(id, parentScreen, eventListener, location, PPSize(10,10))
{
#ifndef __AMIGA__
	button = new PPButton(id, parentScreen, this, location, this->size);
	button->setText(checked ? ::checked : ::notChecked);
#else
	this->obj = MUI_MakeObject(MUIO_Checkmark,(ULONG)NULL);
	SetAttrs(this->obj, MUIA_CycleChain, checked, TAG_END);
#endif
}

PPCheckBox::~PPCheckBox()
{
	delete button;
}

bool PPCheckBox::isChecked() const
{
#ifndef __AMIGA__
	return button->getText().compareTo(checked) == 0;
#else
	return checked; 
#endif
}

void PPCheckBox::checkIt(bool checked)
{
#ifndef __AMIGA__
	button->setText(checked ? ::checked : ::notChecked);
#else
	SetAttrs(this->obj, MUIA_CycleChain, checked, TAG_END);
#endif
}

// from control
void PPCheckBox::paint(PPGraphicsAbstract* graphics)
{
	if (!isVisible())
		return;
#ifndef __AMIGA__
	button->paint(graphics);
#endif
}
	
pp_int32 PPCheckBox::dispatchEvent(PPEvent* event)
{
	//if (!visible)
	//	return 0;
#ifndef __AMIGA__
	if (event->getID() == eLMouseDown)
	{
		button->dispatchEvent(event);
	}
	else if (event->getID() == eLMouseUp)
	{
		button->setText(isChecked() ? ::notChecked : ::checked);
		
		button->dispatchEvent(event);
	}
#endif
	
	parentScreen->paintControl(this);

	return 0;
}

pp_int32 PPCheckBox::handleEvent(PPObject* sender, PPEvent* event)
{
	return eventListener->handleEvent(reinterpret_cast<PPObject*>(this), event); 	
}

void PPCheckBox::enable(bool b)
{
	PPControl::enable(b);
#ifndef __AMIGA__
	button->enable(b);
#endif
}

void PPCheckBox::setSize(const PPSize& size)
{
	PPControl::setSize(size);
#ifndef __AMIGA__
	button->setSize(size);
#endif
}

void PPCheckBox::setLocation(const PPPoint& location)
{
	PPControl::setLocation(location);
#ifndef __AMIGA__
	button->setLocation(location);
#endif
}

