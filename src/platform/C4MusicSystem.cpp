/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 1998-2000, 2007  Matthes Bender
 * Copyright (c) 2001, 2005, 2007  Sven Eberhardt
 * Copyright (c) 2001  Carlo Teubner
 * Copyright (c) 2001  Michael Käser
 * Copyright (c) 2002-2003  Peter Wortmann
 * Copyright (c) 2005-2006, 2008-2009  Günther Brammer
 * Copyright (c) 2009  Nicolas Hake
 * Copyright (c) 2010  Martin Plicht
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

/* Handles Music.ocg and randomly plays songs */

#include <C4Include.h>
#include <C4MusicSystem.h>

#include <C4Window.h>
#include <C4MusicFile.h>
#include <C4Application.h>
#include <C4Random.h>
#include <C4Log.h>
#include <C4Game.h>
#include <C4GraphicsSystem.h>

#if defined HAVE_FMOD
#include <fmod_errors.h>
#elif defined HAVE_LIBSDL_MIXER
#include <SDL.h>
#endif

C4MusicSystem::C4MusicSystem():
		Songs(NULL),
		SongCount(0),
		PlayMusicFile(NULL),
		Volume(100)
#ifdef USE_OPEN_AL
		, alcDevice(NULL), alcContext(NULL)
#endif
{
}

C4MusicSystem::~C4MusicSystem()
{
	Clear();
}

#ifdef USE_OPEN_AL
void C4MusicSystem::SelectContext()
{
	alcMakeContextCurrent(alcContext);
}
#endif

bool C4MusicSystem::InitializeMOD()
{
#if defined HAVE_FMOD
	// init sequence according to docs
	unsigned int version;
	int numdrivers;
	FMOD_SPEAKERMODE speakermode;
	FMOD_CAPS caps;
	char name[256];
	/*
	Create a System object and initialize.
	*/
	if (FMOD::System_Create(&fmod_system) != FMOD_OK) { LogF("FMOD init error"); return false; }
	if (fmod_system->getVersion(&version) != FMOD_OK) { LogF("FMOD version error"); DeinitializeMOD(); return false; }
	if (version < FMOD_VERSION)
	{
		LogF("Error! You are using an old version of FMOD %08x. This program requires %08x\n", version, FMOD_VERSION);
		DeinitializeMOD(); return false;
	}
#ifdef _WIN32
	// Debug code
	void *init_data = NULL;
	switch (Config.Sound.FMMode)
	{
	case 0:
		fmod_system->setOutput(FMOD_OUTPUTTYPE_WINMM);
		break;
	case 1:
		fmod_system->setOutput(FMOD_OUTPUTTYPE_DSOUND);
		init_data = (void *)(Application.pWindow->hWindow);
		break;
	case 2:
		fmod_system->setOutput(FMOD_OUTPUTTYPE_DSOUND);
		break;
	}
	//
	if (fmod_system->getNumDrivers(&numdrivers) != FMOD_OK) { LogF("FMOD driver error"); DeinitializeMOD(); return false; }
	if (numdrivers == 0) { LogF("FMOD: 0 drivers."); DeinitializeMOD(); return false; }
	if (fmod_system->getDriverCaps(0, &caps, 0, &speakermode) != FMOD_OK) { LogF("FMOD driver caps error"); DeinitializeMOD(); return false; }
	/*
	Set the user selected speaker mode.
	*/
	if (fmod_system->setSpeakerMode(speakermode) != FMOD_OK) { LogF("FMOD setSpeakerMode error"); DeinitializeMOD(); return false; }
	if (caps & FMOD_CAPS_HARDWARE_EMULATED)
	{
		/*
		The user has the 'Acceleration' slider set to off! This is really bad
		for latency! You might want to warn the user about this.
		*/
		if (fmod_system->setDSPBufferSize(1024, 10) != FMOD_OK) { LogF("FMOD setDSPBufferSize error"); DeinitializeMOD(); return false; }
	}
	if (fmod_system->getDriverInfo(0, name, 256, 0) != FMOD_OK) { LogF("FMOD getDriverInfo error"); DeinitializeMOD(); return false; }
	if (strstr(name, "SigmaTel"))
	{
		/*
		Sigmatel sound devices crackle for some reason if the format is PCM 16bit.
		PCM floating point output seems to solve it.
		*/
		if (fmod_system->setSoftwareFormat(48000, FMOD_SOUND_FORMAT_PCMFLOAT, 0,0,FMOD_DSP_RESAMPLER_LINEAR) != FMOD_OK) { LogF("FMOD getDriverInfo error"); DeinitializeMOD(); return false; }
	}
#endif
	FMOD_RESULT result = fmod_system->init(100, FMOD_INIT_NORMAL, init_data);
#ifdef _WIN32
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		/*
		Ok, the speaker mode selected isn't supported by this soundcard. Switch it
		back to stereo...
		*/
		if (fmod_system->setSpeakerMode(FMOD_SPEAKERMODE_STEREO) != FMOD_OK)
			 { LogF("FMOD setSpeakerMode error"); DeinitializeMOD(); return false; }
		/*
		... and re-init.
		*/
		result = fmod_system->init(100, FMOD_INIT_NORMAL, init_data);
	}
#endif
	if (result != FMOD_OK)
		{ LogF("FMod: %s", FMOD_ErrorString(result)); DeinitializeMOD(); return false; }
	// ok
	MODInitialized = true;
	return true;
#elif defined HAVE_LIBSDL_MIXER
	SDL_version compile_version;
	const SDL_version * link_version;
	MIX_VERSION(&compile_version);
	link_version=Mix_Linked_Version();
	LogF("SDL_mixer runtime version is %d.%d.%d (compiled with %d.%d.%d)",
	     link_version->major, link_version->minor, link_version->patch,
	     compile_version.major, compile_version.minor, compile_version.patch);
	if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE))
	{
		LogF("SDL: %s", SDL_GetError());
		return false;
	}
	//frequency, format, stereo, chunksize
	if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024))
	{
		LogF("SDL_mixer: %s", SDL_GetError());
		return false;
	}
	MODInitialized = true;
	return true;
#elif defined(USE_OPEN_AL)
	alcDevice = alcOpenDevice(NULL);
	if (!alcDevice)
		return false;
	alcContext = alcCreateContext(alcDevice, NULL);
	if (!alcContext)
		return false;
	return true;
#endif
	return false;
}

void C4MusicSystem::DeinitializeMOD()
{
#if defined HAVE_FMOD
	if (fmod_system)
	{
		fmod_system->release(); fmod_system = NULL;
	}
#elif defined HAVE_LIBSDL_MIXER
	Mix_CloseAudio();
	SDL_Quit();
#elif defined(USE_OPEN_AL)
	alcDestroyContext(alcContext);
	alcCloseDevice(alcDevice);
	alcContext = NULL;
	alcDevice = NULL;
#endif
	MODInitialized = false;
}

bool C4MusicSystem::Init(const char * PlayList)
{
	// init mod
	if (!MODInitialized && !InitializeMOD()) return false;

	// Might be reinitialisation
	ClearSongs();
	// Global music file
	LoadDir(Config.AtSystemDataPath(C4CFN_Music));
	// User music file
	LoadDir(Config.AtUserDataPath(C4CFN_Music));
	// read MoreMusic.txt
	LoadMoreMusic();
	// set play list
	if (PlayList) SetPlayList(PlayList); else SetPlayList(0);
	// set initial volume
	SetVolume(Config.Sound.MusicVolume);

	// ok
	return true;
}

bool C4MusicSystem::InitForScenario(C4Group & hGroup)
{
	// check if the scenario contains music
	bool fLocalMusic = false;
	StdStrBuf MusicDir;
	if (GrpContainsMusic(hGroup))
	{
		// clear global songs
		ClearSongs();
		fLocalMusic = true;
		// add songs
		MusicDir.Take(Game.ScenarioFile.GetFullName());
		LoadDir(MusicDir.getData());
		// log
		LogF(LoadResStr("IDS_PRC_LOCALMUSIC"), MusicDir.getData());
	}
	// check for music folders in group set
	C4Group *pMusicFolder = NULL;
	while ((pMusicFolder = Game.GroupSet.FindGroup(C4GSCnt_Music, pMusicFolder)))
	{
		if (!fLocalMusic)
		{
			// clear global songs
			ClearSongs();
			fLocalMusic = true;
		}
		// add songs
		MusicDir.Take(pMusicFolder->GetFullName());
		MusicDir.AppendChar(DirectorySeparator);
		MusicDir.Append(C4CFN_Music);
		LoadDir(MusicDir.getData());
		// log
		LogF(LoadResStr("IDS_PRC_LOCALMUSIC"), MusicDir.getData());
	}
	// no music?
	if (!SongCount) return false;
	// set play list
	SetPlayList(0);
	// ok
	return true;
}

void C4MusicSystem::Load(const char *szFile)
{
	// safety
	if (!szFile || !*szFile) return;
	C4MusicFile *NewSong=NULL;
	// get extension
#if defined HAVE_FMOD
	const char *szExt = GetExtension(szFile);
	// get type
	switch (GetMusicFileTypeByExtension(GetExtension(szFile)))
	{
	case MUSICTYPE_MOD:
		if (MODInitialized) NewSong = new C4MusicFileFMOD;
		break;
	case MUSICTYPE_MP3:
#ifdef USE_MP3
		if (MODInitialized) NewSong = new C4MusicFileFMOD;
#endif
		break;
	case MUSICTYPE_OGG:
		if (MODInitialized) NewSong = new C4MusicFileFMOD;
		break;

	case MUSICTYPE_MID:
		if (MODInitialized)
			NewSong = new C4MusicFileFMOD;
		break;
	default: return; // safety
	}
#elif defined HAVE_LIBSDL_MIXER
	if (GetMusicFileTypeByExtension(GetExtension(szFile)) == MUSICTYPE_UNKNOWN) return;
	NewSong = new C4MusicFileSDL;
#endif
	// unrecognized type/mod not initialized?
	if (!NewSong) return;
	// init music file
	NewSong->Init(szFile);
	// add song to list (push back)
	C4MusicFile *pCurr = Songs;
	while (pCurr && pCurr->pNext) pCurr = pCurr->pNext;
	if (pCurr) pCurr->pNext = NewSong; else Songs = NewSong;
	NewSong->pNext = NULL;
	// count songs
	SongCount++;
}

void C4MusicSystem::LoadDir(const char *szPath)
{
	char Path[_MAX_FNAME + 1], File[_MAX_FNAME + 1];
	C4Group *pDirGroup = NULL;
	// split path
	SCopy(szPath, Path, _MAX_FNAME);
	char *pFileName = GetFilename(Path);
	SCopy(pFileName, File);
	*(pFileName - 1) = 0;
	// no file name?
	if (!File[0])
		// -> add the whole directory
		SCopy("*", File);
	// no wildcard match?
	else if (!SSearch(File, "*?"))
	{
		// then it's either a file or a directory - do the test with C4Group
		pDirGroup = new C4Group();
		if (!pDirGroup->Open(szPath))
		{
			// so it must be a file
			if (!pDirGroup->Open(Path))
			{
				// -> file/dir doesn't exist
				LogF("Music File not found: %s", szPath);
				delete pDirGroup;
				return;
			}
			// mother group is open... proceed with normal handling
		}
		else
		{
			// ok, set wildcard (load the whole directory)
			SCopy(szPath, Path);
			SCopy("*", File);
		}
	}
	// open directory group, if not already done so
	if (!pDirGroup)
	{
		pDirGroup = new C4Group();
		if (!pDirGroup->Open(Path))
		{
			LogF("Music File not found: %s", szPath);
			delete pDirGroup;
			return;
		}
	}
	// search file(s)
	char szFile[_MAX_FNAME + 1];
	pDirGroup->ResetSearch();
	while (pDirGroup->FindNextEntry(File, szFile))
	{
		char strFullPath[_MAX_FNAME + 1];
		sprintf(strFullPath, "%s%c%s", Path, DirectorySeparator, szFile);
		Load(strFullPath);
	}
	// free it
	delete pDirGroup;
}

void C4MusicSystem::LoadMoreMusic()
{
	StdStrBuf MoreMusicFile;
	// load MoreMusic.txt
	if (!MoreMusicFile.LoadFromFile(Config.AtUserDataPath(C4CFN_MoreMusic))) return;
	// read contents
	char *pPos = MoreMusicFile.getMData();
	while (pPos && *pPos)
	{
		// get line
		char szLine[1024 + 1];
		SCopyUntil(pPos, szLine, '\n', 1024);
		pPos = strchr(pPos, '\n'); if (pPos) pPos++;
		// remove leading whitespace
		char *pLine = szLine;
		while (*pLine == ' ' || *pLine == '\t' || *pLine == '\r') pLine++;
		// and whitespace at end
		char *p = pLine + strlen(pLine) - 1;
		while (*p == ' ' || *p == '\t' || *p == '\r') { *p = 0; --p; }
		// comment?
		if (*pLine == '#')
		{
			// might be a "directive"
			if (SEqual(pLine, "#clear"))
				ClearSongs();
			continue;
		}
		// try to load file(s)
		LoadDir(pLine);
	}
}

void C4MusicSystem::ClearSongs()
{
	Stop();
	while (Songs)
	{
		C4MusicFile *pFile = Songs;
		Songs = pFile->pNext;
		delete pFile;
	}
	SongCount = 0;
}

void C4MusicSystem::Clear()
{
#ifdef HAVE_LIBSDL_MIXER
	// Stop a fadeout
	Mix_HaltMusic();
#endif
	ClearSongs();
	if (MODInitialized) { DeinitializeMOD(); }
}

void C4MusicSystem::Execute()
{
#ifndef HAVE_LIBSDL_MIXER
	if (!::Game.iTick35)
#endif
	{
		if (!PlayMusicFile)
			Play();
		else
			PlayMusicFile->CheckIfPlaying();
	}
}

bool C4MusicSystem::Play(const char *szSongname, bool fLoop)
{
	if (Game.IsRunning ? !Config.Sound.RXMusic : !Config.Sound.FEMusic)
		return false;

	C4MusicFile* NewFile = NULL;

	// Specified song name
	if (szSongname && szSongname[0])
	{
		// Search in list
		for (NewFile=Songs; NewFile; NewFile = NewFile->pNext)
		{
			char songname[_MAX_FNAME+1];
			SCopy(szSongname, songname); DefaultExtension(songname, "mid");
			if (SEqual(GetFilename(NewFile->FileName), songname))
				break;
			SCopy(szSongname, songname); DefaultExtension(songname, "ogg");
			if (SEqual(GetFilename(NewFile->FileName), songname))
				break;
		}
	}

	// Random song
	else
	{
		// try to find random song
		for (int i = 0; i <= 1000; i++)
		{
			int nmb = SafeRandom(Max(ASongCount / 2 + ASongCount % 2, ASongCount - SCounter));
			int j;
			for (j = 0, NewFile = Songs; NewFile; NewFile = NewFile->pNext)
				if (!NewFile->NoPlay)
					if (NewFile->LastPlayed == -1 || NewFile->LastPlayed < SCounter - ASongCount / 2)
					{
						j++;
						if (j > nmb) break;
					}
			if (NewFile) break;
		}

	}

	// File found?
	if (!NewFile)
		return false;

	// Stop old music
	Stop();

	LogF(LoadResStr("IDS_PRC_PLAYMUSIC"), GetFilename(NewFile->FileName));

	// Play new song
	if (!NewFile->Play(fLoop)) return false;
	PlayMusicFile = NewFile;
	NewFile->LastPlayed = SCounter++;
	Loop = fLoop;

	// Set volume
	PlayMusicFile->SetVolume(Volume);

	return true;
}

void C4MusicSystem::NotifySuccess()
{
	// nothing played?
	if (!PlayMusicFile) return;
	// loop?
	if (Loop)
		if (PlayMusicFile->Play())
			return;
	// stop
	Stop();
}

void C4MusicSystem::FadeOut(int fadeout_ms)
{
	if (PlayMusicFile)
	{
		PlayMusicFile->Stop(fadeout_ms);
	}
}

bool C4MusicSystem::Stop()
{
	if (PlayMusicFile)
	{
		PlayMusicFile->Stop();
		PlayMusicFile=NULL;
	}
	return true;
}

int C4MusicSystem::SetVolume(int iLevel)
{
	if (iLevel > 100) iLevel = 100;
	if (iLevel < 0) iLevel = 0;
	// Save volume for next file
	Volume = iLevel;
	// Tell it to the act file
	if (PlayMusicFile)
		PlayMusicFile->SetVolume(iLevel);
	return iLevel;
}

MusicType GetMusicFileTypeByExtension(const char* ext)
{
	if (SEqualNoCase(ext, "mid"))
		return MUSICTYPE_MID;
#if defined HAVE_FMOD || defined HAVE_LIBSDL_MIXER
	else if (SEqualNoCase(ext, "xm") || SEqualNoCase(ext, "it") || SEqualNoCase(ext, "s3m") || SEqualNoCase(ext, "mod"))
		return MUSICTYPE_MOD;
#ifdef USE_MP3
	else if (SEqualNoCase(ext, "mp3"))
		return MUSICTYPE_MP3;
#endif
#endif
	else if (SEqualNoCase(ext, "ogg"))
		return MUSICTYPE_OGG;
	return MUSICTYPE_UNKNOWN;
}

bool C4MusicSystem::GrpContainsMusic(C4Group &rGrp)
{
	// search for known file extensions
	return           rGrp.FindEntry("*.mid")
#ifdef USE_MP3
	                 || rGrp.FindEntry("*.mp3")
#endif
	                 || rGrp.FindEntry("*.xm")
	                 || rGrp.FindEntry("*.it")
	                 || rGrp.FindEntry("*.s3m")
	                 || rGrp.FindEntry("*.mod")
	                 || rGrp.FindEntry("*.ogg");
}

int C4MusicSystem::SetPlayList(const char *szPlayList)
{
	// reset
	C4MusicFile *pFile;
	for (pFile = Songs; pFile; pFile = pFile->pNext)
	{
		pFile->NoPlay = true;
		pFile->LastPlayed = -1;
	}
	ASongCount = 0;
	SCounter = 0;
	if (szPlayList && *szPlayList)
	{
		// match
		char szFileName[_MAX_FNAME + 1];
		for (int cnt = 0; SGetModule(szPlayList, cnt, szFileName, _MAX_FNAME); cnt++)
			for (pFile = Songs; pFile; pFile = pFile->pNext)
				if (pFile->NoPlay && WildcardMatch(szFileName, GetFilename(pFile->FileName)))
				{
					ASongCount++;
					pFile->NoPlay = false;
				}
	}
	else
	{
		// default: all files except the ones beginning with an at ('@')
		// Ignore frontend and credits music
		for (pFile = Songs; pFile; pFile = pFile->pNext)
			if (*GetFilename(pFile->FileName) != '@' &&
			    !SEqual2(GetFilename(pFile->FileName), "Credits.") &&
			    !SEqual2(GetFilename(pFile->FileName), "Frontend."))
			{
				ASongCount++;
				pFile->NoPlay = false;
			}
	}
	return ASongCount;
}

bool C4MusicSystem::ToggleOnOff()
{
	// // command key for music toggle pressed
	// use different settings for game/menu (lobby also counts as "menu", so go by Game.IsRunning-flag rather than startup)
	if (Game.IsRunning)
	{
		// game music
		Config.Sound.RXMusic = !Config.Sound.RXMusic;
		if (!Config.Sound.RXMusic) Stop(); else Play();
		::GraphicsSystem.FlashMessageOnOff(LoadResStr("IDS_CTL_MUSIC"), !!Config.Sound.RXMusic);
	}
	else
	{
		// game menu
		Config.Sound.FEMusic = !Config.Sound.FEMusic;
		if (!Config.Sound.FEMusic) Stop(); else Play();
	}
	// key processed
	return true;
}
