::
:: Copyright (c) 2017 The Altra64 project contributors
:: See LICENSE file in the project root for full license information.
::

@echo off
set "SystemPath=%SystemRoot%\\System32"
IF EXIST %WINDIR%\\sysnative\\reg.exe (
  set "SystemPath=%SystemRoot%\Sysnative"
  echo. "32-bit process..."
)

set env="/usr/local/libdragon"

IF %1.==. (
  echo. "no parameter"
  %SystemPath%\\bash --verbose -c "export N64_INST=%env%; make"
) ELSE (
  echo. "parameter: %1"
  %SystemPath%\\bash --verbose -c "export N64_INST=%env%; make %1"
)

:pause