
#include <libdragon.h>
#include "types.h"
#include "debug.h"


short int gCursorX;
short int gCursorY;

int text_offset = 0;

void printText(char *msg, int x, int y, display_context_t dcon)
{
    x = x + text_offset;

    if (x != -1)
        gCursorX = x;
    if (y != -1)
        gCursorY = y;

    if (dcon)
        graphics_draw_text(dcon, gCursorX * 8, gCursorY * 8, msg);

    gCursorY++;
    if (gCursorY > 29)
    {
        gCursorY = 0;
        gCursorX++;
    }
}
