@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "WIDTH=1920"
set "HEIGHT=1080"
set "FPS=30"
set "VIDEO_CRF=20"
set "VIDEO_PRESET=medium"
set "AUDIO_KBPS=192"
set "OUT_NAME=final_youtube.mp4"

set "LIST_IN=video_list.txt"
set "TEMP_DIR=_norm"
set "LIST_OUT=_concat.txt"

if not exist "%LIST_IN%" (
  echo ERROR: "%LIST_IN%" not found. Create it with one filename per line.
  pause
  exit /b 1
)

if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"
if exist "%LIST_OUT%" del "%LIST_OUT%"

echo.
echo === Normalizing clips (no loudness processing) ===

for /f "usebackq delims=" %%F in ("%LIST_IN%") do (
    if not "%%~aF"=="" (
        if exist "%%~fF" (
            set "IN=%%~fF"
            set "OUT=%TEMP_DIR%\%%~nF_norm.mp4"

            rem --- Get duration using ffprobe ---
            for /f "usebackq tokens=* delims=" %%D in (`
                ffprobe -v error -show_entries format^=duration -of default^=noprint_wrappers^=1:nokey^=1 "%%~fF"
            `) do set "DURATION=%%D"

            echo.
            echo Processing: %%~nxF  [Duration: !DURATION! seconds]

            set "VF=scale=w=%WIDTH%:h=%HEIGHT%:force_original_aspect_ratio=decrease,pad=%WIDTH%:%HEIGHT%:(ow-iw)/2:(oh-ih)/2,fps=%FPS%,format=yuv420p"

            set "HASA="
            for /f "usebackq delims=" %%A in (`
                ffprobe -v error -select_streams a^:0 -show_entries stream^=index -of csv^=p^=0 "%%~fF"
            `) do (
                set "HASA=1"
            )

            if defined HASA (
                ffmpeg -y -hide_banner -loglevel error -stats ^
                  -i "%%~fF" ^
                  -map 0:v:0 -map 0:a:0 ^
                  -vf "!VF!" -c:v libx264 -preset %VIDEO_PRESET% -crf %VIDEO_CRF% -pix_fmt yuv420p ^
                  -color_primaries bt709 -color_trc bt709 -colorspace bt709 -movflags +faststart ^
                  -c:a aac -b:a %AUDIO_KBPS%k -ac 2 -ar 48000 ^
                  "!OUT!"
            ) else (
                ffmpeg -y -hide_banner -loglevel error -stats ^
                  -i "%%~fF" -f lavfi -i anullsrc=channel_layout=stereo:sample_rate=48000 ^
                  -shortest ^
                  -map 0:v:0 -map 1:a:0 ^
                  -vf "!VF!" -c:v libx264 -preset %VIDEO_PRESET% -crf %VIDEO_CRF% -pix_fmt yuv420p ^
                  -color_primaries bt709 -color_trc bt709 -colorspace bt709 -movflags +faststart ^
                  -c:a aac -b:a %AUDIO_KBPS%k -ac 2 -ar 48000 ^
                  "!OUT!"
            )

            if errorlevel 1 (
              echo ERROR: Failed to encode "%%~nxF" — skipping.
            ) else (
              echo file '%CD%\!OUT!'>>"%LIST_OUT%"
            )
        ) else (
            echo WARNING: "%%F" not found — skipping.
        )
    )
)

echo.
echo === Concatenating normalized clips ===
if not exist "%LIST_OUT%" (
  echo No normalized clips to join. Exiting.
  pause
  exit /b 1
)

ffmpeg -y -hide_banner -loglevel error -stats ^
  -f concat -safe 0 -i "%LIST_OUT%" -c copy "%OUT_NAME%"

if errorlevel 1 (
  echo ERROR: Concatenation failed unexpectedly.
  pause
  exit /b 1
)

echo.
echo Done.
echo Output: "%OUT_NAME%"
pause
