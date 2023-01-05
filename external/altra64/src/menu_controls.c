
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"

void menu_controls(display_context_t disp)
{
    printText("          - Controls -", 4, 4, disp);
    printText(" ", 4, -1, disp);
    printText("      L: show mempak menu", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("      Z: about screen", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("      A: start rom/directory", 4, -1, disp);
    printText("         mempak", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("      B: back/cancel", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("  START: start last rom", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(" C-left: rom info/mempak", 4, -1, disp);
    printText("         content view", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("C-right: rom config creen", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("   C-up: view full filename", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText(" C-down: Toplist 15", 4, -1, disp);
    printText(" ", 4, -1, disp);
    printText("  R + L: Delete file", 4, -1, disp);
}
