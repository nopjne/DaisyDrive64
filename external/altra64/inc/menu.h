//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//

#ifndef _MENU_H_
#define _MENU_H_

extern int text_offset;

void printText(char *msg, int x, int y, display_context_t dcon);

void menu_about(display_context_t disp);
void menu_controls(display_context_t disp);
void menu_delete(display_context_t disp, bool isdir);

#endif
