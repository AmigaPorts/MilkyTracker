/*
 *  LoaderMED.cpp
 *  MilkyPlay Module Loader: OctaMED compatible
 */
#include <cstring>
#include <vector>
#include <map>
#include "Loaders.h"

//! Byte swap unsigned short
inline mp_uword SwapUInt16( mp_uword val )
{
	return (val << 8) | (val >> 8 );
}

//! Byte swap short
inline mp_sword SwapInt16( mp_sword val )
{
	return (val << 8) | ((val >> 8) & 0xFF);
}

//! Byte swap unsigned int
inline mp_uint32 SwapUInt32( mp_uint32 val )
{
	val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF );
	return (val << 16) | (val >> 16);
}

//! Byte swap int
inline mp_sint32 SwapInt32( mp_sint32 val )
{
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF );
	return (val << 16) | ((val >> 16) & 0xFFFF);
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	mp_sword_x(a)	SwapInt16((a))
#define	mp_uword_x(a)	SwapUInt16((a))
#define	mp_sint32_x(a)	SwapInt32((a))
#define	mp_uint32_x(a)	SwapUInt32((a))
#else
#define	mp_sword_x(a)		(a)
#define	mp_uword_x(a)		(a)
#define	mp_sint32_x(a)		(a)
#define	mp_uint32_x(a)		(a)
#endif

typedef struct
{
	char     mmd[3];               // "MMD" for the first song in file, "MCN" for the rest
	mp_ubyte  version;              // '0'-'3'
	mp_uint32 modLength;            // Size of file
	mp_uint32 songOffset;           // Position in file for the first song
	mp_uword playerSettings1[2];   // Internal variables for the play routine
	mp_uint32 blockArrOffset;       // Position in file for blocks (patterns)
	mp_ubyte  flags;
	mp_ubyte  reserved1[3];
	mp_uint32 sampleArrOffset;      // Position in file for samples (should be identical between songs)
	mp_uint32 reserved2;
	mp_uint32 expDataOffset;        // Absolute offset in file for ExpData (0 if not present)
	mp_uint32 reserved3;
	char     playerSettings2[11];  // Internal variables for the play routine
	mp_ubyte  extraSongs;           // Number of songs - 1
} *MMD0FileHeader;

struct MMD0Sample
{
	mp_uword loopStart;
	mp_uword loopLength;
	mp_ubyte  midiChannel;
	mp_ubyte  midiPreset;
	mp_ubyte  sampleVolume;
	mp_sbyte   sampleTranspose;
};

// Song header for MMD0/MMD1
struct MMD0Song
{
	mp_ubyte sequence[256];
};

// Song header for MMD2/MMD3
struct MMD2Song
{
	enum Flags3
	{
		FLAG3_STEREO  = 0x01,  // Mixing in stereo
		FLAG3_FREEPAN = 0x02,  // Mixing flag: free pan
	};

	mp_uint32 playSeqTableOffset;
	mp_uint32 sectionTableOffset;
	mp_uint32 trackVolsOffset;
	mp_uword numTracks;
	mp_uword numPlaySeqs;
	mp_uint32 trackPanOffset;  // 0: all centered (according to docs, MED Soundstudio uses Amiga hard-panning instead)
	mp_uint32 flags3;
	mp_uword volAdjust;       // Volume adjust (%)
	mp_uword mixChannels;     // Mixing channels, 0 means 4
	mp_ubyte  mixEchoType;     // 0 = nothing, 1 = normal, 2 = cross
	mp_ubyte  mixEchoDepth;    // 1 - 6, 0 = default
	mp_uword mixEchoLength;   // Echo length in milliseconds
	mp_sbyte   mixStereoSep;    // Stereo separation
	char     pad0[223];
};


// Common song header
struct MMDSong
{
	enum Flags
	{
		FLAG_FILTERON   = 0x01,  // The hardware audio filter is on
		FLAG_JUMPINGON  = 0x02,  // Mouse pointer jumping on
		FLAG_JUMP8TH    = 0x04,  // ump every 8th line (not in OctaMED Pro)
		FLAG_INSTRSATT  = 0x08,  // sng+samples indicator (not useful in MMDs)
		FLAG_VOLHEX     = 0x10,  // volumes are HEX
		FLAG_STSLIDE    = 0x20,  // use ST/NT/PT compatible sliding
		FLAG_8CHANNEL   = 0x40,  // this is OctaMED 5-8 channel song
		FLAG_SLOWHQ     = 0x80,  // HQ V2-4 compatibility mode
	};

	enum Flags2
	{
		FLAG2_BMASK = 0x1F,  // (bits 0-4) BPM beat length (in lines)
		FLAG2_BPM   = 0x20,  // BPM mode on
		FLAG2_MIX   = 0x80,  // Module uses mixing
	};

	mp_uword numBlocks;   // Number of blocks in current song
	mp_uword songLength;  // MMD0: Number of sequence numbers in the play sequence list, MMD2: Number of sections
	union
	{
		MMD0Song mmd0song;
		MMD2Song mmd2song;
	};
	mp_uword defaultTempo;
	mp_sbyte   playTranspose;  // The global play transpose value for current song
	mp_ubyte  flags;
	mp_ubyte  flags2;
	mp_ubyte  tempo2;        // Timing pulses per line (ticks)
	mp_ubyte  trackVol[16];  // 1...64 in MMD0/MMD1, reserved in MMD2
	mp_ubyte  masterVol;     // 1...64
	mp_ubyte  numSamples;
};


struct MMD2PlaySeq
{
	char     name[32];
	mp_uint32 commandTableOffset;
	mp_uint32 reserved;
	mp_uword length;  // Number of entries
};

struct MMD0PatternHeader
{
	mp_ubyte numTracks;
	mp_ubyte numRows;
};

struct MMD1PatternHeader
{
	mp_uword numTracks;
	mp_uword numRows;
	mp_uint32 blockInfoOffset;
};

struct MMDPlaySeqCommand
{
	enum Command
	{
		kStop = 1,
		kJump = 2,
	};

	mp_uword offset;   // Offset within current play sequence, 0xFFFF = end of list
	mp_ubyte  command;  // Stop = 1, Jump = 2
	mp_ubyte  extraSize;
};

struct MMDBlockInfo
{
	mp_uint32 highlightMaskOffset;
	mp_uint32 nameOffset;
	mp_uint32 nameLength;
	mp_uint32 pageTableOffset;    // File offset of command page table
	mp_uint32 cmdExtTableOffset;  // File offset of command extension table (second parameter)
	mp_uint32 reserved[4];
};

struct MMDInstrHeader
{
	enum Types
	{
		VSTI      = -4,
		HIGHLIFE  = -3,
		HYBRID    = -2,
		SYNTHETIC = -1,
		SAMPLE    =  0,  // an ordinary 1-octave sample (or MIDI)
		IFF5OCT   =  1,  // 5 octaves
		IFF3OCT   =  2,  // 3 octaves
		// The following ones are recognized by OctaMED Pro only
		IFF2OCT   = 3,  // 2 octaves
		IFF4OCT   = 4,  // 4 octaves
		IFF6OCT   = 5,  // 6 octaves
		IFF7OCT   = 6,  // 7 octaves
		// OctaMED Pro V5 + later
		EXTSAMPLE = 7,  // two extra-low octaves

		TYPEMASK  = 0x0F,

		S_16          = 0x10,
		STEREO        = 0x20,
		DELTA         = 0x40,
		PACKED        = 0x80,  // MMDPackedSampleHeader follows
		OBSOLETE_MD16 = 0x18,
	};

	mp_uint32 length;
	mp_sword  type;
};

struct MMDPackedSampleHeader
{
	mp_uword packType;    // Only 1 = ADPCM is supported
	mp_uword subType;     // Packing subtype
	// ADPCM subtype
	// 1: g723_40
	// 2: g721
	// 3: g723_24
	mp_ubyte commonFlags;  // flags common to all packtypes (none defined so far)
	mp_ubyte packerFlags;  // flags for the specific packtype
	mp_uint32 leftChLen;   // packed length of left channel in bytes
	mp_uint32 rightChLen;  // packed length of right channel in bytes (ONLY PRESENT IN STEREO SAMPLES)
};


struct MMDInstrExt
{
	enum
	{
		SSFLG_LOOP     = 0x01, // Loop On / Off
		SSFLG_EXTPSET  = 0x02, // Ext.Preset
		SSFLG_DISABLED = 0x04, // Disabled
		SSFLG_PINGPONG = 0x08, // Ping-pong looping
	};

	mp_ubyte  hold;   // 0...127
	mp_ubyte  decay;  // 0...127
	mp_ubyte  suppressMidiOff;
	mp_sbyte   finetune;
	// Below fields saved by >= V5
	mp_ubyte  defaultPitch;
	mp_ubyte  instrFlags;
	mp_uword longMidiPreset;
	// Below fields saved by >= V5.02
	mp_ubyte  outputDevice;
	mp_ubyte  reserved;
	// Below fields saved by >= V7
	mp_uint32 loopStart;
	mp_uint32 loopLength;
};


struct MMDInstrInfo
{
	char name[40];
};


struct MMD0Exp
{
	mp_uint32 nextModOffset;
	mp_uint32 instrExtOffset;
	mp_uword instrExtEntries;
	mp_uword instrExtEntrySize;
	mp_uint32 annoText;
	mp_uint32 annoLength;
	mp_uint32 instrInfoOffset;
	mp_uword instrInfoEntries;
	mp_uword instrInfoEntrySize;
	mp_uint32 jumpMask;
	mp_uint32 rgbTable;
	mp_ubyte  channelSplit[4];
	mp_uint32 notationInfoOffset;
	mp_uint32 songNameOffset;
	mp_uint32 songNameLength;
	mp_uint32 midiDumpOffset;
	mp_uint32 mmdInfoOffset;
	mp_uint32 arexxOffset;
	mp_uint32 midiCmd3xOffset;
	mp_uint32 trackInfoOffset;   // Pointer to song->numtracks pointers to tag lists
	mp_uint32 effectInfoOffset;  // Pointers to group pointers
	mp_uint32 tagEnd;
};



struct MMDTag
{
	enum TagType
	{
		// Generic MMD tags
		MMDTAG_END      = 0x00000000,
		MMDTAG_PTR      = 0x80000000,  // Data needs relocation
		MMDTAG_MUSTKNOW = 0x40000000,  // Loader must fail if this isn't recognized
		MMDTAG_MUSTWARN = 0x20000000,  // Loader must warn if this isn't recognized
		MMDTAG_MASK     = 0x1FFFFFFF,

		// ExpData tags
		// # of effect groups, including the global group (will override settings in MMDSong struct), default = 1
		MMDTAG_EXP_NUMFXGROUPS = 1,
		MMDTAG_TRK_FXGROUP     = 3,

		MMDTAG_TRK_NAME    = 1,  // trackinfo tags
		MMDTAG_TRK_NAMELEN = 2,  // namelen includes zero term.

		// effectinfo tags
		MMDTAG_FX_ECHOTYPE   = 1,
		MMDTAG_FX_ECHOLEN    = 2,
		MMDTAG_FX_ECHODEPTH  = 3,
		MMDTAG_FX_STEREOSEP  = 4,
		MMDTAG_FX_GROUPNAME  = 5,  // the Global Effects group shouldn't have name saved!
		MMDTAG_FX_GRPNAMELEN = 6,  // namelen includes zero term.
	};

	mp_uint32 type;
	mp_uint32 data;
};

static mp_sint32 mot2int(mp_sint32 x)
{
	return (x>>8)+((x&255)<<8);
}

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff  /* 4294967295U */
#endif

const mp_uint32 uint32_max = UINT32_MAX;

static bool ValidateHeader(const MMD0FileHeader &fileHeader)
{
	mp_uint32 songOffset = mp_uint32_x(fileHeader->songOffset);
	mp_uint32 blockArrOffset = mp_uint32_x(fileHeader->blockArrOffset);
	mp_uint32 sampleArrOffset = mp_uint32_x(fileHeader->sampleArrOffset);
	mp_uint32 expDataOffset = mp_uint32_x(fileHeader->expDataOffset);
	if(std::memcmp(fileHeader->mmd, "MMD", 3)
	   || fileHeader->version < '0' || fileHeader->version > '3'
	   || songOffset < sizeof(MMD0FileHeader)
	   || songOffset > uint32_max - 63 * sizeof(MMD0Sample) - sizeof(MMDSong)
	   || blockArrOffset < sizeof(MMD0FileHeader)
	   || sampleArrOffset < sizeof(MMD0FileHeader)
	   || expDataOffset > uint32_max - sizeof(MMD0Exp))
	{
		return false;
	}
	return true;
}


static mp_ubyte MMDTempoToBPM(mp_uint32 tempo, bool is8Ch, bool bpmMode, mp_ubyte rowsPerBeat)
{
	if(bpmMode)
	{
		// You would have thought that we could use modern tempo mode here.
		// Alas, the number of ticks per row still influences the tempo. :(
		return (tempo * rowsPerBeat) / 4.0;
	}
	if(is8Ch && tempo > 0)
	{
		//LimitMax(tempo, 10u);
		static const mp_ubyte tempos[10] = { 47, 43, 40, 37, 35, 32, 30, 29, 27, 26 };
		tempo = tempos[tempo - 1];
	} else if(tempo > 0 && tempo <= 10)
	{
		// SoundTracker compatible tempo
		return (6.0 * 1773447.0 / 14500.0) / tempo;
	}

	return tempo / 0.264;
}

const char* LoaderMED::identifyModule(const mp_ubyte* buffer)
{
	MMD0FileHeader fileHeader = (MMD0FileHeader)buffer;

	return !ValidateHeader(fileHeader) ? NULL : "MED";
}

static void MEDReadNextSong(XMFileBase &file, MMD0FileHeader &fileHeader, MMD0Exp &expData, MMDSong &songHeader)
{
	char block[2048];
	file.read(block, 1, 2048);

	fileHeader = (MMD0FileHeader)block;

	file.seek(mp_uint32_x(fileHeader->songOffset) + 63 * sizeof(MMD0Sample));
	file.read(&songHeader, 1, sizeof(MMDSong));
	if(fileHeader->expDataOffset) {
		file.seek(mp_uint32_x(fileHeader->expDataOffset));
		file.read(&expData, 1, sizeof(MMD0Exp));
	} else {
		expData = {};
	}
}


static mp_ubyte MEDScanNumChannels(XMFileBase &file, const mp_ubyte version)
{
	MMD0FileHeader fileHeader;
	MMD0Exp expData;
	MMDSong songHeader;

	file.seek(0);
	MEDReadNextSong(file, fileHeader, expData, songHeader);

	mp_ubyte numSongs = mp_uint32_x(fileHeader->expDataOffset) ? fileHeader->extraSongs + 1 : 1;

	mp_ubyte numChannels = 4;
	// Scan patterns for max number of channels
	for(mp_ubyte song = 0; song < numSongs; song++)
	{
		const mp_uword numPatterns = mp_uword_x(songHeader.numBlocks);
		if(songHeader.numSamples > 63 || numPatterns > 0x7FFF)
			return 0;

		for(mp_ubyte pat = 0; pat < numPatterns; pat++)
		{
			mp_uint32 newOffset = 0;
			file.seek(mp_uint32_x(fileHeader->blockArrOffset) + pat * 4u);
			file.read(&newOffset, 1, sizeof(mp_uint32));
			file.seek(mp_uint32_x(newOffset));

			numChannels = static_cast<mp_ubyte>(version < 1 ? file.readByte() : mp_uword_x(file.readWord()));
		}

		if(!expData.nextModOffset)
			break;

		file.seek(mp_uint32_x(expData.nextModOffset));
		
		MEDReadNextSong(file, fileHeader, expData, songHeader);
	}
	
	if (numChannels > 32) numChannels = 32; // Temp fix
	
	return numChannels;
}



mp_sint32 LoaderMED::load(XMFileBase& f, XModule* module)
{	
	enum ModuleTypes
	{
		ModuleTypeUnknown,
		ModuleTypeIns63,
	};

	module->cleanUp();

	// this will make code much easier to read
	TXMHeader*		header = &module->header;
	TXMInstrument*	instr  = module->instr;
	TXMSample*		smp	   = module->smp;
	TXMPattern*		phead  = module->phead;	
	
	// we're already out of memory here
	if (!phead || !instr || !smp)
		return MP_OUT_OF_MEMORY;	
	
	char block[2048];
	f.read(block, 1, 2048);

	const char* id = identifyModule((mp_ubyte*)block);

	MMD0FileHeader fileHeader = (MMD0FileHeader)block;
	
	if (!id)
		return MP_LOADER_FAILED;

	ModuleTypes moduleType = ModuleTypeUnknown;
	if (strcmp(id, "MED") == 0)
		moduleType = ModuleTypeIns63;

	if (moduleType == ModuleTypeUnknown)
		return MP_LOADER_FAILED;

	f.seek(0);
	
	//f.read(&header->name,1,20);

	//header->name = fileHeader->version;
	
	switch (moduleType)
	{
		case ModuleTypeIns63:
			header->insnum = 63;
			break;
		default:
			return MP_LOADER_FAILED;
	}
	
#ifdef VERBOSE
	printf("Loading...\n");
#endif

	f.seek(mp_uint32_x(fileHeader->songOffset));

	char sampleHeaderChunk[63 * sizeof(MMD0Sample)];
	
	f.read(sampleHeaderChunk,1,63 * sizeof(MMD0Sample));

	MMDSong songHeader;
	f.read(&songHeader, 1, sizeof(MMDSong));

	if(songHeader.numSamples > 63 || mp_uword_x(songHeader.numBlocks) > 0x7FFF)
		return false;

	MMD0Exp expData{};
	if(mp_uint32_x(fileHeader->expDataOffset))
	{
		f.seek(mp_uint32_x(fileHeader->expDataOffset));
		f.read(&expData,1, sizeof(MMD0Exp));
	}
	
	const mp_ubyte version = fileHeader->version - '0';
	
	header->channum = MEDScanNumChannels(f, version);
	if(header->channum < 1)
		return false;
	
	std::vector<mp_uint32> instrOffsets;
	f.seek(mp_uint32_x(fileHeader->sampleArrOffset));
	for (mp_sint32 i = 0; i < songHeader.numSamples; i++) {
		mp_uint32 tmpInt;
		f.read(&tmpInt, 1, sizeof(mp_uint32));
		instrOffsets.push_back(mp_uint32_x(tmpInt));
	}
	
	header->insnum = header->smpnum = songHeader.numSamples;

	// In MMD0 / MMD1, octave wrapping is only done in 4-channel modules (hardware mixing!), and not for synth instruments
	// - It's required e.g. for automatic terminated to.mmd0
	// - dissociate.mmd0 (8 channels) and starkelsesirap.mmd0 (synth) on the other hand don't need it
	// In MMD2 / MMD3, the mix flag is used instead.
	const bool hardwareMixSamples = (version < 2 && header->channum == 4) || (version >= 2 && !(songHeader.flags2 & MMDSong::FLAG2_MIX));

	bool needInstruments = false;
	bool anySynthInstrs = false;
	std::map<mp_ubyte, mp_ubyte> jumpTargets;
	mp_sint32 i, s = 1;
	for (i = 0; i < header->insnum; i++) {

		mp_uword instrNum = i;
		mp_ubyte insname[22];
		mp_uint32 smplen=0;
		mp_ubyte finetune=0;
		mp_ubyte vol=0;
		mp_uword loopstart=0;
		mp_uword looplen=0;

		MMDInstrHeader instrHeader{};
		if(instrOffsets[instrNum] != 0) {
			f.seek(instrOffsets[instrNum]);
			f.read(&instrHeader, 1, sizeof(MMDInstrHeader));
		}
		
		const bool isSynth = mp_sword_x(instrHeader.type) < 0;
		const size_t maskedType = static_cast<size_t>(mp_sword_x(instrHeader.type) & MMDInstrHeader::TYPEMASK);

		if(isSynth)
		{
			// TODO: Figure out synth instruments
			anySynthInstrs = true;
			//instr.AssignSample(0);
		}

		mp_ubyte numSamples = 1;
		static const mp_ubyte SamplesPerType[] = {1, 5, 3, 2, 4, 6, 7};
		if(!isSynth && maskedType < sizeof(SamplesPerType))
			numSamples = SamplesPerType[maskedType];

		if(numSamples > 1)
		{
			//static_assert(MAX_SAMPLES > 63 * 9, "Check IFFOCT multisample code");
			header->smpnum += numSamples - 1;
			needInstruments = true;
			static const mp_ubyte OctSampleMap[][8] =
					{
							{1, 1, 0, 0, 0, 0, 0, 0},  // 2
							{2, 2, 1, 1, 0, 0, 0, 0},  // 3
							{3, 3, 2, 2, 1, 0, 0, 0},  // 4
							{4, 3, 2, 1, 1, 0, 0, 0},  // 5
							{5, 4, 3, 2, 1, 0, 0, 0},  // 6
							{6, 5, 4, 3, 2, 1, 0, 0},  // 7
					};

			static const mp_sbyte OctTransposeMap[][8] =
					{
							{ 0, 0, -12, -12, -24, -36, -48, -60},  // 2
							{ 0, 0, -12, -12, -24, -36, -48, -60},  // 3
							{ 0, 0, -12, -12, -24, -36, -48, -60},  // 4
							{12, 0, -12, -24, -24, -36, -48, -60},  // 5
							{12, 0, -12, -24, -36, -48, -48, -60},  // 6
							{12, 0, -12, -24, -36, -48, -60, -72},  // 7
					};

			// TODO: Move octaves so that they align better (C-4 = lowest, we don't have access to the highest four octaves)
			for(int octave = 4; octave < 10; octave++)
			{
				for(int note = 0; note < 12; note++)
				{
					//instr.Keyboard[12 * octave + note] = smp + OctSampleMap[numSamples - 2][octave - 4];
					//instr.NoteMap[12 * octave + note] += OctTransposeMap[numSamples - 2][octave - 4];
				}
			}
		} else if(maskedType == MMDInstrHeader::EXTSAMPLE)
		{
			needInstruments = true;
			//instr.Transpose(-24);
		} else if(!isSynth && hardwareMixSamples)
		{
			for(int octave = 7; octave < 10; octave++)
			{
				for(int note = 0; note < 12; note++)
				{
					//instr.NoteMap[12 * octave + note] -= static_cast<mp_ubyte>((octave - 6) * 12);
				}
			}
		}

		MMD0Sample sampleHeader;
		memcpy(&sampleHeader, sampleHeaderChunk, sizeof(MMD0Sample));
		
		smplen = mp_uint32_x(instrHeader.length);
		loopstart = mp_uword_x(sampleHeader.loopStart)* 2;
		looplen = mp_uword_x(sampleHeader.loopLength);
		vol = sampleHeader.sampleVolume;
		mp_ubyte loopend = loopstart + mp_uword_x(sampleHeader.loopLength) * 2;
		//transpose = SwapBE16(sampleHeader->sampleTranspose);
		//midiChannel = sampleHeader->midiChannel;
		//midiPreset = sampleHeader->midiPreset;

		// midiChannel = 0xFF == midi instrument but with invalid channel, midiChannel = 0x00 == sample-based instrument?
		if(sampleHeader.midiChannel > 0 && sampleHeader.midiChannel <= 16)
		{
			//instr.nMidiChannel = sampleHeader.midiChannel - 1 + MidiFirstChannel;
			needInstruments = true;
		}


		if(sampleHeader.midiPreset > 0 && sampleHeader.midiPreset <= 128)
		{
			//instr.nMidiProgram = sampleHeader.midiPreset;
		}

		if(isSynth)// || !(loadFlags & loadSampleData))
		{
			s += numSamples;
			continue;
		}

		/*
		SampleIO sampleIO(
				SampleIO::_8bit,
				SampleIO::mono,
				SampleIO::bigEndian,
				SampleIO::signedPCM);*/
		
		mp_ubyte sampleIO = XModule::ST_DEFAULT;

		const bool hasLoop = sampleHeader.loopLength > 1;
		
		if(instrHeader.type & MMDInstrHeader::S_16)
		{
			sampleIO |= XModule::ST_16BIT;
			//smplen /= 2;
		}
		if (instrHeader.type & MMDInstrHeader::STEREO)
		{
			//sampleIO |= SampleIO::stereoSplit;
			//sampleIO = XModule::ST_;
			//smplen /= 2;
		}
		if(instrHeader.type & MMDInstrHeader::DELTA)
		{
			//sampleIO |= SampleIO::deltaPCM;
			sampleIO = sampleIO |= XModule::ST_DELTA;
		}

		if(numSamples > 1)
			smplen = smplen / ((1u << numSamples) - 1);

		
		for( mp_sword numSample = 0; numSample < numSamples; numSample++)
		{
			TXMSample* mptSmp = &module->smp[s];
			mptSmp->vol = XModule::vol64to255(sampleHeader.sampleVolume); // 4u * std::min<mp_ubyte>(sampleHeader.sampleVolume, 64u);
			mptSmp->relnote = sampleHeader.sampleTranspose;

			instr[instrNum].snum[numSample] = s;
			
			mptSmp->samplen = smplen;

			f.seek(instrOffsets[instrNum]+sizeof(MMDInstrHeader));
			
			if (instrHeader.type == MMDInstrHeader::SYNTHETIC || instrHeader.type == MMDInstrHeader::HYBRID) continue;

			mp_sint32 result = module->loadModuleSample(f, s, sampleIO);
			if (result != MP_OK)
				return result;

			if(hasLoop)
			{
				mptSmp->loopstart = loopstart;
				mptSmp->looplen = looplen;
				//instr[i].snum[s+i]
				//mptSmp->flags = 
				//mptSmp.uFlags.set(CHN_LOOP);
			}

			smplen *= 2;
			loopstart *= 2;
			loopend *= 2;
			s++;
		}

		//s += numSamples;
		instr[instrNum].samp = s;//numSamples;
	}

	if(mp_uint32_x(expData.instrInfoOffset) != 0 && mp_uword_x(expData.instrInfoEntries) != 0)
	{
		f.seek(mp_uint32_x(expData.instrInfoOffset));
		const mp_uword entries = std::min<mp_uword>(mp_uword_x(expData.instrInfoEntries), songHeader.numSamples);
		const mp_uword size = mp_uword_x(expData.instrInfoEntrySize);
		for( mp_uword e = 0; e < entries; e++)
		{
			MMDInstrInfo instrInfo;
			f.read(&instrInfo, 1, size);
			
			memcpy(module->instr[e].name, instrInfo.name, sizeof(module->instr[e].name));
			for(mp_sword j = 1; j <= module->instr[e].samp; j++)
			{
				mp_sword simp = module->instr[e].snum[j];
				memcpy(module->smp[simp].name, instrInfo.name, sizeof(module->smp[simp].name));
			}
		}
	}

	if(mp_uint32_x(expData.songNameOffset))
	{
		f.seek(mp_uint32_x(expData.songNameOffset));
		mp_ubyte songName[mp_uint32_x(expData.songNameLength)];
		f.read(songName, 1, mp_uint32_x(expData.songNameLength));
		/*if(numSongs > 1)
			order.SetName(mpt::ToUnicode(mpt::Charset::ISO8859_1, m_songName));*/
		
		memcpy(header->name, songName, sizeof(header->name));
	}
	
	mp_ubyte basePattern = 0;
	
	if(version < 2)
	{
		if(mp_uword_x(songHeader.songLength) > 256 || header->channum > 16)
			return false;
		
		memcpy(&header->ord, songHeader.mmd0song.sequence, sizeof(header->ord));
		header->ordnum = mp_uword_x(songHeader.songLength);
		
		//SetupMODPanning(true);
		for(mp_ubyte chn = 0; chn < header->channum; chn++)
		{
			//ChnSettings[chn].nVolume = std::min<uint8>(songHeader.trackVol[chn], 64);
		}
	} 
	/*else
	{
		const MMD2Song &header = songHeader.mmd2song;
		if(header.numTracks < 1 || header.numTracks > 64 || m_nChannels > 64)
			return false;

		const bool freePan = (header.flags3 & MMD2Song::FLAG3_FREEPAN);
		if(header.volAdjust)
			preamp = Util::muldivr_unsigned(preamp, std::min<uint16>(header.volAdjust, 800), 100);
		if (freePan)
			preamp /= 2;

		if(file.Seek(header.trackVolsOffset))
		{
			for(CHANNELINDEX chn = 0; chn < m_nChannels; chn++)
			{
				ChnSettings[chn].nVolume = std::min<uint8>(file.ReadUint8(), 64);
			}
		}
		if(header.trackPanOffset && file.Seek(header.trackPanOffset))
		{
			for(CHANNELINDEX chn = 0; chn < m_nChannels; chn++)
			{
				ChnSettings[chn].nPan = (Clamp<int8, int8>(file.ReadInt8(), -16, 16) + 16) * 8;
			}
		} else
		{
			SetupMODPanning(true);
		}

		std::vector<uint16be> sections;
		if(!file.Seek(header.sectionTableOffset)
		   || !file.CanRead(songHeader.songLength * 2)
		   || !file.ReadVector(sections, songHeader.songLength))
			continue;
		for(uint16 section : sections)
		{
			if(section > header.numPlaySeqs)
				continue;

			file.Seek(header.playSeqTableOffset + section * 4);
			if(file.Seek(file.ReadUint32BE()) || !file.CanRead(sizeof(MMD2PlaySeq)))
			{
				MMD2PlaySeq playSeq;
				file.ReadStruct(playSeq);

				if(!order.empty())
					order.push_back(order.GetIgnoreIndex());

				size_t readOrders = playSeq.length;
				if(!file.CanRead(readOrders))
					LimitMax(readOrders, file.BytesLeft());
				LimitMax(readOrders, ORDERINDEX_MAX);

				size_t orderStart = order.size();
				order.reserve(orderStart + readOrders);
				for(size_t ord = 0; ord < readOrders; ord++)
				{
					PATTERNINDEX pat = file.ReadUint16BE();
					if(pat < 0x8000)
					{
						order.push_back(basePattern + pat);
					}
				}
				if(playSeq.name[0])
					order.SetName(mpt::ToUnicode(mpt::Charset::ISO8859_1, playSeq.name));

				// Play commands (jump / stop)
				if(playSeq.commandTableOffset > 0 && file.Seek(playSeq.commandTableOffset))
				{
					MMDPlaySeqCommand command;
					while(file.ReadStruct(command))
					{
						FileReader chunk = file.ReadChunk(command.extraSize);
						ORDERINDEX ord = mpt::saturate_cast<ORDERINDEX>(orderStart + command.offset);
						if(command.offset == 0xFFFF || ord >= order.size())
							break;
						if(command.command == MMDPlaySeqCommand::kStop)
						{
							order[ord] = order.GetInvalidPatIndex();
						} else if(command.command == MMDPlaySeqCommand::kJump)
						{
							jumpTargets[ord] = chunk.ReadUint16BE();
							order[ord] = order.GetIgnoreIndex();
						}
					}
				}
			}
		}
	}*/
	
	header->patnum = mp_uword_x(songHeader.numBlocks);
	for (int pat = 0; pat < mp_uword_x(songHeader.numBlocks); pat++) {
		TXMPattern* pattern = &phead[pat];
		int transpose;
		pattern->rows = 0;
		pattern->effnum = 1;
		pattern->channum = (mp_ubyte)header->channum;

		mp_uint32 newOffset = 0;
		f.seek(mp_uint32_x(fileHeader->blockArrOffset) + pat * 4u);
		f.read(&newOffset, 1, sizeof(mp_uint32));
		f.seek(mp_uint32_x(newOffset));
		
		MMD1PatternHeader patHeader;
		f.read(&patHeader, 1, sizeof(MMD1PatternHeader));
		pattern->channum = mp_uword_x(patHeader.numTracks);
		pattern->rows = mp_uword_x(patHeader.numRows) + 1;
		transpose = /*NOTE_MIN*/1 + (version <= 2 ? 47 : 23) + songHeader.playTranspose;


		if(mp_uint32_x(patHeader.blockInfoOffset))
		{
			mp_uint32 offset = f.pos();
			f.seek(mp_uint32_x(patHeader.blockInfoOffset));
			MMDBlockInfo blockInfo;
			f.read(&blockInfo, 1, sizeof(MMDBlockInfo));
			
			f.seek(mp_uint32_x(blockInfo.nameOffset));
			mp_ubyte blockName[mp_uint32_x(blockInfo.nameLength)];
			f.read(&blockName, 1, sizeof(blockName));

			if(mp_uint32_x(blockInfo.cmdExtTableOffset))
			{
				f.seek(mp_uint32_x(blockInfo.cmdExtTableOffset));
				f.read(&newOffset, 1, sizeof(mp_uint32));
				f.seek(mp_uint32_x(newOffset));

				//cmdExt = file.ReadChunk(numTracks * numRows);
			}

			f.seek(offset);
		}

		pattern->patternData = new mp_ubyte[pattern->rows*header->channum *4];

		// out of memory?
		if (pattern->patternData == NULL)
		{
			return MP_OUT_OF_MEMORY;
		}

		memset(pattern->patternData,0,pattern->rows*header->channum *4);

		mp_sint32 r,c,cnt = 0;
		mp_sint32 offs = 0;
		for (r = 0; r < pattern->rows; r++)
		{
			for (c = 0; c < header->channum;c++)
			{
				mp_ubyte note;
				f.read(&note, 1, sizeof(mp_ubyte));
				
				mp_ubyte ins;
				f.read(&ins, 1, sizeof(mp_ubyte));

				mp_ubyte eff;
				f.read(&eff, 1, sizeof(mp_ubyte));
				
				mp_ubyte op;
				f.read(&op, 1, sizeof(mp_ubyte));
				

				if(note & 0x7F)
					note = (note & 0x7F) + transpose-12;
				else if(note == 0x80)
					note = XModule::NOTE_CUT;

				//ConvertMEDEffect(*m, header->channum == 8, true, rowsPerBeat, volHex);

				pattern->patternData[cnt] = note;
				pattern->patternData[cnt+1] = ins & 0x3F;
				pattern->patternData[cnt+2] = eff;
				pattern->patternData[cnt+3] = op;

				offs+=(pattern->effnum * 2 + 2);

				cnt+=4;
			}

		}
	
	}
	
	/*
	header->ordnum = f.readByte();
	
	f.read(&header->whythis1a,1,1);
	f.read(&header->ord,1,128);
	
	if (moduleType == ModuleTypeIns63)
		f.read(header->sig,1,4);
	
	if ((memcmp(header->sig+2,"CH",2) != 0 && 
		memcmp(header->sig+1,"CHN",3) != 0) ||
		moduleType == ModuleTypeIns15)
		header->flags = XModule::MODULE_PTNEWINSTRUMENT;
	
	header->patnum=0;
	for (i=0;i<128;i++)
		if (header->ord[i]>header->patnum) header->patnum=header->ord[i];
	
	header->patnum++;
	
	//patterns = new mp_ubyte*[modhead.numpatts];
	
	
	//mp_sint32 patternsize = modhead.numchannels*modhead.numrows*5;
	mp_sint32 modpatternsize = header->channum*64*4;
	
	mp_ubyte *buffer = new mp_ubyte[modpatternsize];
	
	if (buffer == NULL) 
	{
		return MP_OUT_OF_MEMORY;
	}
	
	for (i=0;i<header->patnum;i++) {
		f.read(buffer,1,modpatternsize);
		
		phead[i].rows=64;
		phead[i].effnum=1;
		phead[i].channum=(mp_ubyte)header->channum;
		
		phead[i].patternData=new mp_ubyte[phead[i].rows*header->channum*4];
		
		// out of memory?
		if (phead[i].patternData == NULL)
		{
			delete[] buffer;
			return MP_OUT_OF_MEMORY;
		}
		
		memset(phead[i].patternData,0,phead[i].rows*header->channum*4);
		
		mp_sint32 r,c,cnt=0;
		for (r=0;r<64;r++) {
			for (c=0;c<header->channum;c++) {
				mp_ubyte b1 = buffer[cnt];
				mp_ubyte b2 = buffer[cnt+1];
				mp_ubyte b3 = buffer[cnt+2];
				mp_ubyte b4 = buffer[cnt+3];
				
				mp_sint32 note,ins,eff,notenum = 0;
				note = ((b1&0xf)<<8)+b2;
				ins = (b1&0xf0)+(b3>>4);
				eff = b3&0xf;
				
				if (eff==0xE) {
					eff=(b4>>4)+0x30;
					b4&=0xf;
				}
				
				if ((!eff)&&b4) 
					eff=0x20;
				
				// old style modules don't support last effect for:
				// - portamento up/down
				// - volume slide
				if (eff==0x1&&(!b4)) eff = 0;
				if (eff==0x2&&(!b4)) eff = 0;
				if (eff==0xA&&(!b4)) eff = 0;

				if (eff==0x5&&(!b4)) eff = 0x3;
				if (eff==0x6&&(!b4)) eff = 0x4;
				
				if (eff==0xC) {
					b4 = XModule::vol64to255(b4);
				}
				
				if (note) 
					notenum = XModule::amigaPeriodToNote(note);

				phead[i].patternData[cnt]=notenum;
				phead[i].patternData[cnt+1]=ins;
				phead[i].patternData[cnt+2]=eff;
				phead[i].patternData[cnt+3]=b4;
				
				cnt+=4;
			}
		}
		
	}
	delete[] buffer;
	
	for (i=0; i < header->smpnum; i++) 
	{
		// Take a peek of the sample and check if we have to do some nasty MODPLUG ADPCM decompression
		bool adpcm = false;
		
		if (f.posWithBaseOffset() + 5 <= f.sizeWithBaseOffset())
		{
			f.read(block, 1, 5);
			adpcm = memcmp(block, "ADPCM", 5) == 0;
			if (!adpcm)
				f.seekWithBaseOffset(f.posWithBaseOffset() - 5);
		}
					
		mp_sint32 result = module->loadModuleSample(f, i, adpcm ? XModule::ST_PACKING_ADPCM : XModule::ST_DEFAULT);
		if (result != MP_OK)
			return result;
	}

*/


	const bool volHex = (songHeader.flags & MMDSong::FLAG_VOLHEX) != 0;
	const bool is8Ch = (songHeader.flags & MMDSong::FLAG_8CHANNEL) != 0;
	const bool bpmMode = (songHeader.flags2 & MMDSong::FLAG2_BPM) != 0;
	const mp_ubyte rowsPerBeat = 1 + (songHeader.flags2 & MMDSong::FLAG2_BMASK);
	header->speed = MMDTempoToBPM(mp_uword_x(songHeader.defaultTempo), is8Ch, bpmMode, rowsPerBeat);
	header->tempo = songHeader.tempo2;//Clamp<mp_ubyte, mp_ubyte>(songHeader.tempo2, 1, 32);
	
	if(bpmMode)
	{
		//m_nDefaultRowsPerBeat = rowsPerBeat;
		//m_nDefaultRowsPerMeasure = m_nDefaultRowsPerBeat * 4u;
	}

	if(songHeader.masterVol)
		header->mainvol = std::min<mp_ubyte>(songHeader.masterVol, 64) * 4;
	
	strcpy(header->tracker,"OctaMED");

	module->postLoadAnalyser();
	module->postProcessSamples();
	
#ifdef VERBOSE
	printf("%i / %i\n", f.pos(), f.size());
#endif

	return MP_OK;
}
