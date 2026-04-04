@echo off
setlocal ENABLEDELAYEDEXPANSION

:: Change to script directory
pushd "%~dp0" || (
    echo Failed to change to script directory.
    exit /b 1
)

:: Initialize ESC character for ANSI colors
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
set "G=%ESC%[92m"
set "Y=%ESC%[93m"
set "R=%ESC%[91m"
set "N=%ESC%[0m"

:: Parse --debug flag
set "BUILD_TYPE=RelWithDebInfo"
set "LTO_FLAG=-DENABLE_LTO=ON"
for %%A in (%*) do (
    if /I "%%A"=="--debug" (
        set "BUILD_TYPE=Debug"
        set "LTO_FLAG=-DENABLE_LTO=OFF"
    )
)

title "3sx - Compile (%BUILD_TYPE%)"
echo %G%============================================%N%
echo %G%   3sx - Compile (%BUILD_TYPE%)%N%
echo %G%============================================%N%
echo.

:: Find MSYS2
set "MSYS2_ROOT="
for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\MSYS2 64bit" /v InstallLocation 2^>nul') do set "MSYS2_ROOT=%%B"

if "%MSYS2_ROOT%"=="" (
    if exist "C:\msys64\msys2_shell.cmd" set "MSYS2_ROOT=C:\msys64"
    if exist "C:\msys2\msys2_shell.cmd"  set "MSYS2_ROOT=C:\msys2"
)

if "%MSYS2_ROOT%"=="" (
    echo %R%[X] MSYS2 not found. Please install MSYS2 first.%N%
    popd
    exit /b 1
)

if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo %R%[X] MSYS2 bash not found at "%MSYS2_ROOT%\usr\bin\bash.exe".%N%
    popd
    exit /b 1
)

echo %G%[V] Using MSYS2 at: "%MSYS2_ROOT%"%N%
echo.

:: Set up environment
set "REPO_DIR=%~dp0"
set "MSYSTEM=MINGW64"
set "CHERE_INVOKING=1"

:: Convert Windows path to Unix path
set "UNIX_REPO_DIR=%REPO_DIR:\=/%"
set "UNIX_REPO_DIR=/%UNIX_REPO_DIR::=%"

:: Remove trailing slash if present
if "%UNIX_REPO_DIR:~-1%"=="/" set "UNIX_REPO_DIR=%UNIX_REPO_DIR:~0,-1%"

echo %Y%[*] Building in: %REPO_DIR% (%BUILD_TYPE%)%N%
echo.

:: Build commands
set "DEP_CMD=pacman -S --noconfirm --needed make mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-headers-git sed mingw-w64-x86_64-llvm mingw-w64-x86_64-lld mingw-w64-x86_64-miniupnpc"
set "PATCH_CMD=if [ -f zlib/infblock.c ]; then sed -i 's/%%lu/%%llu/g' zlib/infblock.c; echo 'Applied zlib patch'; fi"
set "CLEAN_CMD=rm -rf build"
set "BUILD_CMD=sh build-deps.sh && %CLEAN_CMD% && CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DENABLE_TESTS=OFF -DENABLE_TRACY=ON %LTO_FLAG% -DCMAKE_RANLIB=llvm-ranlib -DCMAKE_AR=llvm-ar -DCMAKE_NM=llvm-nm -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_MODULE_LINKER_FLAGS="-fuse-ld=lld" && cmake --build build --parallel && cmake --install build --prefix build/application"


echo %G%[*] Starting MSYS2 MinGW64 build pipeline...%N%
echo.

"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "cd '%UNIX_REPO_DIR%' && %DEP_CMD% && %PATCH_CMD% && %BUILD_CMD%"
set "BUILD_EXIT=%ERRORLEVEL%"

if %BUILD_EXIT% neq 0 (
    echo.
    echo %R%[X] Build failed with exit code: %BUILD_EXIT%%N%
    popd
    exit /b %BUILD_EXIT%
)

echo.
echo %G%============================================%N%
echo %G%   Build Completed Successfully!            %N%
echo %G%   Location: %N%
echo %Y%   "%REPO_DIR%build\application"%N%
echo %G%============================================%N%

popd
endlocal
exit /b 0
