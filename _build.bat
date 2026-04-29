@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /std:c++17 /W3 /O2 /D_UNICODE /DUNICODE main.cpp gui.cpp scheduler.cpp /Fe:scheduler_rr_srtf.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib
