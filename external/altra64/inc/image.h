//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//

#ifndef _IMAGE_H
#define	_IMAGE_H

sprite_t *loadImage32DFS(char *fname);
sprite_t *loadImageDFS(char *fname);
sprite_t *loadImage32(u8 *tbuf, int size);
void drawImage(display_context_t dcon, sprite_t *sprite);

#endif
