@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "E:\Games\The Matrix Online\mxoemu_fork\Reality\build"
cmake -G Ninja ..
ninja Reality
exit /b %ERRORLEVEL%
