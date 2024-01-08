#include "AudioDriver_Arne.h"
#include "MasterMixer.h"

#define CUSTOM_DMACON2          (CUSTOM_DMACON  + 0x200)
#define CUSTOM_INTENA2          (CUSTOM_INTENA  + 0x200)

#define AUDIO_REGBASE(CH)       (CUSTOM_REGBASE + 0x400 + (((CH) & 0xf) << 4))
#define AUDIO_REG(CH, IDX)      (AUDIO_REGBASE(CH) + (IDX))

#define AUDIO_LOCHI(CH)         AUDIO_REG(CH, 0x00)
#define AUDIO_LOCLO(CH)         AUDIO_REG(CH, 0x02)
#define AUDIO_LENHI(CH)         AUDIO_REG(CH, 0x04)
#define AUDIO_LENLO(CH)         AUDIO_REG(CH, 0x06)
#define AUDIO_VOLUME(CH)        AUDIO_REG(CH, 0x08)
#define AUDIO_MODE(CH)          AUDIO_REG(CH, 0x0a)
#define AUDIO_PERIOD(CH)        AUDIO_REG(CH, 0x0c)

#define AUDIO_MODEF_16          (1<<0)
#define AUDIO_MODEF_ONESHOT     (1<<1)

#define DMA2F_AUDIO             0xfff

#define MAX_CHANNELS            16

AudioDriver_Arne::AudioDriver_Arne()
{
}

AudioDriver_Arne::~AudioDriver_Arne()
{
}

mp_sint32
AudioDriver_Arne::getChannels() const
{
    return MAX_CHANNELS;
}

const char*
AudioDriver_Arne::getDriverID()
{
    return "Apollo SAGA Arne 16-ch";
}

mp_sint32
AudioDriver_Arne::getPreferredBufferSize() const
{
    return 4096;
}

mp_sint32
AudioDriver_Arne::initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer)
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
    printf("Servus, my name is Arne.\n");
    printf("Wanted mix frequency: %ld\n", mixFrequency);
#endif

    mixFrequency = mixFrequency > 44100 ? 44100 : mixFrequency;

    return AudioDriver_Amiga::initDevice(bufferSizeInWords, mixFrequency, mixer);
}

// ---------------------------------------------------------------------------
// DirectOut
// ---------------------------------------------------------------------------

mp_sint32
AudioDriver_Arne_DirectOut::initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer)
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
    printf("Driver outputting in DirectOut mode\n");
#endif

    return AudioDriver_Arne::initDevice(bufferSizeInWords, mixFrequency, mixer);
}

bool
AudioDriver_Arne_DirectOut::isMultiChannel() const
{
    return true;
}

mp_sint32
AudioDriver_Arne_DirectOut::allocResources()
{
    // Fetch buffers for each channel (16-bit stereo)
    chanFetch = (mp_sword **) AllocMem(nChannels * sizeof(mp_sword *), MEMF_PUBLIC | MEMF_CLEAR);
    for(int i = 0; i < nChannels; i++)
        chanFetch[i] = (mp_sword *) AllocMem(fetchSize * sizeof(mp_sword) * MP_NUMCHANNELS, MEMF_PUBLIC | MEMF_CLEAR);

    // Ring buffers for each channel
    chanRing = (mp_sword **) AllocMem(nChannels * sizeof(mp_sword *), MEMF_PUBLIC | MEMF_CLEAR);
    for(int i = 0; i < nChannels; i++)
        chanRing[i] = (mp_sword *) AllocMem(ringSize * sizeof(mp_sword), MEMF_CHIP | MEMF_CLEAR);

    mixerProxy = new MixerProxyDirectOut(nChannels);

    return 0;
}

void
AudioDriver_Arne_DirectOut::deallocResources()
{
    delete mixerProxy;

    for(int i = 0; i < nChannels; i++)
        FreeMem(chanRing[i], ringSize * sizeof(mp_sword));
    FreeMem(chanRing, nChannels * sizeof(mp_sword *));

    for(int i = 0; i < nChannels; i++)
        FreeMem(chanFetch[i], fetchSize * sizeof(mp_sword) * MP_NUMCHANNELS);
    FreeMem(chanFetch, nChannels * sizeof(mp_sword *));
}

void
AudioDriver_Arne_DirectOut::initHardware()
{
    mp_uword period = PAULA_CLK / this->mixFrequency;

    for(int i = 0; i < MAX_CHANNELS; i++) {
        *((volatile mp_uint32 *) AUDIO_LOCHI(i)) = (mp_uint32) chanRing[i];
        *((volatile mp_uint32 *) AUDIO_LENHI(i)) = chunkSize >> 1;
        *((volatile mp_uword *) AUDIO_PERIOD(i)) = period;
        *((volatile mp_uword *) AUDIO_MODE(i)) = AUDIO_MODEF_16;
    }
}

void
AudioDriver_Arne_DirectOut::setGlobalVolume(mp_ubyte volume)
{
    for(int i = 0; i < MAX_CHANNELS; i++) {
        *((volatile mp_uword *) AUDIO_VOLUME(i)) = volume;
    }
}

void
AudioDriver_Arne_DirectOut::disableDMA()
{
    *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_AUDIO;
    *((volatile mp_uword *) CUSTOM_DMACON2) = DMA2F_AUDIO;
}

void
AudioDriver_Arne_DirectOut::enableDMA()
{
    *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_SETCLR | DMAF_AUDIO;
    *((volatile mp_uword *) CUSTOM_DMACON2) = DMAF_SETCLR | DMA2F_AUDIO;
}

void
AudioDriver_Arne_DirectOut::playAudioImpl()
{
    for(int i = 0; i < MAX_CHANNELS; i++) {
        *((volatile mp_uint32 *) AUDIO_LOCHI(i)) = (mp_uint32) (chanRing[i] + idxRead);
        *((volatile mp_uint32 *) AUDIO_LENHI(i)) = chunkSize >> 1;
    }
}

void
AudioDriver_Arne_DirectOut::bufferAudioImpl()
{
    if (isMixerActive()) {
        for(int i = 0; i < MAX_CHANNELS; i++)
            mixerProxy->setBuffer<mp_sword>(i, chanFetch[i]);
        mixer->mixerHandler(NULL, mixerProxy);

        for(int i = 0; i < MAX_CHANNELS; i++) {
            mp_sword * s = chanFetch[i];
            mp_sword * d = chanRing[i] + idxWrite;
            int l = -1;

            for(int j = 0; j < fetchSize; j++) {
                *(d++) = *(s++);
                s++;
            }
        }
    } else {
        for(int i = 0; i < MAX_CHANNELS; i++)
            memset(chanRing[i] + idxWrite, 0, fetchSize * sizeof(mp_sword));
    }
}

// ---------------------------------------------------------------------------
// ResampleHW
// ---------------------------------------------------------------------------

mp_sint32
AudioDriver_Arne_ResampleHW::initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer)
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
    printf("Driver outputting in ResampleHW mode\n");
#endif

    return AudioDriver_Arne::initDevice(bufferSizeInWords, mixFrequency, mixer);
}

bool
AudioDriver_Arne_ResampleHW::isMultiChannel() const
{
    return true;
}

mp_sint32
AudioDriver_Arne_ResampleHW::allocResources()
{
    mixerProxy = new MixerProxyHardwareOut(nChannels, this);
    zeroSample = (mp_sbyte *) AllocMem(16, MEMF_CHIP | MEMF_CLEAR);

    return 0;
}

void
AudioDriver_Arne_ResampleHW::deallocResources()
{
    FreeMem(zeroSample, 16);
    delete mixerProxy;
}

void
AudioDriver_Arne_ResampleHW::initHardware()
{
    newDMACON = 0;

    for(int i = 0; i < MAX_CHANNELS; i++) {
        channelLoopStart[i] = 0;
        channelRepeatLength[i] = 1;
        channelSampleExactPos[i] = 0.0f;
        channelSamplePos[i] = 0;
        channelExactPeriod[i] = 1.0f;
        channelPeriod[i] = 1;
    }
}

void
AudioDriver_Arne_ResampleHW::bufferAudioImpl()
{
    if(isMixerActive()) {
        mixer->mixerHandler(NULL, mixerProxy);
    }
}

void
AudioDriver_Arne_ResampleHW::bufferAudio()
{
}

void
AudioDriver_Arne_ResampleHW::disableDMA()
{
    custom.dmacon = DMAF_AUDIO;
    *((volatile mp_uword *) CUSTOM_DMACON2) = DMA2F_AUDIO;
}

void
AudioDriver_Arne_ResampleHW::disableIRQ()
{
    if(!irqEnabled)
        return;

    if(!wasExterIRQEnabled)
        custom.intena = INTF_EXTER;
    RemIntServer(INTB_EXTER, irqPlayAudio);

    // Restore CIA registers
    ciab.ciatalo = oldTimerALo;
    ciab.ciatahi = oldTimerAHi;
    ciab.ciatblo = oldTimerBLo;
    ciab.ciatbhi = oldTimerBHi;

    // Disable CIA interrupts
    ciab.ciaicr = CIAICRF_IR | CIAICRF_FLG | CIAICRF_SP | CIAICRF_ALRM | CIAICRF_TB | CIAICRF_TA;

    irqEnabled = false;
}

void
AudioDriver_Arne_ResampleHW::enableIRQ()
{
    if(irqEnabled)
        return;

    // Save regs
    oldTimerALo = ciab.ciatalo;
    oldTimerAHi = ciab.ciatahi;
    oldTimerBLo = ciab.ciatblo;
    oldTimerBHi = ciab.ciatbhi;

    // Disable CIA interrupts
    ciab.ciaicr = CIAICRF_IR | CIAICRF_FLG | CIAICRF_SP | CIAICRF_ALRM | CIAICRF_TB | CIAICRF_TA;

    // Reset Timer A
    ciab.ciacra = CIACRAF_LOAD | CIACRAF_START;
    ciab.ciatalo = (1773447/(125 * 5))&0xff;
    ciab.ciatahi = (1773447/(125 * 5))>>8;

    // Reset Timer B
    ciab.ciacrb = CIACRBF_LOAD;
    ciab.ciatblo = 576&0xff;
    ciab.ciatbhi = 576>>8;

    // Enable CIA interrupt
    ciab.ciaicr = CIAICRF_SETCLR | CIAICRF_TA | CIAICRF_TB;

    // Read current EXTER IRQ state
    wasExterIRQEnabled = (custom.intenar & INTF_EXTER) ? true : false;

    // Enable channel playback interrupt
    AddIntServer(INTB_EXTER, irqPlayAudio);
    if(!wasExterIRQEnabled)
        custom.intena = INTF_SETCLR | INTF_EXTER;
    irqEnabled = true;
}

void
AudioDriver_Arne_ResampleHW::playAudio()
{
    int i;

    // Read ICR (which clears the reg!)
    UBYTE icr = ciab.ciaicr;

    // If Timer-A has been fired
    if(icr & CIAICRF_TA) {
        bufferAudioImpl();
    }

    // If Timer-B has been fired
    if(icr & CIAICRF_TB) {
        if(newDMACON) {
            //
            // First round
            //

            // Enable one-shot timer to set repeat
            ciab.ciacrb = CIACRBF_LOAD | CIACRBF_RUNMODE | CIACRBF_START;

            // Enable DMA channels for 4 Paula legacy channels
            *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_SETCLR | (newDMACON & 0xf);

            // Enable DMA channels for 12 new Apollo channels
            *((volatile mp_uword *) CUSTOM_DMACON2) = DMAF_SETCLR | (newDMACON >> 4);

            newDMACON = 0;
        } else {
            //
            // Second round
            //

            // Set sample pointers and lengths
            for(i = 0; i < MAX_CHANNELS; i++) {
                *((volatile mp_uint32 *) AUDIO_LOCHI(i)) = channelLoopStart[i];
                *((volatile mp_uint32 *) AUDIO_LENHI(i)) = channelRepeatLength[i];
            }
        }
    }
}

void
AudioDriver_Arne_ResampleHW::setChannelFrequency(ChannelMixer::TMixerChannel * chn)
{
    //printf("ch %ld per = %ld\n", chn->index, chn->period);

    channelExactPeriod[chn->index] = (float) chn->period / 1024.0f;

    *((volatile mp_uword *) AUDIO_PERIOD(chn->index)) = (mp_uword) channelExactPeriod[chn->index];
    channelPeriod[chn->index] = (mp_uword) channelExactPeriod[chn->index];
}

void
AudioDriver_Arne_ResampleHW::setChannelVolume(ChannelMixer::TMixerChannel * chn)
{
    mp_sint32 voll = (chn->finalvoll >> 21) + 6;
    mp_sint32 volr = (chn->finalvolr >> 21) + 6;

    *((volatile mp_uword *) AUDIO_VOLUME(chn->index)) =
        (((voll >> 3) & 0xff) << 8) | ((volr >> 3) & 0xff);
}

void
AudioDriver_Arne_ResampleHW::playSample(ChannelMixer::TMixerChannel * chn)
{
    bool is16bit = chn->flags & 4;

    ciab.ciacrb = CIACRBF_LOAD;

    // Stop running sample
    if(chn->index >= 4) {
        *((volatile mp_uword *) CUSTOM_DMACON2) = DMAF_AUD0 << (chn->index - 4);
    } else {
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_AUD0 << chn->index;
    }

    // Get sample position
    mp_sint32 smppos = (chn->flags & ChannelMixer::MP_SAMPLE_RESTART) ? chn->smppos : channelSamplePos[chn->index];

    /*printf("ch %ld play = $%08lx smppos = $%08lx, $%08lx, $%08lx, loopend = $%08lx\n",
        chn->index, chn->sample, chn->smppos, smppos, hwChannelPos[chn->index], chn->loopend);*/

    // When end of sample is reached, either loop or stop
    if(smppos >= chn->loopend) {
        if ((chn->flags & 3) == 0) {
            stopSample(chn);
            return;
        } else {
            smppos = ((smppos - chn->loopstart) % (chn->loopend - chn->loopstart)) + chn->loopstart;
        }
    }

    // Set sample
    if(is16bit) {
        *((volatile mp_uint32 *) AUDIO_LOCHI(chn->index)) = (mp_uint32) (((mp_sword *) chn->sample) + smppos);
        *((volatile mp_uword *) AUDIO_MODE(chn->index)) = AUDIO_MODEF_16;
    } else {
        *((volatile mp_uint32 *) AUDIO_LOCHI(chn->index)) = (mp_uint32) (((mp_sbyte *) chn->sample) + smppos);
        *((volatile mp_uword *) AUDIO_MODE(chn->index)) = 0;
    }
    *((volatile mp_uint32 *) AUDIO_LENHI(chn->index)) = (mp_uint32) ((chn->loopend - smppos) >> 1);

    channelSampleExactPos[chn->index] = (float) smppos;
    channelSamplePos[chn->index] = smppos;

    setChannelFrequency(chn);
    setChannelVolume(chn);

    if ((chn->flags & 3) == 0) {
        channelLoopStart[chn->index] = (mp_uint32) zeroSample;
        channelRepeatLength[chn->index] = 1;
    } else {
        if(is16bit) {
            channelLoopStart[chn->index] = (mp_uint32) (((mp_sword *) chn->sample) + chn->loopstart);
        } else {
            channelLoopStart[chn->index] = (mp_uint32) (((mp_sbyte *) chn->sample) + chn->loopstart);
        }
        channelRepeatLength[chn->index] = (mp_uint32) ((chn->loopend - chn->loopstart) >> 1);
    }

    // And mark DMA channel as to be played
    newDMACON |= DMAF_AUD0 << chn->index;

    //
    // After processing plays and stops, this continues in tickDone
    // to enable the interrupt which enables the DMA
    //
}

void
AudioDriver_Arne_ResampleHW::stopSample(ChannelMixer::TMixerChannel * chn)
{
    //printf("ch %ld stop = %ld\n", chn->index, chn->sample);

    // Stop running sample
    if(chn->index >= 4) {
        *((volatile mp_uword *) CUSTOM_DMACON2) = DMAF_AUD0 << (chn->index - 4);
    } else {
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_AUD0 << chn->index;
    }

    channelSampleExactPos[chn->index] = 0.0f;
    channelSamplePos[chn->index] = 0;
    channelLoopStart[chn->index] = 0;
    channelRepeatLength[chn->index] = 1;
    channelExactPeriod[chn->index] = 1.0f;
    channelPeriod[chn->index] = 1;

    //
    // See playSample what happens next!
    //
}

void
AudioDriver_Arne_ResampleHW::tickDone(ChannelMixer::TMixerChannel * chn)
{
    int i;

    // Handle one-shot
    for(i = 0; i < MAX_CHANNELS; i++) {
        if((chn->flags & 3) == 0 && chn->flags & ChannelMixer::MP_SAMPLE_ONESHOT && channelSamplePos[i] >= chn->loopend) {
            chn->flags &= ~ChannelMixer::MP_SAMPLE_ONESHOT;
            chn->flags |= 1;
            chn->loopend = chn->loopendcopy;

            channelLoopStart[i] = (mp_uint32) chn->sample;
            channelRepeatLength[i] = (mp_uword) (((chn->loopend - chn->loopstart) >> 1) & 0xffff);
            channelSamplePos[i] = ((channelSamplePos[i] - chn->loopstart) % (chn->loopend - chn->loopstart)) + chn->loopstart;
            channelSampleExactPos[i] = (float) channelSamplePos[i];

            newDMACON |= DMAF_AUD0 << i;
        }

        chn++;
    }

    // If there are new samples playing on a channel, let irqEnableChannels enable the DMA for us
    if(newDMACON) {
        // Enable one-shot timer to set repeat
        ciab.ciacrb = CIACRBF_LOAD | CIACRBF_RUNMODE | CIACRBF_START;
    }

    // Record exact position inside sample
    for(i = 0; i < MAX_CHANNELS; i++) {
        //
        // Period is bound to Paula/Video clock !
        //
        float a = ((float) PAULA_CLK / 250.0f) / channelExactPeriod[i];
        channelSampleExactPos[i] += a;
        channelSamplePos[i] = (mp_sint32) channelSampleExactPos[i];
    }
}

int
AudioDriver_Arne_ResampleHW::getOperationFrequency()
{
    return 250;
}
