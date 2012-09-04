@echo off

REM Test whether ds9 is open
set open=0
for /F "delims=" %%i in ('xpaaccess -n Online_Preview') do set open=%%i
if %open% equ 0 (
    START "Online_Preview" /B ds9 -title Online_Preview
) else (
    REM If ds9 is available, and a frame has been aquired (not startup), then display it
    if not "%1%"=="startup" (
	    xpaset -p Online_Preview file preview.fits.gz
    )
)