@echo off
setlocal

set OUTFILE=video_list.txt

if exist "%OUTFILE%" del "%OUTFILE%"

for %%F in (*.mp4 *.mov *.mkv *.avi *.m4v *.wmv *.flv) do (
    echo %%~nxF>>"%OUTFILE%"
)

echo Video list saved to %OUTFILE%
pause
