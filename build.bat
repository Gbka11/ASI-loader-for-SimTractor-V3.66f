@echo off
REM Run in "x86 Native Tools Command Prompt for VS" (32-bit!)
cl /nologo /LD /O2 /MT src\asi_loader.cpp /Fe:d3d9.dll
pause
