//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#ifndef _MP3_H_
#define _MP3_H_

void mp3_Start(char *fname, long long *samples, int *rate, int *channels);
void mp3_Stop(void);
int mp3_Update(char *buf, int bytes);

#endif // _MP3_H_
