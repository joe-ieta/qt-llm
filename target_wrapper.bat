@echo off

if defined QTBIN set "QT_BIN=%QTBIN%"

if not defined QT_BIN if defined QTVERSION (
    if /I "%QTVERSION:~0,1%"=="6" (
        if defined QT6_ROOT set "QT_BIN=%QT6_ROOT%\bin"
        if defined QT6_HOME set "QT_BIN=%QT6_HOME%\bin"
        if defined QT6_PATH set "QT_BIN=%QT6_PATH%\bin"
    ) else if /I "%QTVERSION:~0,1%"=="5" (
        if defined QT5_ROOT set "QT_BIN=%QT5_ROOT%\bin"
        if defined QT5_HOME set "QT_BIN=%QT5_HOME%\bin"
        if defined QT5_PATH set "QT_BIN=%QT5_PATH%\bin"
    )
)

if not defined QT_BIN if defined QT6_ROOT set "QT_BIN=%QT6_ROOT%\bin"
if not defined QT_BIN if defined QT5_ROOT set "QT_BIN=%QT5_ROOT%\bin"
if not defined QT_BIN if defined QT_HOME set "QT_BIN=%QT_HOME%\bin"
if not defined QT_BIN set "QT_BIN=E:\Qt\6.10.3\msvc2022_64\bin"
if not exist "%QT_BIN%\Qt6Core.dll" if not exist "%QT_BIN%\Qt5Core.dll" if defined QT6_ROOT if exist "%QT6_ROOT%\bin\Qt6Core.dll" set "QT_BIN=%QT6_ROOT%\bin"
if not exist "%QT_BIN%\Qt6Core.dll" if not exist "%QT_BIN%\Qt5Core.dll" if defined QT5_ROOT if exist "%QT5_ROOT%\bin\Qt5Core.dll" set "QT_BIN=%QT5_ROOT%\bin"

if not exist "%QT_BIN%\Qt6Core.dll" if not exist "%QT_BIN%\Qt5Core.dll" (
    echo Qt runtime not found under "%QT_BIN%". 1>&2
    exit /b 2
)

for %%I in ("%QT_BIN%\..") do set "QT_ROOT=%%~fI"
set "PATH=%QT_BIN%;%PATH%"
if defined CMAKE_PREFIX_PATH (
    set "CMAKE_PREFIX_PATH=%QT_ROOT%;%CMAKE_PREFIX_PATH%"
) else (
    set "CMAKE_PREFIX_PATH=%QT_ROOT%"
)

if "%~1"=="" (
    echo QT_BIN=%QT_BIN%
    echo CMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%
    exit /b 0
)

call %*
exit /b %ERRORLEVEL%

set "PATH=%QT_BIN%;%PATH%"

if defined QT_PLUGIN_PATH (
    set "QT_PLUGIN_PATH=%QT_BIN%\..\plugins;%QT_PLUGIN_PATH%"
) else (
    set "QT_PLUGIN_PATH=%QT_BIN%\..\plugins"
)

if defined CMAKE_PREFIX_PATH (
    set "CMAKE_PREFIX_PATH=%QT_BIN%\..\lib\cmake;%CMAKE_PREFIX_PATH%"
) else (
    set "CMAKE_PREFIX_PATH=%QT_BIN%\..\lib\cmake"
)

%*
