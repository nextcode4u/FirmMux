@echo off
setlocal

set "SCRIPT_URL=https://raw.githubusercontent.com/nextcode4u/FirmMux/main/tools/firmmux_setup_pc.py"
set "TMP_SCRIPT=%TEMP%\firmmux_setup_pc.py"

echo Downloading latest FirmMux setup script...
where curl >nul 2>nul
if %errorlevel%==0 (
    curl -L --fail -o "%TMP_SCRIPT%" "%SCRIPT_URL%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri '%SCRIPT_URL%' -OutFile '%TMP_SCRIPT%'"
)

if not exist "%TMP_SCRIPT%" (
    echo Failed to download setup script.
    goto :end
)

where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%TMP_SCRIPT%"
    goto :end
)

where python >nul 2>nul
if %errorlevel%==0 (
    python "%TMP_SCRIPT%"
    goto :end
)

echo Python 3 was not found.
echo Install Python 3, then run this launcher again:
echo https://www.python.org/downloads/windows/

:end
echo.
pause
endlocal
