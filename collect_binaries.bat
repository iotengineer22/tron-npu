@echo off
echo ===================================================
echo   Collecting built binaries to debug/ directory...
echo ===================================================
python "%~dp0collect_binaries.py"
echo.
echo Done. Press any key to exit.
pause > nul
