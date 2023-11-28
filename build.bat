@ECHO .
@ECHO .  ****    ****    *            **
@ECHO .  *   *  *       * *          *  *
@ECHO .  *   *   ***   *****  *****    *
@ECHO .  *   *      *  *   *          *
@ECHO .  ****   ****   *   *         ****
@ECHO .
@ECHO . build file for version 5.0
@ECHO .
@REM set installation directory of assembler
@REM 
SET MPASM="%MPLAB%\mpasmwin.exe"
@REM
if "%1" == "clean" GOTO :clean
if "%1" == "distclean" GOTO :distclean
%MPASM% DSA-2.asm
GOTO :fin
:distclean
ERASE *.HEX
:clean
ERASE *.COD
ERASE *.ERR
ERASE *.LST
:fin
