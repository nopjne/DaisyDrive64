//
// Copyright (c) 2017 The Altra64 project contributors
// Portions (c) 2013 saturnu (Alt64) based on libdragon, Neo64Menu, ED64IO, libn64-hkz, libmikmod
// See LICENSE file in the project root for full license information.
//

#include <libdragon.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "mempak.h"
#include "memorypak.h"
#include "ff.h"
#include "menu.h"
#include "debug.h"
#include "strlib.h"
#include "sys.h"
#include "debug.h"


enum MemoryPakFormat
{
    None,
    DexDrive,
    Z64
};

static uint8_t mempak_data[128 * 256];
char *mempak_path;

char ___TranslateNotes(char *bNote, char *Text)
{
#pragma warning(disable : 4305 4309)
    char cReturn = 0x00;
    const char aSpecial[] = {0x21, 0x22, 0x23, 0x60, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x3A, 0x3D, 0x3F, 0x40, 0x74, 0xA9, 0xAE};
                        //  { '!' , '\"', '#' , '`' , '*' , '+' , ',' , '-' , '.' , '/' , ':' , '=' , '?' , '>' , 'tm', '(r)','(c)' };
#pragma warning(default : 4305 4309)
    int i = 16;
    do
    {
        char b = bNote[i];
        if ((b > 0) && i < 32)
        {
            if (b <= 0x0F) // translate Icons as Spaces
                *Text = 0x20;
            else if (b <= 0x19) // Numbers
                *Text = 0x20 + b;
            else if (b <= 0x33) // Characters
                *Text = 0x47 + b;
            else if (b <= 0x44) // special Symbols
                *Text = aSpecial[b - 0x34];
            else if (b <= 0x94) // Japan
                *Text = 0xC0 + (b % 40);
            else // unknown
                *Text = (char)0xA4;

            ++i;
            ++Text;
        }
        else
        {
            *Text = '\0';
            if (b)
            {
                i = 12;
                Text = &cReturn;
            }
            else
                i = 13;
        }
    } while (i != 13);

    return cReturn;
}

char ___CountBlocks(char *bMemPakBinary, char *aNoteSizes)
{
    int wRemainingBlocks = 123;
    char bNextIndex;
    int i = 0;
    while (i < 16 && wRemainingBlocks <= 123)
    {
        aNoteSizes[i] = 0;
        bNextIndex = bMemPakBinary[0x307 + (i * 0x20)];
        while ((bNextIndex >= 5) && (aNoteSizes[i] < wRemainingBlocks))
        {
            aNoteSizes[i]++;
            bNextIndex = bMemPakBinary[0x101 + (bNextIndex * 2)];
        }

        if (aNoteSizes[i] > wRemainingBlocks)
            wRemainingBlocks = 0xFF;
        else
            wRemainingBlocks -= aNoteSizes[i];

        i++;
    }
    return wRemainingBlocks;
}

//old method to write a file to the mempak at controller 1
int file_to_mpk(display_context_t disp, u8 *filename)
{
    enum MemoryPakFormat memorypak_format;
    u8 buff[64];

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, filename, FA_READ);

    if (result == FR_OK)
    {
        //int fsize = f_size(&file);

        u8 *pch;
        pch = strrchr(filename, '.');
        sprintf(buff, "%s", (pch + 2));

        if (strcmp(buff, "64") == 0)
        {
            TRACE(disp, "Dexdrive format");
            memorypak_format = DexDrive;
            //skip header
            result = f_lseek (
                &file,  /* [IN] File object */
                4160  /* [IN] File read/write pointer */
              );
        }

        TRACE(disp, "Z64 format");
        memorypak_format = Z64;

        result =
        f_read (
            &file,        /* [IN] File object */
            mempak_data,  /* [OUT] Buffer to store read data */
            32768,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );

        f_close(&file);

        int err = 0;
        for (int j = 0; j < 128; j++)
        {
            err |= write_mempak_sector(0, j, &mempak_data[j * MEMPAK_BLOCK_SIZE]);
        }
    }
    else
    {
        memorypak_format = None;
    }

    return (int)memorypak_format; //TODO: should return enum
}

void view_mpk_file(display_context_t disp, char *mpk_filename)
{
    u8 buff[64];

    FRESULT result;
    FIL file;
    UINT bytesread;
    result = f_open(&file, mpk_filename, FA_READ);

    if (result == FR_OK)
    {
        //int fsize = f_size(&file);

        u8 *pch;
        pch = strrchr(mpk_filename, '.');
        sprintf(buff, "%s", (pch + 2));

        if (strcmp(buff, "64") == 0) //DEXDRIVE format
        {
            //skip header
            result = f_lseek (
                &file,  /* [IN] File object */
                4160  /* [IN] File read/write pointer */
              );
        }

        result =
        f_read (
            &file,        /* [IN] File object */
            mempak_data,  /* [OUT] Buffer to store read data */
            32768,         /* [IN] Number of bytes to read */
            &bytesread    /* [OUT] Number of bytes read */
        );


        f_close(&file);

        printText("File contents:", 11, 5, disp);
        printText("   ", 11, -1, disp);

        int notes_c = 0;

        char szBuffer[40],
            cAppendix;
        int bFirstChar;

        int i = 0,
            nNotes = 0,
            iSum = 0,
            iRemainingBlocks;

        char aNoteSizes[16];

        for (i = 0x10A; i < 0x200; i++)
            iSum += mempak_data[i];

        if (((iSum % 256) == mempak_data[0x101]))
        {
            iRemainingBlocks = ___CountBlocks(mempak_data, aNoteSizes);

            if (iRemainingBlocks <= 123)
            {
                for (notes_c = 0; notes_c < 16; notes_c++)
                {
                    if (mempak_data[0x300 + (notes_c * 32)] ||
                        mempak_data[0x301 + (notes_c * 32)] ||
                        mempak_data[0x302 + (notes_c * 32)])
                    {
                        cAppendix = ___TranslateNotes(&mempak_data[0x300 + (notes_c * 32)], szBuffer);

                        if (cAppendix != '\0')
                            sprintf(szBuffer, "%s. %c", szBuffer, cAppendix);

                        bFirstChar = 1;
                        for (i = 0; i < (int)strlen(szBuffer); i++)
                        {
                            if (szBuffer[i] == ' ')
                                bFirstChar = 1;
                            else
                            {
                                if (bFirstChar && (szBuffer[i] >= 'a') && (szBuffer[i] <= 'z'))
                                {
                                    bFirstChar = 0;
                                    szBuffer[i] -= 0x20;
                                }
                            }
                        }
                        printText(szBuffer, 11, -1, disp);

                        switch (mempak_data[0x303 + (notes_c * 32)])
                        {
                        case 0x00:
                            sprintf(szBuffer, "None");
                            break;
                        case 0x37:
                            sprintf(szBuffer, "Beta");
                            break;
                        case 0x41:
                            sprintf(szBuffer, "NTSC");
                            break;
                        case 0x44:
                            sprintf(szBuffer, "Germany");
                            break;
                        case 0x45:
                            sprintf(szBuffer, "USA");
                            break;
                        case 0x46:
                            sprintf(szBuffer, "France");
                            break;
                        case 0x49:
                            sprintf(szBuffer, "Italy");
                            break;
                        case 0x4A:
                            sprintf(szBuffer, "Japan");
                            break;
                        case 0x50:
                            sprintf(szBuffer, "Europe");
                            break;
                        case 0x53:
                            sprintf(szBuffer, "Spain");
                            break;
                        case 0x55:
                            sprintf(szBuffer, "Australia");
                            break;
                        case 0x58:
                        case 0x59:
                            sprintf(szBuffer, "PAL");
                            break;
                        default:
                            sprintf(szBuffer, "Unknown(%02X)", mempak_data[0x303 + (notes_c * 32)]);
                        }

                        sprintf(szBuffer, "%i", aNoteSizes[notes_c]);
                        nNotes++;
                    }
                }
            }

            int free_c = 0;
            for (free_c = nNotes; free_c < 16; free_c++)
                printText("[free]", 11, -1, disp);

            char buff[512];
            printText("   ", 11, -1, disp);
            printText("Free space:", 11, -1, disp);
            sprintf(buff, "%i blocks", iRemainingBlocks);
            printText(buff, 11, -1, disp);
        }
        else
        {
            printText("empty", 11, -1, disp);
        }
    }
}

void view_mpk(display_context_t disp)
{
    int err;

    printText("Mempak content:", 11, 5, disp);
    struct controller_data output;
    get_accessories_present( &output);

    /* Make sure they don't have a rumble pak inserted instead */
    switch (identify_accessory(0))
    {
    case ACCESSORY_NONE:

        printText(" ", 11, -1, disp);
        printText("no Mempak", 11, -1, disp);
        break;

    case ACCESSORY_MEMPAK:
        if ((err = validate_mempak(0)))
        {
            if (err == -3)
            {
                printText(" ", 11, -1, disp);

                printText("not formatted", 11, -1, disp);
            }
            else
            {
                printText(" ", 11, -1, disp);
                printText("read error", 11, -1, disp);
            }
        }
        else
        {
            printText("   ", 11, -1, disp);
            for (int j = 0; j < 16; j++)
            {
                entry_structure_t entry;

                get_mempak_entry(0, j, &entry);

                if (entry.valid)
                {
                    char tmp[512];
                    sprintf(tmp, "%s", entry.name);
                    printText(tmp, 11, -1, disp);
                }
                else
                {
                    printText("[free]", 11, -1, disp);
                }
            }

            char buff[512];
            printText("   ", 11, -1, disp);
            printText("Free space:", 11, -1, disp);
            sprintf(buff, "%d blocks", get_mempak_free_space(0));
            printText(buff, 11, -1, disp);
        }
        break;

    case ACCESSORY_RUMBLEPAK:
        printText("RumblePak inserted", 11, -1, disp);
        break;

    default:
        break;
    }
}

//old function to dump a mempak to a file
void mpk_to_file(display_context_t disp, char *mpk_filename, int quick)
{
    u8 buff[64];
    u8 v = 0;
    u8 ok = 0;

    if (quick)
        sprintf(buff, "%s%s", mempak_path, mpk_filename);
    else
        sprintf(buff, "%s%s.MPK", mempak_path, mpk_filename);

    FRESULT fr;
    FILINFO fno;

    fr = f_stat(buff, &fno);
    if(fr == FR_OK)
    {
        printText("File exists", 9, -1, disp);
        if (quick)
            printText("override", 9, -1, disp);
        else
            while (fr == FR_OK)
            {
                sprintf(buff, "%s%s%i.MPK", mempak_path, mpk_filename, v);

                fr = f_stat(buff, &fno);
                if (fr == FR_OK)
                    v++;
                else
                    break;
            }
    }

    FRESULT result;
    FIL file;
    result = f_open(&file, buff, FA_WRITE | FA_OPEN_ALWAYS);

    if (result == FR_OK)
    {
        controller_init();

        int err = 0;
        for (int j = 0; j < 128; j++)
        {
            err |= read_mempak_sector(0, j, &mempak_data[j * 256]);
        }

        UINT bw;
        result =
        f_write (
            &file,          /* [IN] Pointer to the file object structure */
            mempak_data, /* [IN] Pointer to the data to be written */
            32768,         /* [IN] Number of bytes to write */
            &bw          /* [OUT] Pointer to the variable to return number of bytes written */
          );

        f_close(&file);


        sprintf(buff, "File: %s%i.MPK", mpk_filename, v);

        printText(buff, 9, -1, disp);
        printText("backup done...", 9, -1, disp);
    }
}
