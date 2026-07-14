@echo off
setlocal

rem The Codex terminal can contain both PATH and Path. MSBuild rejects that environment.
set "PATH="
set "Path="
set "PATH=C:\Windows\System32"

set "CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

"%CMAKE%" -S "%~dp0." -B "%~dp0build-csv-ok" -G "Visual Studio 17 2022" -A x64 -DCLASSRANKER_WITH_XLSX=OFF
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%~dp0build-csv-ok" --config Release
exit /b %errorlevel%
