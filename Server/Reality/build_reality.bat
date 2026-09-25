@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0build"
cmake -G Ninja ..
ninja Reality
exit /b %ERRORLEVEL%
