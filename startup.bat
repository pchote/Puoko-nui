@echo off

REM Test whether ds9 is open
set open=0
for /F "delims=" %%i in ('xpaaccess -n Online_Preview') do set open=%%i
if %open% equ 0 (
    START "Online_Preview" /B ds9 -title Online_Preview
)