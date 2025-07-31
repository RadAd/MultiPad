@echo off
set DEST=%1
if not defined DEST set DEST=.
xcopy /D /U /EXCLUDE:xcopyexclude.txt %DEST% "%USERPROFILE%\Sync\Utils\ProjectTemplates\Win32Cpp"
xcopy /D /U /EXCLUDE:xcopyexclude.txt "%USERPROFILE%\Sync\Utils\ProjectTemplates\Win32Cpp" %DEST%
