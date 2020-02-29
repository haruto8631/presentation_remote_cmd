call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cl hid_lib.c /LD
cl joycon_presentation_remote.cpp user32.lib hid_lib.lib /MD /D_AFXDLL
del *.obj
pause