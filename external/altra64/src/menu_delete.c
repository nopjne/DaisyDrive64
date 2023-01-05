
#include <libdragon.h>
#include <stdio.h>
#include "types.h"
#include "menu.h"
#include "version.h"
#include "main.h"
#include "everdrive.h"


void menu_delete(display_context_t disp, bool isdir)
{	
	if (isdir)
	{
		printText("Cannot delete directories!", 7, 14, disp);
		printText("B: Exit", 13, 16, disp);	
	}
	else
	{
		printText("Delete this file?", 10, 14, disp);
		printText("A: Confirm", 13, 16, disp);
		printText("B: Cancel", 13, 17, disp);
	}
} 
