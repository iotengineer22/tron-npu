@echo off
setlocal
echo Add Arm GCC Toolchain to PATH...
set "PATH=%PATH%;C:\Renesas\RA\e2studio_v2026-04.2_fsp_v6.5.0\toolchains\gcc_arm\13.2.rel1\bin"

cd /d "%~dp0Debug"
echo Starting build in Debug configuration...
"C:\Renesas\RA\e2studio_v2026-04.2_fsp_v6.5.0\eclipse\plugins\com.renesas.ide.exttools.gnumake.win32.x86_64_4.3.1.v20240909-0854\mk\make.exe" -j12 all
if %ERRORLEVEL% equ 0 (
    echo.
    echo =======================================
    echo Build Succeeded!
    echo =======================================
) else (
    echo.
    echo =======================================
    echo Build Failed!
    echo =======================================
)
