@echo off
cd /d "%~dp0"
if not exist "dist\PokeMulti-0.26.7\pokemulti.exe" (
  echo Build and package version 0.26.7 first.
  pause
  exit /b 1
)
start "" "%~dp0dist\PokeMulti-0.26.7\pokemulti.exe" %*
