@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%F in (*.mp4 *.mov *.mkv *.m4v) do (
    set "vcodec="
    set "vwidth="
    set "vheight="
    set "vfps="
    set "acodec="
    set "ach="
    set "asr="

    for /f "usebackq tokens=1-4 delims=," %%a in (`
        ffprobe -v error -select_streams v^:0 ^
        -show_entries "stream=codec_name,width,height,r_frame_rate" ^
        -of "csv=p=0:s=," "%%F"
    `) do (
        set "vcodec=%%a"
        set "vwidth=%%b"
        set "vheight=%%c"
        set "vfps=%%d"
    )

    for /f "usebackq tokens=1-3 delims=," %%a in (`
        ffprobe -v error -select_streams a^:0 ^
        -show_entries "stream=codec_name,channels,sample_rate" ^
        -of "csv=p=0:s=," "%%F"
    `) do (
        set "acodec=%%a"
        set "ach=%%b"
        set "asr=%%c"
    )

    if not defined vcodec set "vcodec=NA" & set "vwidth=NA" & set "vheight=NA" & set "vfps=NA"
    if not defined acodec set "acodec=NA" & set "ach=NA" & set "asr=NA"

    echo %%~nxF ^| !vcodec! !vwidth!x!vheight!@!vfps! ^| !acodec! !ach!ch !asr!Hz
)

pause
