//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//

#ifndef _MEMORYPAK_H
#define	_MEMORYPAK_H

//TODO: not sure if this is correct!!!
extern char *mempak_path;

int file_to_mpk(display_context_t disp, u8 *filename);
void mpk_to_file(display_context_t disp, char *mpk_filename, int quick);
void view_mpk_file(display_context_t disp, char *mpk_filename);
void view_mpk(display_context_t disp);

#endif
