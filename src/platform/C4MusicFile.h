/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2001  Sven Eberhardt
 * Copyright (c) 2001  Michael Käser
 * Copyright (c) 2002-2003  Peter Wortmann
 * Copyright (c) 2005, 2008-2009  Günther Brammer
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

#ifndef INC_C4MusicFile
#define INC_C4MusicFile

#if defined HAVE_FMOD
#include <fmod.hpp>
#elif defined HAVE_LIBSDL_MIXER
#define USE_RWOPS
#include <SDL_mixer.h>
#undef USE_RWOPS
#endif
/* Base class */

class C4MusicFile
{
public:

	C4MusicFile() : LastPlayed(-1), NoPlay(false), SongExtracted(false) { }
	virtual ~C4MusicFile() { }

	// data
	char FileName[_MAX_FNAME +1];
	C4MusicFile *pNext;
	int LastPlayed;
	bool NoPlay;

	virtual bool Init(const char *strFile);
	virtual bool Play(bool loop = false) = 0;
	virtual void Stop(int fadeout_ms = 0) = 0;
	virtual void CheckIfPlaying() = 0;
	virtual void SetVolume(int) = 0;

protected:

	// helper: copy data to a (temp) file
	bool ExtractFile();
	bool RemTempFile(); // remove the temp file

	bool SongExtracted;

};
#if defined HAVE_FMOD

class C4MusicFileFMOD : public C4MusicFile
{
public:
	C4MusicFileFMOD();
	~C4MusicFileFMOD();
	bool Play(bool loop = false);
	void Stop(int fadeout_ms = 0);
	void CheckIfPlaying();
	void SetVolume(int);

	static signed char __stdcall OnEnd(FMOD::Sound* stream, void* buff, int length, void* param);
protected:
	FMOD::Sound *mod;
	FMOD::Channel *channel;
	char *Data;

	bool Playing;
};

#elif defined HAVE_LIBSDL_MIXER
typedef struct _Mix_Music Mix_Music;
class C4MusicFileSDL : public C4MusicFile
{
public:
	C4MusicFileSDL();
	~C4MusicFileSDL();
	bool Play(bool loop = false);
	void Stop(int fadeout_ms = 0);
	void CheckIfPlaying();
	void SetVolume(int);
protected:
	char *Data;
	Mix_Music * Music;
};
#endif // HAVE_LIBSDL_MIXER

#endif
