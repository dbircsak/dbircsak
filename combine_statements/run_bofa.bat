@echo off
setlocal

REM ---- Configuration ----
set VENV_DIR=.venv
set SCRIPT=bofa_pdf_to_csv.py

REM ---- If venv does not exist, create and install deps ----
if not exist "%VENV_DIR%\Scripts\python.exe" (
    echo Virtual environment not found. Creating...
    python -m venv "%VENV_DIR%"

    call "%VENV_DIR%\Scripts\activate.bat"

    echo Installing pip tools...
    python -m pip install --upgrade pip setuptools wheel

    echo Installing dependencies...
    python -m pip install pandas pdfplumber
) else (
    REM ---- Activate existing venv ----
    call "%VENV_DIR%\Scripts\activate.bat"
)

REM ---- Run script, pass through args ----
python "%SCRIPT%" %*

echo.
echo Done.
REM pause
