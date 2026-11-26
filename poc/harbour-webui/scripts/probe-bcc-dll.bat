@echo off
rem ======================================================================
rem PoC-001 BCC probe kit: can BCC 7.4 use the WebUI DLL (webui-2.dll)?
rem
rem STATUS: UNVERIFIED on Windows. Written without access to Windows.
rem
rem Usage (clean cmd.exe window):
rem   call d:\hbdev32\bcc_x32.bat
rem   cd /d d:\hbdev32\harbour-webui
rem   scripts\probe-bcc-dll.bat
rem
rem Needs: d:\hbdev32\webui\dist\webui-2.dll (built with nmake in vs2019-x32).
rem
rem Six steps, each logs to scripts\probe-logs\step-N.log. A failing step
rem never stops the run; its exit code is recorded in the summary. Only a
rem missing BCC environment (HB_COMPILER) stops the script, with exit 1.
rem
rem   1 identify tools        2 implib + tlib symbol list
rem   3 dumpbin (if on PATH)  4 link probe via hbmk2 (no window is shown)
rem   5 build and run PoC with the DLL as hbwebui_bcc.exe (you click, then
rem     close the window); the MSVC hbwebui.exe is never touched
rem   6 hbmk2 -trace -info for the PoC build (VM mode, link line)
rem
rem Earlier logs are never deleted. Only the same step file is overwritten.
rem When done, zip scripts\probe-logs and send it back.
rem ======================================================================

setlocal EnableExtensions

if not defined HB_COMPILER goto err_env
if /i not "%HB_COMPILER%"=="bcc" goto err_env

pushd "%~dp0"
if errorlevel 1 goto err_cd
set "SCRIPT_DIR=%CD%\"
pushd ..
if errorlevel 1 goto err_cd2
set "ROOT=%CD%"
for %%I in ("%ROOT%\..\webui") do set "WEBUI=%%~fI"
set "LOGDIR=%SCRIPT_DIR%probe-logs"
set "SUMFILE=%LOGDIR%\summary.txt"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%LOGDIR%" goto err_logdir

set "HBMK2="
for /f "delims=" %%P in ('where hbmk2.exe 2^>nul') do if not defined HBMK2 set "HBMK2=%%P"
if not defined HBMK2 if exist "d:\hbdev32\harbour-core\bin\win\bcc\hbmk2.exe" set "HBMK2=d:\hbdev32\harbour-core\bin\win\bcc\hbmk2.exe"

set "RC1=-" & set "RC2=-" & set "RC3=-" & set "RC4=-" & set "RC5=-" & set "RC6=-"
set "NOTE1=" & set "NOTE2=" & set "NOTE3=" & set "NOTE4=" & set "NOTE5=" & set "NOTE6="
set "LIB5=webui-2"

echo BCC probe kit. STATUS: UNVERIFIED on Windows.
echo PoC root: %ROOT%
echo WebUI   : %WEBUI%
echo Logs    : %LOGDIR%
if defined VSCMD_VER echo [WARN] A Visual Studio environment is active in this window. Use a clean window for BCC.
if defined HBMK2 echo HBMK2   : %HBMK2%
if not defined HBMK2 echo HBMK2   : NOT FOUND
if not defined HBMK2 echo [WARN] hbmk2.exe not found: steps 4, 5 and 6 will fail.
echo.

echo Step 1 of 6: identify tools ...
call :step1 > "%LOGDIR%\step-1.log" 2>&1
set "RC1=%ERRORLEVEL%"
echo   rc=%RC1% %NOTE1%

echo Step 2 of 6: implib and tlib on webui-2.dll ...
call :step2 > "%LOGDIR%\step-2.log" 2>&1
set "RC2=%ERRORLEVEL%"
echo   rc=%RC2% %NOTE2%

echo Step 3 of 6: dumpbin ...
call :step3 > "%LOGDIR%\step-3.log" 2>&1
set "RC3=%ERRORLEVEL%"
echo   rc=%RC3% %NOTE3%

echo Step 4 of 6: link probe ...
call :step4 > "%LOGDIR%\step-4.log" 2>&1
set "RC4=%ERRORLEVEL%"
echo   rc=%RC4% %NOTE4%

echo Step 5 of 6: build and run the PoC with the DLL ...
call :step5
set "RC5=%ERRORLEVEL%"
echo   rc=%RC5% %NOTE5%

echo Step 6 of 6: hbmk2 trace ...
call :step6 > "%LOGDIR%\step-6.log" 2>&1
set "RC6=%ERRORLEVEL%"
echo   rc=%RC6% %NOTE6%
echo.

if exist "%SUMFILE%" del "%SUMFILE%"
call :say "BCC probe summary   %DATE% %TIME%"
call :say "STATUS: UNVERIFIED on Windows (script was written without running it)."
call :say "HBMK2 used: %HBMK2%"
call :say "----------------------------------------------------------------------"
call :say "Step  rc    Name                        Note"
call :say "----------------------------------------------------------------------"
call :say "1     %RC1%    identify tools              %NOTE1%"
call :say "2     %RC2%    implib + tlib symbols       %NOTE2%"
call :say "3     %RC3%    dumpbin                     %NOTE3%"
call :say "4     %RC4%    link probe (hbmk2)          %NOTE4%"
call :say "5     %RC5%    PoC run with DLL            %NOTE5%"
call :say "6     %RC6%    hbmk2 trace (not decisive)  %NOTE6%"
call :say "----------------------------------------------------------------------"
call :say "Send back: zip the whole folder  %LOGDIR%"
call :say "(include run.log, step-N.log, summary.txt and the other files in it)"

popd
popd
endlocal & exit /b 0

:err_env
echo [ERROR] BCC environment not found: HB_COMPILER is not set to bcc.
echo         In a clean command prompt run:  call d:\hbdev32\bcc_x32.bat
echo         then run this script again.
exit /b 1

:err_cd
echo [ERROR] Cannot change directory to the script folder.
exit /b 1

:err_cd2
popd
echo [ERROR] Cannot change directory to the PoC root.
exit /b 1

:err_logdir
popd
popd
echo [ERROR] Cannot create the log folder: %LOGDIR%
exit /b 1


rem ----------------------------------------------------------------------
rem Helpers
rem ----------------------------------------------------------------------

:say
echo.%~1
>>"%SUMFILE%" echo.%~1
exit /b 0

:showfile
set "FSZ=0"
if exist "%~1" for %%F in ("%~1") do set "FSZ=%%~zF"
if "%FSZ%"=="0" goto showfile_empty
type "%~1"
exit /b 0
:showfile_empty
echo (empty or missing: %~1)
exit /b 0

:where1
echo --- where %1
where %1 2>&1
if errorlevel 1 set "TOOLS_MISSING=1"
echo.
exit /b 0


rem ----------------------------------------------------------------------
rem Step 1: identify tools (decisive)
rem ----------------------------------------------------------------------
:step1
echo STEP 1: identify tools
echo HB_COMPILER=%HB_COMPILER%
echo.
set "TOOLS_MISSING="
set "BCC32="
for %%T in (bcc32 implib tlib ilink32 hbmk2) do call :where1 %%T
for /f "delims=" %%P in ('where bcc32 2^>nul') do if not defined BCC32 set "BCC32=%%P"

echo --- bcc32 banner (no arguments, stdout and stderr) saved to step-1-bcc32-banner.txt
bcc32 > "%LOGDIR%\step-1-bcc32-banner.txt" 2>&1
echo bcc32 rc=%ERRORLEVEL%
type "%LOGDIR%\step-1-bcc32-banner.txt"
echo.

if not defined BCC32 goto step1_nocfg
for %%D in ("%BCC32%") do set "BCCBIN=%%~dpD"
echo bcc32 folder: %BCCBIN%
for %%C in (bcc32.cfg ilink32.cfg) do call :typecfg %%C
:step1_nocfg
echo --- HB_ variables
set HB_
echo.
if defined TOOLS_MISSING goto step1_missing
set "NOTE1=all tools found"
exit /b 0
:step1_missing
echo [ERROR] One or more tools were not found on PATH, see above.
set "NOTE1=some tools missing"
exit /b 1

:typecfg
echo --- %1
if exist "%BCCBIN%%1" goto typecfg_show
echo (not found next to bcc32: %BCCBIN%%1)
echo.
exit /b 0
:typecfg_show
type "%BCCBIN%%1"
echo.
exit /b 0


rem ----------------------------------------------------------------------
rem Step 2: import library from the DLL (decisive)
rem implib -c makes dist\bcc\webui-2.lib. A second variant, webui-2-us.lib,
rem is made with implib -a (adds leading underscore names for cdecl); it is
rem an extra safety net, not part of the original request. The script only
rem records what it finds; BCC may need _webui_new_window (leading underscore).
rem ----------------------------------------------------------------------
:step2
echo STEP 2: import library from webui-2.dll
echo Removing stale outputs of this step first.
for %%F in (webui-2.lib webui-2.lst webui-2-us.lib webui-2-us.lst) do if exist "%WEBUI%\dist\bcc\%%F" del "%WEBUI%\dist\bcc\%%F"
for %%F in (step-2-symbols.txt step-2-symbols-us.txt) do if exist "%LOGDIR%\%%F" del "%LOGDIR%\%%F"
if exist "%WEBUI%\dist\webui-2.dll" goto step2_havedll
echo [ERROR] DLL not found: %WEBUI%\dist\webui-2.dll
echo         Build it first with nmake in d:\hbdev32\webui (vs2019-x32 environment).
set "NOTE2=webui-2.dll missing"
exit /b 1
:step2_havedll
pushd "%WEBUI%"
if not exist "dist\bcc" mkdir "dist\bcc"
set "R2A=0" & set "R2B=0" & set "R2C=0" & set "R2D=0"

echo.
echo --- implib -c dist\bcc\webui-2.lib dist\webui-2.dll
implib -c dist\bcc\webui-2.lib dist\webui-2.dll
set "R2A=%ERRORLEVEL%"
echo implib rc=%R2A%

echo.
echo --- tlib dist\bcc\webui-2.lib, dist\bcc\webui-2.lst
tlib dist\bcc\webui-2.lib, dist\bcc\webui-2.lst
set "R2B=%ERRORLEVEL%"
echo tlib rc=%R2B%

echo.
echo --- symbols found in dist\bcc\webui-2.lst (saved to step-2-symbols.txt)
if exist "dist\bcc\webui-2.lst" goto step2_haslst
echo (no .lst file, nothing to search)
echo no .lst file> "%LOGDIR%\step-2-symbols.txt"
set "R2C=1"
goto step2_us
:step2_haslst
findstr /i /l /c:"webui_new_window" /c:"webui_bind" /c:"webui_show" /c:"webui_wait" /c:"webui_get_string" /c:"webui_return_string" /c:"webui_set_config" "dist\bcc\webui-2.lst" > "%LOGDIR%\step-2-symbols.txt" 2>&1
set "R2C=%ERRORLEVEL%"
call :showfile "%LOGDIR%\step-2-symbols.txt"
echo findstr rc=%R2C%  (0 = matches found, 1 = none)

:step2_us
echo.
echo --- extra variant: implib -a -c dist\bcc\webui-2-us.lib dist\webui-2.dll
implib -a -c dist\bcc\webui-2-us.lib dist\webui-2.dll
set "R2D=%ERRORLEVEL%"
echo implib -a rc=%R2D%
if not exist "dist\bcc\webui-2-us.lib" goto step2_done
tlib dist\bcc\webui-2-us.lib, dist\bcc\webui-2-us.lst
echo tlib rc=%ERRORLEVEL%
if not exist "dist\bcc\webui-2-us.lst" goto step2_done
echo --- symbols found in dist\bcc\webui-2-us.lst (saved to step-2-symbols-us.txt)
findstr /i /l /c:"webui_new_window" /c:"webui_bind" /c:"webui_show" /c:"webui_wait" /c:"webui_get_string" /c:"webui_return_string" /c:"webui_set_config" "dist\bcc\webui-2-us.lst" > "%LOGDIR%\step-2-symbols-us.txt" 2>&1
call :showfile "%LOGDIR%\step-2-symbols-us.txt"

:step2_done
popd
echo.
echo NOTE: BCC adds a leading underscore to cdecl names. Compare the names above
echo       with webui_new_window and _webui_new_window. Nothing is decided here.
set "NOTE2=implib=%R2A% tlib=%R2B% symbols=%R2C% implib-a=%R2D%"
if not "%R2A%"=="0" exit /b %R2A%
if not "%R2B%"=="0" exit /b %R2B%
if not "%R2C%"=="0" exit /b %R2C%
exit /b 0


rem ----------------------------------------------------------------------
rem Step 3: dumpbin (decisive when available; it ships with MSVC, not BCC)
rem ----------------------------------------------------------------------
:step3
echo STEP 3: dumpbin (or tdump) on webui-2.dll
echo NOTE: the export names still need to be confirmed from the vs2019 window with
echo       dumpbin /exports %WEBUI%\dist\webui-2.dll
echo       Output from BCC tools below is only a hint.
echo.
where dumpbin >nul 2>&1
if not errorlevel 1 goto step3_dumpbin
where tdump >nul 2>&1
if not errorlevel 1 goto step3_tdump
echo SKIPPED: neither dumpbin nor tdump is on PATH.
echo dumpbin ships with Visual Studio, tdump with BCC. No call to vcvarsall is made.
set "NOTE3=SKIPPED, no dumpbin or tdump on PATH"
exit /b 0

:step3_dumpbin
if exist "%WEBUI%\dist\webui-2.dll" goto step3_dumpbin_run
echo [ERROR] DLL not found: %WEBUI%\dist\webui-2.dll
set "NOTE3=webui-2.dll missing"
exit /b 1
:step3_dumpbin_run
echo --- dumpbin /exports
dumpbin /exports "%WEBUI%\dist\webui-2.dll"
set "R3A=%ERRORLEVEL%"
echo dumpbin /exports rc=%R3A%
echo.
echo --- dumpbin /dependents
dumpbin /dependents "%WEBUI%\dist\webui-2.dll"
set "R3B=%ERRORLEVEL%"
echo dumpbin /dependents rc=%R3B%
set "NOTE3=exports=%R3A% dependents=%R3B%"
if not "%R3A%"=="0" exit /b %R3A%
exit /b %R3B%

:step3_tdump
if exist "%WEBUI%\dist\webui-2.dll" goto step3_tdump_run
echo [ERROR] DLL not found: %WEBUI%\dist\webui-2.dll
set "NOTE3=webui-2.dll missing"
exit /b 1
:step3_tdump_run
echo dumpbin not found, fallback: tdump -ee
tdump -ee "%WEBUI%\dist\webui-2.dll"
set "R3A=%ERRORLEVEL%"
echo tdump -ee rc=%R3A%
set "NOTE3=dumpbin missing, tdump -ee rc=%R3A%, confirm with dumpbin"
exit /b %R3A%


rem ----------------------------------------------------------------------
rem Step 4: link probe (decisive)
rem probe\link_probe.c calls webui_new_window and webui_wait. It does NOT
rem show a window and is never run; it only tests compile and link.
rem hbmk2 knows the BCC link line. -nohblib avoids Harbour libs and their
rem own main. Attempt A uses webui-2.lib, attempt B uses webui-2-us.lib.
rem ----------------------------------------------------------------------
:step4
echo STEP 4: link probe, compile and link only (no window is shown, not run)
echo Removing stale outputs of this step first.
for %%F in (step-4-A.txt step-4-B.txt step-4-unresolved.txt link_probe_A.exe link_probe_B.exe link_probe.obj) do if exist "%LOGDIR%\%%F" del "%LOGDIR%\%%F"
for %%F in (link_probe_A link_probe_B) do for %%X in (tds map ilc ild ilf ils) do if exist "%LOGDIR%\%%F.%%X" del "%LOGDIR%\%%F.%%X"
set "R4C=-" & set "R4A=-" & set "R4B=-"

echo.
echo --- bcc32 -c -A (C89 compile check of probe\link_probe.c)
bcc32 -c -A "-n%LOGDIR%" "%SCRIPT_DIR%probe\link_probe.c"
set "R4C=%ERRORLEVEL%"
echo bcc32 rc=%R4C%

if not defined HBMK2 goto step4_nohbmk2
echo.
echo --- attempt A: hbmk2 with webui-2.lib
"%HBMK2%" -trace -info -rebuild -nohblib "%SCRIPT_DIR%probe\link_probe.c" "-L%WEBUI%\dist\bcc" -lwebui-2 "-o%LOGDIR%\link_probe_A.exe" > "%LOGDIR%\step-4-A.txt" 2>&1
set "R4A=%ERRORLEVEL%"
type "%LOGDIR%\step-4-A.txt"
echo hbmk2 attempt A rc=%R4A%

echo.
echo --- attempt B: hbmk2 with webui-2-us.lib (only if that library exists)
if exist "%WEBUI%\dist\bcc\webui-2-us.lib" goto step4_runb
echo SKIPPED: %WEBUI%\dist\bcc\webui-2-us.lib does not exist.
goto step4_unres
:step4_runb
"%HBMK2%" -trace -info -rebuild -nohblib "%SCRIPT_DIR%probe\link_probe.c" "-L%WEBUI%\dist\bcc" -lwebui-2-us "-o%LOGDIR%\link_probe_B.exe" > "%LOGDIR%\step-4-B.txt" 2>&1
set "R4B=%ERRORLEVEL%"
type "%LOGDIR%\step-4-B.txt"
echo hbmk2 attempt B rc=%R4B%

:step4_unres
echo.
echo --- unresolved externals (saved to step-4-unresolved.txt)
type nul > "%LOGDIR%\step-4-unresolved.txt"
if exist "%LOGDIR%\step-4-A.txt" findstr /i /c:"unresolved" /c:"undefined" "%LOGDIR%\step-4-A.txt" >> "%LOGDIR%\step-4-unresolved.txt" 2>&1
if exist "%LOGDIR%\step-4-B.txt" findstr /i /c:"unresolved" /c:"undefined" "%LOGDIR%\step-4-B.txt" >> "%LOGDIR%\step-4-unresolved.txt" 2>&1
call :showfile "%LOGDIR%\step-4-unresolved.txt"

set "NOTE4=compile=%R4C% A=%R4A% B=%R4B%"
if "%R4A%"=="0" exit /b 0
if "%R4B%"=="0" set "LIB5=webui-2-us"
if "%R4B%"=="0" exit /b 0
if not "%R4A%"=="-" exit /b %R4A%
exit /b 1
:step4_nohbmk2
echo [ERROR] hbmk2.exe not found, link probe cannot run.
set "NOTE4=hbmk2 missing, compile=%R4C%"
exit /b 1


rem ----------------------------------------------------------------------
rem Step 5: build the PoC against the DLL import library, run it (decisive)
rem Not redirected as a whole, because pause needs the console. Output goes
rem to step-5.log and run.log. You click in the window, then close it.
rem The output is hbwebui_bcc.exe so the MSVC hbwebui.exe is never overwritten.
rem ----------------------------------------------------------------------
:step5
set "S5=%LOGDIR%\step-5.log"
> "%S5%" echo STEP 5: PoC with DLL route, import library %LIB5%
call :say5 "Import library used: %LIB5% (webui-2-us only if step 4 needed it)"

if exist "%WEBUI%\dist\webui-2.dll" goto step5_havedll
call :say5 "[ERROR] DLL not found: %WEBUI%\dist\webui-2.dll"
set "NOTE5=webui-2.dll missing"
exit /b 1
:step5_havedll
if defined HBMK2 goto step5_havehbmk2
call :say5 "[ERROR] hbmk2.exe not found."
set "NOTE5=hbmk2 missing"
exit /b 1
:step5_havehbmk2
pushd "%ROOT%"
if exist "hbwebui_bcc.exe" del "hbwebui_bcc.exe"
call :say5 "--- build: hbmk2 -st with %LIB5%, output hbwebui_bcc.exe (MSVC hbwebui.exe is not touched)"
>> "%S5%" echo "%HBMK2%" -st prg\main.prg prg\hello.prg src\bridge.c -I..\webui\include "-L%WEBUI%\dist\bcc" -l%LIB5% -ohbwebui_bcc.exe
"%HBMK2%" -st prg\main.prg prg\hello.prg src\bridge.c -I..\webui\include "-L%WEBUI%\dist\bcc" -l%LIB5% -ohbwebui_bcc.exe >> "%S5%" 2>&1
set "R5A=%ERRORLEVEL%"
call :say5 "hbmk2 rc=%R5A%"
if not "%R5A%"=="0" goto step5_buildfail
if exist "hbwebui_bcc.exe" goto step5_built
call :say5 "[ERROR] hbmk2 returned 0 but hbwebui_bcc.exe was not created."
popd
set "NOTE5=build: no exe"
exit /b 1
:step5_buildfail
call :say5 "[ERROR] Build failed, see step-5.log. The exe will not be started."
popd
set "NOTE5=build failed rc=%R5A%"
exit /b %R5A%
:step5_built
copy /y "%WEBUI%\dist\webui-2.dll" "%ROOT%\webui-2.dll" >> "%S5%" 2>&1
set "R5C=%ERRORLEVEL%"
call :say5 "copy webui-2.dll next to hbwebui_bcc.exe rc=%R5C%"
echo.
echo Starting hbwebui_bcc.exe now. Click the button in the window a few times,
echo then close the window. This script waits until hbwebui_bcc.exe exits.
echo Output goes to %LOGDIR%\run.log
echo If the window does not appear or the program hangs, press Ctrl+C. cmd then
echo asks "Terminate batch job (Y/N)?" - answer N, the script continues and the
echo summary is still written. Answering Y ends the whole script with no summary.
echo.
> "%LOGDIR%\run.log" echo run started %DATE% %TIME% in %CD%
hbwebui_bcc.exe >> "%LOGDIR%\run.log" 2>&1
set "R5B=%ERRORLEVEL%"
>> "%LOGDIR%\run.log" echo run ended %DATE% %TIME% exit code %R5B%
call :say5 "hbwebui_bcc.exe exit code=%R5B% (details in run.log)"
popd
echo.
echo hbwebui_bcc.exe finished with exit code %R5B%. Read the output above if any.
pause
set "NOTE5=build=%R5A% copy=%R5C% run=%R5B%"
if not "%R5C%"=="0" exit /b %R5C%
exit /b %R5B%

:say5
echo %~1
>> "%S5%" echo %~1
exit /b 0


rem ----------------------------------------------------------------------
rem Step 6: hbmk2 -trace -info for the PoC build (not decisive, record)
rem Runs twice into step-6.log: default VM mode and with -st. The exes go to
rem the log folder, not to the PoC root. The VM mode verdict is a heuristic:
rem hbvmmt in the link line means MT, otherwise ST.
rem ----------------------------------------------------------------------
:step6
echo STEP 6: hbmk2 -trace -info for the PoC build (record only)
if defined HBMK2 goto step6_run
echo [ERROR] hbmk2.exe not found.
set "NOTE6=hbmk2 missing"
exit /b 1
:step6_run
for %%F in (probe6a.exe probe6b.exe step-6a.txt step-6b.txt) do if exist "%LOGDIR%\%%F" del "%LOGDIR%\%%F"
pushd "%ROOT%"
echo.
echo --- run 6a: default VM mode (no -st, no -mt)
"%HBMK2%" -trace -info -rebuild prg\main.prg prg\hello.prg src\bridge.c -I..\webui\include "-L%WEBUI%\dist\bcc" -l%LIB5% "-o%LOGDIR%\probe6a.exe" > "%LOGDIR%\step-6a.txt" 2>&1
set "R6A=%ERRORLEVEL%"
type "%LOGDIR%\step-6a.txt"
echo hbmk2 run 6a rc=%R6A%
findstr /i /c:"hbvmmt" "%LOGDIR%\step-6a.txt" >nul 2>&1
set "MODE6A=ST"
if not errorlevel 1 set "MODE6A=MT"

echo.
echo --- run 6b: with -st
"%HBMK2%" -trace -info -rebuild -st prg\main.prg prg\hello.prg src\bridge.c -I..\webui\include "-L%WEBUI%\dist\bcc" -l%LIB5% "-o%LOGDIR%\probe6b.exe" > "%LOGDIR%\step-6b.txt" 2>&1
set "R6B=%ERRORLEVEL%"
type "%LOGDIR%\step-6b.txt"
echo hbmk2 run 6b rc=%R6B%
findstr /i /c:"hbvmmt" "%LOGDIR%\step-6b.txt" >nul 2>&1
set "MODE6B=ST"
if not errorlevel 1 set "MODE6B=MT"
popd

echo.
echo VM mode guess: default=%MODE6A%  with -st=%MODE6B%  (heuristic: hbvmmt in link line = MT)
set "NOTE6=6a=%R6A% (%MODE6A%) 6b=%R6B% (%MODE6B%)"
if not "%R6A%"=="0" exit /b %R6A%
exit /b %R6B%
