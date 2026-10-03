@echo off
chcp 65001 >nul

setlocal
set HERE=%~dp0
set SRC=%HERE%cpprizm.cpp
set OUTDIR=%HERE%..

set GPP=g++
where g++ >nul 2>nul
if errorlevel 1 (
  if exist "C:\Users\Zhen\mingw64\bin\g++.exe" set GPP=C:\Users\Zhen\mingw64\bin\g++.exe
)
echo [1/2] compiling GUI version ...
"%GPP%" -std=c++17 -O2 -municode -finput-charset=UTF-8 -fexec-charset=UTF-8 -static -static-libgcc -static-libstdc++ -mwindows "%SRC%" -o "%OUTDIR%\cpprizm-gui.exe" -lcomdlg32 -lcomctl32 -lshell32 -lole32 -luser32 -lgdi32
if errorlevel 1 goto fail

echo [2/2] compiling console version ...
"%GPP%" -std=c++17 -O2 -municode -finput-charset=UTF-8 -fexec-charset=UTF-8 -static -static-libgcc -static-libstdc++ -mconsole -DPRIZM_CLI "%SRC%" -o "%OUTDIR%\cpprizm.exe" -lcomdlg32 -lcomctl32 -lshell32 -lole32 -luser32 -lgdi32
if errorlevel 1 goto fail

echo.
echo Done. Both exes were rebuilt in %OUTDIR%
goto end

:fail
echo.
echo BUILD FAILED.

:end
endlocal
pause
