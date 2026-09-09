@echo off
call "%~dp0build_debug.bat"
if errorlevel 1 exit /b 1
start "" "%~dp0build\Debug\pokemulti.exe" %*
