#include "AudioDriver_Amiga.h"
#include "MasterMixer.h"

extern volatile struct Custom custom;

static mp_uint32
getVerticalBeamPosition() {
    struct vpos {
        mp_uint32 :13;
        mp_uint32 vpos:11;
        mp_uint32 hpos:8;
    };
    return ((struct vpos*) 0xdff004)->vpos;
}

static void
playAudioService(register AudioDriver_Amiga<void> * that __asm("a1"))
{
    that->playAudio();

    custom.intreq = INTF_AUD0;
}

template<typename SampleType>
AudioDriver_Amiga<SampleType>::AudioDriver_Amiga()
: AudioDriverInterface_Amiga()
, irqEnabled(false)
, irqAudioOld(NULL)
, allocated(false)
#if defined(AMIGA_DIRECTOUT)
, outputMode(DirectOut)
#else
, outputMode(Mix)
#endif
, statVerticalBlankMixMedian(0)
, statAudioBufferReset(0)
, statAudioBufferResetMedian(0)
, statRingBufferFull(0)
, statRingBufferFullMedian(0)
, statCountPerSecond(0)
{
}

template<typename SampleType>
AudioDriver_Amiga<SampleType>::~AudioDriver_Amiga()
{
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::initDevice(mp_sint32 bufferSizeInWords, mp_uint32 mixFrequency, MasterMixer* mixer)
{
	mp_sint32 res = AudioDriverBase::initDevice(bufferSizeInWords, mixFrequency, mixer);
	if (res < 0)
		return res;

    //            _BB__BDBCBSDAAAA
    // DMACON 23f0 010001111110000
    dmaconOld = custom.dmaconr;

    // Create interrupt for playback
    irqPlayAudio = (struct Interrupt *) AllocMem(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_CLEAR);
    irqPlayAudio->is_Node.ln_Type = NT_INTERRUPT;
    irqPlayAudio->is_Node.ln_Name = (char *) "mt-saga-play-irq";
    irqPlayAudio->is_Data = this;
    irqPlayAudio->is_Code = (void(*)()) playAudioService;

    // Zero volume & disable audio DMA
    setGlobalVolume(0);
    disableDMA();
    disableIRQ();

    // Disable lowpass filter and reset ADKCON
    *((volatile mp_ubyte *) CIAAPRA) |= CIAF_LED;
    custom.adkcon = 0xff;

    mp_sint32 newBufferSize = alloc(bufferSizeInWords);

#if DEBUG_DRIVER
    printf("Obtained mix frequency: %ld\n", mixFrequency);
    printf("Obtained buffer size: %ld\n", newBufferSize);
#endif

	return newBufferSize;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::alloc(mp_sint32 bufferSize)
{
    int i;

    sampleSize = getSampleSize();
    nChannels = getChannels();

    dealloc();

    // Force buffer size of 2^n
    for (mp_uint32 i = 0; i < 16; i++) {
        if ((unsigned)(1 << i) >= (unsigned)bufferSize) {
            bufferSize = 1 << i;
            break;
        }
    }

    // Number of samples to fetch from Milky's ChannelMixer
    // ! Needs to be the same as the requested size
    fetchSize = bufferSize;

    // Number of samples in ring buffer we write into *and* read from at the same time
    ringSize = fetchSize << 2;

    // Number of samples processed via DMA for each audio block
    chunkSize = fetchSize >> 2;

    // Reset ring buffer indices
    idxRead = 0;
    idxWrite = ringSize >> 1;

    // Reset stat values
    statVerticalBlankMixMedian = 0;
    statAudioBufferReset = 0;
    statAudioBufferResetMedian = 0;
    statRingBufferFull = 0;
    statRingBufferFullMedian = 0;
    statCountPerSecond = 0;

    switch(outputMode) {
    case Mix:
        // Fetch buffer for 16-bit stereo (4 byte a sample)
        samplesFetched = (mp_sword *) AllocMem(fetchSize << 2, MEMF_PUBLIC | MEMF_CLEAR);

        // Ring buffers for each side
        samplesLeft = (SampleType *) AllocMem(ringSize * sampleSize, MEMF_CHIP | MEMF_CLEAR);
        samplesRight = (SampleType *) AllocMem(ringSize * sampleSize, MEMF_CHIP | MEMF_CLEAR);

        break;
    default:
    case DirectOut:
        // Ring buffers for each channel
        chanFetch = (mp_sword **) AllocMem(nChannels * sizeof(mp_sword *), MEMF_PUBLIC | MEMF_CLEAR);
        chanRing = (SampleType **) AllocMem(nChannels * sizeof(SampleType *), MEMF_PUBLIC | MEMF_CLEAR);

        for(i = 0; i < nChannels; i++) {
            chanFetch[i] = (mp_sword *) AllocMem(fetchSize * sizeof(mp_sword), MEMF_PUBLIC | MEMF_CLEAR);
            chanRing[i] = (SampleType *) AllocMem(ringSize * sampleSize, MEMF_CHIP | MEMF_CLEAR);
        }

        chanRingPtrs = (SampleType **) AllocMem(nChannels * sizeof(SampleType *), MEMF_PUBLIC | MEMF_CLEAR);

        break;
    }

    allocated = true;

    initHardware();

    return bufferSize;
}

template<typename SampleType>
void
AudioDriver_Amiga<SampleType>::dealloc()
{
    int i;

    if(!allocated)
        return;
    allocated = false;

    switch(outputMode) {
    case Mix:
        FreeMem(samplesRight, ringSize * sampleSize);
        FreeMem(samplesLeft, ringSize * sampleSize);

        FreeMem(samplesFetched, fetchSize << 2);

        break;
    default:
    case DirectOut:
        FreeMem(chanRingPtrs, nChannels * sizeof(SampleType *));

        for(i = 0; i < nChannels; i++) {
            FreeMem(chanRing[i], ringSize * sampleSize);
            FreeMem(chanFetch[i], fetchSize * sizeof(mp_sword));
        }

        FreeMem(chanRing, nChannels * sizeof(SampleType *));
        FreeMem(chanFetch, nChannels * sizeof(mp_sword *));

        break;
    }
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::closeDevice()
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
#endif
    setGlobalVolume(0);
    disableDMA();
    disableIRQ();

    custom.dmacon = DMAF_SETCLR | dmaconOld;

    dealloc();

    FreeMem(irqPlayAudio, sizeof(struct Interrupt));

	return MP_OK;
}

template<typename SampleType>
void
AudioDriver_Amiga<SampleType>::playAudio()
{
    playAudioImpl();

    idxRead += chunkSize;
    if(idxRead >= ringSize)
        idxRead = 0;

    statAudioBufferReset++;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::bufferAudio()
{
    int i, j;

    statCountPerSecond++;
    if(statCountPerSecond == REFRESHRATE) {
        statCountPerSecond = 0;

        statAudioBufferResetMedian = (statAudioBufferResetMedian + statAudioBufferReset) >> 1;
        statAudioBufferReset = 0;

        statRingBufferFullMedian = (statRingBufferFullMedian + statRingBufferFull) >> 1;
        statRingBufferFull = 0;
    }

    // Fetch only if we would not write into the block to read
    mp_sint32 idxDist;
    if(idxRead <= idxWrite)
        idxDist = idxWrite - idxRead;
    else
        idxDist = (ringSize - idxRead) + idxWrite;

    if(idxDist >= fetchSize) {
        mp_uint32 vpos = getVerticalBeamPosition();

        bufferAudioImpl();

        idxWrite += fetchSize;
        if(idxWrite >= ringSize)
            idxWrite = 0;

        mp_uint32 mix = (((getVerticalBeamPosition() - vpos) * 1000000) / SCANLINES) / 10000;
        statVerticalBlankMixMedian = (statVerticalBlankMixMedian + mix) >> 1;
    } else {
        statRingBufferFull++;
    }

    return 1;
}

template<typename SampleType>
void
AudioDriver_Amiga<SampleType>::disableIRQ()
{
    custom.intena = INTF_AUD0;

    if(irqEnabled) {
        SetIntVector(INTB_AUD0, irqAudioOld);
        irqAudioOld = NULL;
        irqEnabled = false;
    }
}

template<typename SampleType>
void
AudioDriver_Amiga<SampleType>::enableIRQ()
{
    if(!irqEnabled) {
        irqAudioOld = SetIntVector(INTB_AUD0, irqPlayAudio);
        irqEnabled = true;
    }

    custom.intena = INTF_SETCLR | INTF_AUD0;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::start()
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
#endif
    setGlobalVolume(MAX_VOLUME);
    enableDMA();
    enableIRQ();

	return MP_OK;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::stop()
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
#endif
    setGlobalVolume(0);
    disableDMA();
    disableIRQ();

    return MP_OK;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::pause()
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
#endif
    disableDMA();

    return MP_OK;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::resume()
{
#if DEBUG_DRIVER
    printf("%s\n", __PRETTY_FUNCTION__);
#endif
    enableDMA();

    return MP_OK;
}

template<typename SampleType>
mp_sint32
AudioDriver_Amiga<SampleType>::getStatValue(mp_uint32 key)
{
    switch(key) {
    case 0:
        return statVerticalBlankMixMedian;
    case 1:
        return statAudioBufferResetMedian;
    case 2:
        return statRingBufferFullMedian;
    }

    return 0;
}

// Explicit specializations for signed 8-bit and 16-bit output
template class AudioDriver_Amiga<mp_sbyte>;
template class AudioDriver_Amiga<mp_sword>;
