@echo off
rem ======================================================================
rem PoC-001 one-command build: vs2019-x32 (MSVC VS2019, x86)
rem
rem STATUS: UNVERIFIED on Windows. Written without access to Windows.
rem
rem Usage: open a CLEAN cmd.exe window (vcvarsall must run only once per
rem        window) and run:  scripts\build-vs2019-x32.bat
rem
rem Layout assumed (see README.md):
rem   d:\hbdev32\harbour-webui\   PoC root (this script is in scripts\)
rem   d:\hbdev32\webui\           WebUI library, dist\webui-2-static.lib
rem   d:\hbdev32\harbour-core\    Harbour, bin\win\msvc\hbmk2.exe
rem
rem Output: hbwebui.exe in the PoC root, log in scripts\build-vs2019-x32.log
rem Exit code: 0 on success, 1 on any failure.
rem ======================================================================

setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "LOG=%SCRIPT_DIR%build-vs2019-x32.log"

rem Work from the script folder first, then move to the PoC root, because
rem the hbmk2 command line uses paths relative to the PoC root.
pushd "%SCRIPT_DIR%"
if errorlevel 1 goto err_cd
pushd ..
if errorlevel 1 goto err_cd2
set "ROOT=%CD%"

if exist "%LOG%" del "%LOG%"

rem All real work runs in :main with stdout and stderr going to the log;
rem the log is then echoed to the console.
call :main > "%LOG%" 2>&1
set "BUILD_RC=%ERRORLEVEL%"

type "%LOG%"
echo.
rem Success needs exit code 0 AND the BUILD OK marker line in the log.
rem This catches any path in :main that ends without a proper exit code.
if "%BUILD_RC%"=="0" findstr /b /c:"BUILD OK" "%LOG%" >nul 2>&1
if "%BUILD_RC%"=="0" if errorlevel 1 set "BUILD_RC=1"
if "%BUILD_RC%"=="0" goto done_ok

echo ======================================================================
echo BUILD FAILED. Exit code %BUILD_RC%.
echo Read the log file for details:
echo   %LOG%
echo The last [ERROR] line in the log names the failing step.
echo ======================================================================
popd
popd
endlocal & exit /b 1

:err_cd
echo [ERROR] Cannot change directory to the script folder.
exit /b 1

:err_cd2
popd
echo [ERROR] Cannot change directory to the PoC root.
exit /b 1

:done_ok
popd
popd
endlocal & exit /b 0


:main
echo Build started: %DATE% %TIME%
echo PoC root: %ROOT%
echo Log file: %LOG%
echo STATUS: UNVERIFIED on Windows.
echo.

rem ---- 1. Environment (values from docs/team/ENVIRONMENT.md) ----------
set "HBDEV=d:\hbdev32"
set "HB_BIN=%HBDEV%\harbour-core\bin\win\msvc"
set "HBMK2=%HB_BIN%\hbmk2.exe"
set "WEBUI_LIB=%ROOT%\..\webui\dist\webui-2-static.lib"
set "WEBUI_INC=%ROOT%\..\webui\include"

if not defined VCVARSALL set "VCVARSALL=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"

set "HB_COMPILER=msvc"
set "HB_BUILD_MODE=c"
set "HB_BUILD_DYN=no"
set "HB_BUILD_CONTRIB_DYN=no"
set "HB_BUILD_CONTRIBS="
set "HB_STATIC_OPENSSL=yes"
set "HB_STATIC_CURL=yes"
set "HB_USER_CFLAGS=/source-charset:utf-8 /execution-charset:utf-8"
set "HB_USER_PRGFLAGS=-l-"
set "HB_WITH_OPENSSL=%HBDEV%\openssl-3.5.9\x32"
set "HB_WITH_CURL=%HBDEV%\curl-8.23.0\release\x32"
set "HB_WITH_ADS=%HBDEV%\acesdk\x32"

chcp 65001 >nul

rem ---- 2. Preflight: tools and source files ----------------------------
set "MISSING="

if not exist "%HBMK2%" call :missing "%HBMK2%"
if not exist "%ROOT%\prg\main.prg" call :missing "%ROOT%\prg\main.prg"
if not exist "%ROOT%\prg\hello.prg" call :missing "%ROOT%\prg\hello.prg"
if not exist "%ROOT%\src\bridge.c" call :missing "%ROOT%\src\bridge.c"
if not exist "%ROOT%\ui\index.html" call :missing "%ROOT%\ui\index.html"
if not exist "%WEBUI_INC%\webui.h" call :missing "%WEBUI_INC%\webui.h"
if defined MISSING goto err_preflight
echo Preflight OK: hbmk2.exe, 4 source files and webui.h found.

rem OpenSSL, curl and ADS roots are only needed to build Harbour contribs.
rem The PoC link step needs none of them, so a missing root is a WARN only.
call :warn_dir "%HB_WITH_OPENSSL%" OpenSSL
call :warn_dir "%HB_WITH_CURL%" curl
call :warn_dir "%HB_WITH_ADS%" ADS

rem ---- 3. WebUI static library -----------------------------------------
if not exist "%WEBUI_LIB%" goto err_weblib
echo WebUI library OK: %WEBUI_LIB%

rem ---- 4. MSVC environment: vcvarsall.bat x86, called exactly once -----
if defined VSCMD_VER goto err_vsenv
if not exist "%VCVARSALL%" goto err_vcvars
echo Calling vcvarsall.bat x86 ...
call "%VCVARSALL%" x86
if errorlevel 1 goto err_vcvarsrc
where cl.exe >nul 2>&1
if errorlevel 1 goto err_nocl
where link.exe >nul 2>&1
if errorlevel 1 goto err_nolink

set "PATH=%HB_BIN%;%PATH%"

rem ---- 5. Build with hbmk2 (same command line as README.md) ------------
if exist "%ROOT%\hbwebui.exe" del "%ROOT%\hbwebui.exe"
echo.
echo Running hbmk2 ...
"%HBMK2%" -st prg\main.prg prg\hello.prg src\bridge.c -I..\webui\include -L..\webui\dist -lwebui-2-static -lws2_32 -lole32 -luser32 -ladvapi32 -lshell32 -ohbwebui.exe
set "HBMK2_RC=%ERRORLEVEL%"
if not "%HBMK2_RC%"=="0" goto err_hbmk2
if not exist "%ROOT%\hbwebui.exe" goto err_noexe

echo.
echo BUILD OK
echo Output: %ROOT%\hbwebui.exe
echo Run it from the PoC root so that ui\index.html is found:
echo   cd /d %ROOT%
echo   hbwebui.exe
exit /b 0

:missing
echo [ERROR] Required file not found: %~1
set "MISSING=1"
exit /b 0

:warn_dir
if exist "%~1\" exit /b 0
echo [WARN] %~2 root not found: %~1
echo        Not needed to link the PoC with hbmk2, so the build continues.
exit /b 0

:err_preflight
echo [ERROR] Preflight failed. Fix the missing items above and run again.
exit /b 1

:err_weblib
echo [ERROR] WebUI static library not found: %WEBUI_LIB%
echo         Build it first: open a clean command prompt, then run
echo           cd /d d:\hbdev32\webui
echo           call d:\hbdev32\vs2019-x32.bat
echo           nmake
echo         It must produce dist\webui-2-static.lib. Then run this script again.
exit /b 1

:err_vsenv
echo [ERROR] This command prompt already has a Visual Studio environment.
echo         vcvarsall.bat must be called only once per window, or paths get too long.
echo         Open a NEW clean command prompt and run this script again.
exit /b 1

:err_vcvars
echo [ERROR] vcvarsall.bat not found: %VCVARSALL%
echo         Install VS2019 Enterprise, or set the VCVARSALL variable to the
echo         correct full path before running this script.
exit /b 1

:err_vcvarsrc
echo [ERROR] vcvarsall.bat x86 returned a nonzero exit code.
exit /b 1

:err_nocl
echo [ERROR] cl.exe is not on PATH after vcvarsall.bat x86.
exit /b 1

:err_nolink
echo [ERROR] link.exe is not on PATH after vcvarsall.bat x86.
exit /b 1

:err_hbmk2
echo [ERROR] hbmk2 failed with exit code %HBMK2_RC%.
exit /b 1

:err_noexe
echo [ERROR] hbmk2 returned 0 but hbwebui.exe was not created.
exit /b 1
