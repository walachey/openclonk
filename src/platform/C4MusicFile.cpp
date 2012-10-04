/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2001, 2006  Sven Eberhardt
 * Copyright (c) 2001  Michael Käser
 * Copyright (c) 2002-2004  Peter Wortmann
 * Copyright (c) 2004  Armin Burgmeier
 * Copyright (c) 2005-2006, 2008-2009  Günther Brammer
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
/* Handles Music Files */

#include <C4Include.h>
#include <C4MusicFile.h>

#include <C4Application.h>
#include <C4Log.h>

#ifdef HAVE_FMOD
#include <fmod_errors.h>
#endif

/* helpers */

bool C4MusicFile::ExtractFile()
{
	// safety
	if (SongExtracted) return true;
	// extract entry
	if (!C4Group_CopyItem(FileName, Config.AtTempPath(C4CFN_TempMusic2))) return false;
	// ok
	SongExtracted = true;
	return true;
}

bool C4MusicFile::RemTempFile()
{
	if (!SongExtracted) return true;
	// delete it
	EraseFile(Config.AtTempPath(C4CFN_TempMusic2));
	SongExtracted = false;
	return true;
}

bool C4MusicFile::Init(const char *szFile)
{
	SCopy(szFile, FileName);
	return true;
}

#if defined HAVE_FMOD
C4MusicFileFMOD::C4MusicFileFMOD() : mod(NULL), channel(NULL), Playing(false), Data(NULL)
{
}

C4MusicFileFMOD::~C4MusicFileFMOD()
{
	Stop();
}

bool C4MusicFileFMOD::Play(bool loop)
{
	// check existance
	if (!FileExists(FileName))
		// try extracting it
		if (!ExtractFile())
			// doesn't exist - or file is corrupt
			return false;

	// init fmusic
	FMOD_RESULT result = fmod_system->createSound(SongExtracted ? Config.AtTempPath(C4CFN_TempMusic2) : FileName, FMOD_DEFAULT | FMOD_CREATESTREAM | FMOD_CREATESTREAM, NULL, &mod);

	if (!mod)
	{
		LogF("FMod load MIDI: %s", FMOD_ErrorString(result));
		return false;
	}

	// Play Song
	result = fmod_system->playSound(FMOD_CHANNEL_FREE, mod, true, &channel);
	if (result != FMOD_OK)
	{
		LogF("FMod play MIDI: %s", FMOD_ErrorString(result));
		return false;
	}
	// Set highest priority
	if (channel->setPriority(255) != FMOD_OK)
		return false;

	Playing = true;

	// Actually start playing
	channel->setPaused(false);
	


	return true;
}

void C4MusicFileFMOD::Stop(int fadeout_ms)
{
	if (mod)
	{
		mod->release();
		mod = NULL;
		channel = NULL;
	}
	if (Data) { delete[] Data; Data = NULL; }
	RemTempFile();
	Playing = false;
}

void C4MusicFileFMOD::CheckIfPlaying()
{
	// Check if still playing
	if (mod && channel && Playing)
	{
		Playing = false;
		// Sound must still be in channel...
		FMOD::Sound *chan_sound;
		if (channel->getCurrentSound(&chan_sound) == FMOD_OK)
		{
			if (chan_sound == mod)
			{
				// ...and not done yet
				unsigned int chan_pos, song_len;
				if (   channel->getPosition(&chan_pos, FMOD_TIMEUNIT_PCM) == FMOD_OK
					&& mod->getLength(&song_len, FMOD_TIMEUNIT_PCM) == FMOD_OK)
				{
					if (chan_pos < song_len)
						Playing = true;
				}
				
			}
		}
	}
	if (!Playing)
		Application.MusicSystem.NotifySuccess();
}

void C4MusicFileFMOD::SetVolume(int iLevel)
{
	if (channel)
		channel->setVolume(BoundBy(float(iLevel)/100.0f, 0.0f, 1.0f));
}


#elif defined HAVE_LIBSDL_MIXER
C4MusicFileSDL::C4MusicFileSDL():
		Data(NULL),
		Music(NULL)
{
}

C4MusicFileSDL::~C4MusicFileSDL()
{
	Stop();
}

bool C4MusicFileSDL::Play(bool loop)
{
	const SDL_version * link_version = Mix_Linked_Version();
	if (link_version->major < 1
	    || (link_version->major == 1 && link_version->minor < 2)
	    || (link_version->major == 1 && link_version->minor == 2 && link_version->patch < 7))
	{
		// Check existance and try extracting it
		if (!FileExists(FileName)) if (!ExtractFile())
				// Doesn't exist - or file is corrupt
			{
				LogF("Error reading %s", FileName);
				return false;
			}
		// Load
		Music = Mix_LoadMUS(SongExtracted ? Config.AtTempPath(C4CFN_TempMusic2) : FileName);
		// Load failed
		if (!Music)
		{
			LogF("SDL_mixer: %s", SDL_GetError());
			return false;
		}
		// Play Song
		if (Mix_PlayMusic(Music, loop? -1 : 1) == -1)
		{
			LogF("SDL_mixer: %s", SDL_GetError());
			return false;
		}
	}
	else
	{
		// Load Song
		// Fixme: Try loading this from the group incrementally for less lag
		size_t filesize;
		if (!C4Group_ReadFile(FileName, &Data, &filesize))
		{
			LogF("Error reading %s", FileName);
			return false;
		}
		// Mix_FreeMusic frees the RWop
		Music = Mix_LoadMUS_RW(SDL_RWFromConstMem(Data, filesize));
		if (!Music)
		{
			LogF("SDL_mixer: %s", SDL_GetError());
			return false;
		}
		if (Mix_PlayMusic(Music, loop? -1 : 1) == -1)
		{
			LogF("SDL_mixer: %s", SDL_GetError());
			return false;
		}
	}
	return true;
}

void C4MusicFileSDL::Stop(int fadeout_ms)
{
	if (fadeout_ms && Music)
	{
		// Don't really stop yet
		Mix_FadeOutMusic(fadeout_ms);
		return;
	}
	if (Music)
	{
		Mix_FreeMusic(Music);
		Music = NULL;
	}
	RemTempFile();
	if (Data)
	{
		delete[] Data;
		Data = NULL;
	}
}

void C4MusicFileSDL::CheckIfPlaying()
{
	if (!Mix_PlayingMusic())
		Application.MusicSystem.NotifySuccess();
}

void C4MusicFileSDL::SetVolume(int iLevel)
{
	Mix_VolumeMusic((int) ((iLevel * MIX_MAX_VOLUME) / 100));
}

#endif
