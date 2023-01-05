//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//
#ifndef _SOUND_H
#define	_SOUND_H

void sndInit(void);
void sndPlayBGM(char* filename);
void sndStopAll(void);
void sndPlaySFX(char* filename);
void sndUpdate(void);

#endif
