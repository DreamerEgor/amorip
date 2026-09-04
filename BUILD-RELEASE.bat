@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Could not find Visual Studio Installer / vswhere.exe.
  echo Open CS2_External.sln in Visual Studio and build Release x64 instead.
  pause
  exit /b 1
)

set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"

if not defined MSBUILD (
  echo MSBuild was not found. Make sure Desktop development with C++ is installed.
  pause
  exit /b 1
)

echo Building Release x64...
"%MSBUILD%" "CS2_External.sln" /t:Rebuild /m /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
  echo.
  echo Build failed. Copy the errors from this window and send them to ChatGPT.
  pause
  exit /b 1
)

set "BUILT_EXE=%~dp0Release\cs2-external.exe"
if not exist "%BUILT_EXE%" (
  echo.
  echo Build completed, but the expected EXE was not found:
  echo   %BUILT_EXE%
  pause
  exit /b 1
)

:GENERATE_NAME
setlocal EnableDelayedExpansion
set "CHARS=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
set "RANDOM_NAME="
for /L %%I in (1,1,16) do (
  set /A "IDX=!RANDOM! %% 62"
  for %%J in (!IDX!) do set "RANDOM_NAME=!RANDOM_NAME!!CHARS:~%%J,1!"
)

if not defined RANDOM_NAME (
  echo.
  echo Could not generate a random output name.
  pause
  exit /b 1
)

if exist "%~dp0Release\!RANDOM_NAME!.exe" (
  endlocal
  goto GENERATE_NAME
)

move /Y "%BUILT_EXE%" "%~dp0Release\!RANDOM_NAME!.exe" >nul
if errorlevel 1 (
  echo.
  echo Build succeeded, but renaming the EXE failed.
  pause
  exit /b 1
)

set "FINAL_NAME=!RANDOM_NAME!"
endlocal & set "RANDOM_NAME=%FINAL_NAME%"

echo.
echo Done. This build was given a random 16-character name:
echo   Release\%RANDOM_NAME%.exe
start "" explorer.exe /select,"%~dp0Release\%RANDOM_NAME%.exe"
exit /b 0
