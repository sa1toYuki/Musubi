@echo off
rem # SPDX-License-Identifier: GPL-3.0-or-later
setlocal
cd /d "%~dp0.."

if not defined QT_MSVC_DIR (
    for %%Q in (C:\Qt\6.12.0\msvc2022_64 C:\Qt\6.11.2\msvc2022_64 C:\Qt\6.10.0\msvc2022_64 "%~dp0..\build\deps\Qt\6.12.0\msvc2022_64") do (
        if exist "%%~Q\bin\windeployqt.exe" set "QT_MSVC_DIR=%%~Q"
    )
)
if not defined QT_MSVC_DIR (
    echo ERRO: defina QT_MSVC_DIR para o kit Qt 6 MSVC 2022 x64.
    echo Exemplo: C:\Qt\6.12.0\msvc2022_64
    exit /b 2
)

if not defined VSCMD_VER (
    if exist "C:\VS2022BuildTools\Common7\Tools\VsDevCmd.bat" call "C:\VS2022BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
    if not defined VSCMD_VER if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
)

if exist "C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" set "PATH=C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
if exist "C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja" set "PATH=C:\VS2022BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
if defined VSINSTALLDIR (
    if exist "%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin" set "PATH=%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    if exist "%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja" set "PATH=%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
)
if not exist "cpp\CMakeLists.txt" (
    echo ERRO: cpp\CMakeLists.txt nao foi encontrado.
    exit /b 2
)
where cmake >nul 2>nul || (
    echo ERRO: CMake nao encontrado no PATH.
    exit /b 2
)
where ninja >nul 2>nul || (
    echo ERRO: Ninja nao encontrado no PATH.
    exit /b 2
)
where cl >nul 2>nul || (
    echo ERRO: cl.exe nao encontrado. Inicie este script no Developer PowerShell/Command Prompt do Visual Studio 2022.
    exit /b 2
)
if not exist "%QT_MSVC_DIR%\bin\windeployqt.exe" (
    echo ERRO: windeployqt.exe nao encontrado em "%QT_MSVC_DIR%\bin".
    exit /b 2
)

set "BUILD_DIR=%CD%\build\windows-msvc"
set "DIST_DIR=%CD%\dist\windows"
cmake -S "%CD%\cpp" -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_MSVC_DIR%" ^
    -DCMAKE_INSTALL_PREFIX="%DIST_DIR%"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b %errorlevel%

cmake --install "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b %errorlevel%

if not exist "%DIST_DIR%\Musubi.exe" (
    echo ERRO: Musubi.exe nao foi instalado em "%DIST_DIR%".
    exit /b 1
)

"%QT_MSVC_DIR%\bin\windeployqt.exe" --release --no-translations --skip-plugin-types generic,network,networkinformation,tls --dir "%DIST_DIR%" "%DIST_DIR%\Musubi.exe"
if errorlevel 1 exit /b %errorlevel%

rem Remove arquivos opcionais de rede que podem ter sobrado de uma implanta??o anterior.
if exist "%DIST_DIR%\Qt6Network.dll" del /q "%DIST_DIR%\Qt6Network.dll"
if exist "%DIST_DIR%\networkinformation" rmdir /s /q "%DIST_DIR%\networkinformation"
if exist "%DIST_DIR%\tls" rmdir /s /q "%DIST_DIR%\tls"
if exist "%DIST_DIR%\generic" rmdir /s /q "%DIST_DIR%\generic"

if exist "%CD%\dist\Musubi-windows-x64.zip" del /q "%CD%\dist\Musubi-windows-x64.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%CD%\dist\Musubi-windows-x64.zip' -Force"
if errorlevel 1 exit /b %errorlevel%

echo Build concluido: "%DIST_DIR%\Musubi.exe"
echo Pacote: "%CD%\dist\Musubi-windows-x64.zip"
