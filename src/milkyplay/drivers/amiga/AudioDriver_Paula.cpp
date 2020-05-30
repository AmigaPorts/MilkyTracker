#include "AudioDriver_Paula.h"
#include "MasterMixer.h"

#define AUDIO_REGBASE(CH)       (CUSTOM_REGBASE + 0x0a0 + ((CH & 0x3) << 4))
#define AUDIO_REG(CH, IDX)      (AUDIO_REGBASE(CH) + (IDX))

#define AUDIO_LOCHI(CH)         AUDIO_REG(CH, 0x00)
#define AUDIO_LOCLO(CH)         AUDIO_REG(CH, 0x02)
#define AUDIO_LENGTH(CH)        AUDIO_REG(CH, 0x04)
#define AUDIO_PERIOD(CH)        AUDIO_REG(CH, 0x06)
#define AUDIO_VOLUME(CH)        AUDIO_REG(CH, 0x08)

#define MAX_CHANNELS            4

#define SAMPLE_SIZE             MP_NUMBYTES


AudioDriver_Paula::AudioDriver_Paula()
{
}

AudioDriver_Paula::~AudioDriver_Paula()
{
}

mp_uint32
AudioDriver_Paula::getChannels() const
{
    return MAX_CHANNELS;
}

mp_uint32
AudioDriver_Paula::getSampleSize() const
{
    return SAMPLE_SIZE;
}

mp_sint32
AudioDriver_Paula::initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer)
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
    printf("Wanted mix frequency: %ld\n", mixFrequency);
    printf("Driver running in Paula mode\n");
#endif

    mixFrequency = 22050;

    return AudioDriver_Amiga::initDevice(bufferSizeInWords, mixFrequency, mixer);
}

void
AudioDriver_Paula::setGlobalVolume(mp_ubyte volume)
{
    int i;

    for(i = 0; i < MAX_CHANNELS; i++) {
        *((volatile mp_uword *) AUDIO_VOLUME(i)) = volume;
    }
}

void
AudioDriver_Paula::disableDMA()
{
    switch(outputMode) {
    case Mix:
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_AUD0 | DMAF_AUD1;
        break;
    default:
    case DirectOut:
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_AUDIO;
        break;
    }
}

void
AudioDriver_Paula::enableDMA()
{
    switch(outputMode) {
    case Mix:
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_SETCLR | DMAF_AUD0 | DMAF_AUD1;
        break;
    default:
    case DirectOut:
        *((volatile mp_uword *) CUSTOM_DMACON) = DMAF_SETCLR | DMAF_AUDIO;
        break;
    }
}

void
AudioDriver_Paula::initHardware()
{
    int i;

    // Initialize audio hardware with default values
    mp_uword period = PAULA_CLK / this->mixFrequency;

    switch(outputMode) {
    case Mix:
        *((volatile mp_uint32 *) AUDIO_LOCHI(0)) = (mp_uint32) samplesLeft;
        *((volatile mp_uword *) AUDIO_LENGTH(0)) = chunkSize >> 1;
        *((volatile mp_uword *) AUDIO_PERIOD(0)) = period;

        *((volatile mp_uint32 *) AUDIO_LOCHI(1)) = (mp_uint32) samplesRight;
        *((volatile mp_uword *) AUDIO_LENGTH(1)) = chunkSize >> 1;
        *((volatile mp_uword *) AUDIO_PERIOD(1)) = period;

        break;
    default:
    case DirectOut:
        for(i = 0; i < MAX_CHANNELS; i++) {
            *((volatile mp_uint32 *) AUDIO_LOCHI(i)) = (mp_uint32) chanRing[i];
            *((volatile mp_uword *) AUDIO_LENGTH(i)) = chunkSize >> 1;
            *((volatile mp_uword *) AUDIO_PERIOD(i)) = period;
        }

        break;
    }
}

void
AudioDriver_Paula::playAudioImpl()
{
    int i;

    switch(outputMode) {
    case Mix:
        *((volatile mp_uint32 *) AUDIO_LOCHI(0)) = (mp_uint32) (samplesLeft + idxRead);
        *((volatile mp_uword *) AUDIO_LENGTH(0)) = chunkSize >> 1;

        *((volatile mp_uint32 *) AUDIO_LOCHI(1)) = (mp_uint32) (samplesRight + idxRead);
        *((volatile mp_uword *) AUDIO_LENGTH(1)) = chunkSize >> 1;

        break;
    default:
    case DirectOut:
        for(i = 0; i < MAX_CHANNELS; i++) {
            *((volatile mp_uint32 *) AUDIO_LOCHI(i)) = (mp_uint32) (chanRing[i] + idxRead);
            *((volatile mp_uword *) AUDIO_LENGTH(i)) = chunkSize >> 1;
        }

        break;
    }
}

void
AudioDriver_Paula::bufferAudioImpl()
{
    int i, j;

    switch(outputMode) {
    case Mix:
        {
            mp_sword * f = samplesFetched;
            mp_sbyte
                * l = samplesLeft + idxWrite,
                * r = samplesRight + idxWrite;

            if (isMixerActive())
                mixer->mixerHandler(f);
            else
                memset(f, 0, fetchSize << 2);

            for(i = 0; i < fetchSize; i++) {
                *(l++) = *(f++) >> 8;
                *(r++) = *(f++) >> 8;
            }
        }
        break;
    default:
    case DirectOut:
        {
            if (isMixerActive()) {
                mixer->mixerHandler(NULL, MAX_CHANNELS, chanFetch);

                for(i = 0; i < MAX_CHANNELS; i++) {
                    mp_sword * s = chanFetch[i];
                    mp_sbyte * d = chanRing[i] + idxWrite;

                    for(j = 0; j < fetchSize; j++) {
                        *(d++) = *(s++) >> 8;
                    }
                }
            } else {
                for(i = 0; i < MAX_CHANNELS; i++) {
                    memset(chanRing[i] + idxWrite, 0, fetchSize * SAMPLE_SIZE);
                }
            }
        }
        break;
    }
}


const char*
AudioDriver_Paula::getDriverID()
{
    return "Commodore Paula Audio 4-ch";
}

mp_sint32
AudioDriver_Paula::getPreferredBufferSize() const
{
    return 2048;
}
