/*
 *  AudioDriver_PAMELA.h
 *  MilkyPlay
 *
 *  Created by neoman on 14.05.20
 *
 */
#ifndef __AUDIODRIVER_PAMELA_H__
#define __AUDIODRIVER_PAMELA_H__

#include "AudioDriver_Amiga.h"

class AudioDriver_Pamela : public AudioDriver_Amiga<mp_sword>
{
protected:
	virtual void        initHardware();
	virtual void        bufferAudioImpl();
	virtual void 		playAudioImpl();

	virtual mp_uint32   getChannels() const;
	virtual mp_uint32   getSampleSize() const;

	virtual void		setGlobalVolume(mp_ubyte volume);
	virtual void    	disableDMA();
	virtual void      	enableDMA();
public:
						AudioDriver_Pamela();
	virtual				~AudioDriver_Pamela();

	virtual	mp_sint32	initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer);

	virtual	const char*	getDriverID();
	virtual	mp_sint32	getPreferredBufferSize() const;
};

#endif
