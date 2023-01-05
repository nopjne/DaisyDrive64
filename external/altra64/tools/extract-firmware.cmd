::
:: Copyright (c) 2017 The Altra64 project contributors
:: See LICENSE file in the project root for full license information.
::

@echo off
cd ..
set "fs=%cd%\res\filesystem\firmware"
set "lfs=%fs%"

set "drive=%lfs:~0,1%"

set lfs=%lfs:~2%
set "lfs=%lfs:\=/%"

if %drive%==A set "drive=a"
if %drive%==B set "drive=b"
if %drive%==C set "drive=c"
if %drive%==D set "drive=d"
if %drive%==E set "drive=e"
if %drive%==F set "drive=f"
if %drive%==G set "drive=g"
if %drive%==H set "drive=h"
if %drive%==I set "drive=i"
if %drive%==J set "drive=j"
if %drive%==K set "drive=k"
if %drive%==L set "drive=l"
if %drive%==M set "drive=m"
if %drive%==N set "drive=n"
if %drive%==O set "drive=o"
if %drive%==P set "drive=p"
if %drive%==Q set "drive=q"
if %drive%==R set "drive=r"
if %drive%==S set "drive=s"
if %drive%==T set "drive=t"
if %drive%==U set "drive=u"
if %drive%==V set "drive=v"
if %drive%==W set "drive=w"
if %drive%==X set "drive=x"
if %drive%==Y set "drive=y"
if %drive%==Z set "drive=z"

set "lfs=/mnt/%drive%%lfs%"

echo "Windows dir is %fs%"
echo "Linux dir is %lfs%"

:: del old firmware dir in ../res/filesystem
RD /S /Q "%fs%"
:: mk firmware dir in ../res/filesystem
MKDIR "%fs%"

SET "rom=%1"
IF %1.==. (
SET /P rom="Please enter full path to OS64.v64 V2.12:"
)

set "drive=%rom:~0,1%"

set rom=%rom:~2%
set "rom=%rom:\=/%"

if %drive%==A set "drive=a"
if %drive%==B set "drive=b"
if %drive%==C set "drive=c"
if %drive%==D set "drive=d"
if %drive%==E set "drive=e"
if %drive%==F set "drive=f"
if %drive%==G set "drive=g"
if %drive%==H set "drive=h"
if %drive%==I set "drive=i"
if %drive%==J set "drive=j"
if %drive%==K set "drive=k"
if %drive%==L set "drive=l"
if %drive%==M set "drive=m"
if %drive%==N set "drive=n"
if %drive%==O set "drive=o"
if %drive%==P set "drive=p"
if %drive%==Q set "drive=q"
if %drive%==R set "drive=r"
if %drive%==S set "drive=s"
if %drive%==T set "drive=t"
if %drive%==U set "drive=u"
if %drive%==V set "drive=v"
if %drive%==W set "drive=w"
if %drive%==X set "drive=x"
if %drive%==Y set "drive=y"
if %drive%==Z set "drive=z"

set "rom=/mnt/%drive%%rom%"

echo "Linux rom dir is %rom%"



::echo. "This script is not yet ready and will now exit."
::GOTO exit

@echo ON

:: OS64.V64 - Version 2.12 firmware offsets:
:: cart 	offset (hex)	offset (dec) 	length
:: v2_old 	0x25070 	151664		61552
:: v2 		0x15930 	88368		63276
:: v2.5 	0x340F0 	213232		69911
:: v3 		0x45210 	283152		71187

:: Count = lengh / blocksize
:: Seek = offset converted to decimal / blocksize

:: ED rev 2_old
bash --verbose -c "dd skip=9479 count=3847 if=%rom% of=%lfs%/firmware_v2_old.bin bs=16"

:: ED rev 2.0 (default should return 0x214)
bash --verbose -c "dd skip=5523 count=3954 if=%rom% of=%lfs%/firmware_v2.bin bs=16"

:: ED rev 2.5 (default should return 0x250)
bash --verbose -c "dd skip=13327 count=4369 if=%rom% of=%lfs%/firmware_v2_5.bin bs=16"

:: ED rev 3 (default should return 0x300)
bash --verbose -c "dd skip=17697 count=4449 if=%rom% of=%lfs%/firmware_v3.bin bs=16"


pause
:exit