@echo off
setlocal
pushd "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c11 /W4 /I..\main protocol_test.c ..\main\protocol.c /Fe:protocol_test.exe
if errorlevel 1 exit /b 1
protocol_test.exe
if errorlevel 1 exit /b 1
node ui_test.js
if errorlevel 1 exit /b 1
node response_test.js
popd
