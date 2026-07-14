@echo off
setlocal

rem The Codex terminal can contain both PATH and Path. MSBuild rejects that environment.
set "PATH="
set "Path="
set "PATH=C:\Windows\System32"

set "CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

"%CMAKE%" -S "%~dp0." -B "%~dp0build-full-ok" -G "Visual Studio 17 2022" -A x64 -DCLASSRANKER_WITH_XLSX=ON -DCMAKE_PREFIX_PATH=C:\Users\Bobily\Qt\6.8.3\msvc2022_64
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%~dp0build-full-ok" --config Release
exit /b %errorlevel%
