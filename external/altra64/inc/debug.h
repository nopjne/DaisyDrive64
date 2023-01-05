//
// Copyright (c) 2017 The Altra64 project contributors
// See LICENSE file in the project root for full license information.
//

#ifndef _DEBUG_H
#define	_DEBUG_H

//#ifdef DEBUG
//    #define TRACEF(disp, text, ...)    dbg_printf(disp, text, __VA_ARGS__);
//    #define TRACE(disp, text)    dbg_print(disp, text); 
//#else
    #define TRACEF(disp, text, ...)    do { if (0)  dbg_printf(disp, text, __VA_ARGS__); } while (0)
    #define TRACE(disp, text)    do { if (0) dbg_print(disp, text); } while (0)
//#endif

void dbg_printf(display_context_t disp, const char *fmt, ...);
void dbg_print(display_context_t disp, char *fmt);

#endif
