@echo off
set DEST=%1
if not defined DEST set DEST=Rad
xcopy /D /U %DEST% "%USERPROFILE%\Sync\Utils\RadLibPlus\Win\"
xcopy /D /U "%USERPROFILE%\Sync\Utils\RadLibPlus\Win\" %DEST%
