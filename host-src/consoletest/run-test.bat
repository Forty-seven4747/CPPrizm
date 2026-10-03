@echo off
chcp 65001 >nul

setlocal enabledelayedexpansion
set HERE=%~dp0
set PRIZM=%HERE%..\..

set GPP=g++
where g++ >nul 2>nul
if errorlevel 1 (
  if exist "C:\Users\Zhen\mingw64\bin\g++.exe" set GPP=C:\Users\Zhen\mingw64\bin\g++.exe
)

rem The harness compiles the real prizm_console.cpp against fake fxcg headers,
rem so every console change can be regression tested on the PC.  All six
rem font / paper combinations are built and run; the last line of each run is
rem "<something>pass=N fail=M".
rem
rem Include order matters: fake comes first so that <string> resolves to the
rem host stand-in, and toolchain\cpplib is deliberately NOT on the path - the
rem target C++ shims would shadow MinGW's own <cstdlib> / <cmath> and break
rem the compiler's own standard library.

set FONTS=1 2 3
set BGS=0 1
set BAD=0

for %%F in (%FONTS%) do (
  for %%B in (%BGS%) do (
    set EXE=%HERE%console_test_f%%F_b%%B.exe
    echo.
    echo === font=%%F  paper=%%B  ===
    "%GPP%" -std=c++17 -O0 -g -Wno-attributes -DPRIZM_CON_FONT=%%F -DPRIZM_CON_BG=%%B ^
      -I "%HERE%fake" -I "%PRIZM%\toolchain" -I "%PRIZM%\toolchain\cppshim" ^
      "%HERE%host_main.cpp" "%PRIZM%\toolchain\prizm_console.cpp" ^
      -o "!EXE!"
    if errorlevel 1 (
      echo   BUILD FAILED for font=%%F paper=%%B
      set BAD=1
    ) else (
      for /f "delims=" %%L in ('"!EXE!"') do set LAST=%%L
      echo   !LAST!
      echo !LAST! | find "fail=0" >nul || set BAD=1
    )
  )
)

echo.
if "!BAD!"=="1" (echo RESULT: FAILURES PRESENT) else (echo RESULT: all six schemes pass)

endlocal
pause
exit /b 0
