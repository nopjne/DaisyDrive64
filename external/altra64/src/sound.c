//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//
#define SOUND_ENABLED
#if !defined(SOUND_ENABLED)


void sndInit(void)
{
}

void sndPlayBGM(char* filename)
{
}

void sndStopAll(void)
{
}

void sndPlaySFX(char* filename)
{
}

void sndUpdate(void)
{
}

#else

#include <mikmod.h>
#include <libdragon.h> //needed for audio_get_frequency()
#include "hashtable.h"

MIKMODAPI extern UWORD md_mode __attribute__((section (".data")));
MIKMODAPI extern UWORD md_mixfreq __attribute__((section (".data")));

MODULE *moduleBGM = NULL;

/* sound effects */
hashtable* samples = NULL;

/* voices */
SBYTE voiceSFX;

void sndInit(void)
{
    samples = hashtable_create();

    /* register all the drivers */
    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();

    /* initialize the library */
    md_mode = 0;
    md_mode |= DMODE_16BITS;
    md_mode |= DMODE_SOFT_MUSIC;
    md_mode |= DMODE_SOFT_SNDFX;
    md_mode |= DMODE_INTERP;

    md_mixfreq = audio_get_frequency();

    MikMod_Init("");

    /* reserve 2 voices for sound effects */
    MikMod_SetNumVoices(-1, 2);

    /* get ready to play */
    MikMod_EnableOutput();


}

void sndPlayBGM(char* filename)
{
    if (Player_Active())
    {
        Player_Stop();
    }
    Player_Free(moduleBGM);
    moduleBGM = NULL;

    moduleBGM = Player_Load(filename, 64, 0);

    if (moduleBGM)
    {
        Player_Start(moduleBGM);
        Player_SetVolume(80);
    }
}

void sndStopAll(void)
{
    Voice_Stop(voiceSFX);
    Player_Stop();
    MikMod_DisableOutput();

    int index = 0;
	while (index < samples->capacity) {
        Sample_Free(samples->body[index].value);
        index = index + 1;
	}

    hashtable_destroy(samples);

    Player_Free(moduleBGM);
    moduleBGM = NULL;

    samples = hashtable_create();
    //MikMod_Exit(); //I dont think we should ever exit as that would mean reinitialising?!
}

void sndPlaySFX(char* filename)
{
    if (!Voice_Stopped(voiceSFX))
    {
        Voice_Stop(voiceSFX);
    }


    if (hashtable_get(samples, filename) == NULL)
    {
        hashtable_set(samples, filename, Sample_Load(filename));
    }

    //audio_write_silence();
    Voice_SetVolume(voiceSFX, 800);
    voiceSFX = Sample_Play(hashtable_get(samples, filename), 0, 0);

    MikMod_Update(); //force an update so that the voice is registered as playing!

}

void sndUpdate(void)
{
    if (!Voice_Stopped(voiceSFX) || Player_Active())
    {
        MikMod_Update();

    }
}
#endif
