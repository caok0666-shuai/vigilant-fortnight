@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo.
echo ==========================================
echo   Community Smart Bookshelf - Backend
echo ==========================================
echo.

set "PYTHON_EXE="

REM Prefer common Anaconda / Miniconda installations.
for %%P in (
    "%USERPROFILE%\anaconda3\python.exe"
    "%USERPROFILE%\miniconda3\python.exe"
    "%LOCALAPPDATA%\anaconda3\python.exe"
    "%LOCALAPPDATA%\miniconda3\python.exe"
    "%ProgramData%\anaconda3\python.exe"
    "%ProgramData%\miniconda3\python.exe"
) do (
    if not defined PYTHON_EXE if exist "%%~P" (
        "%%~P" -c "import flask" >nul 2>nul
        if not errorlevel 1 set "PYTHON_EXE=%%~P"
    )
)

if defined PYTHON_EXE goto :run_conda

REM Try the Windows Python launcher.
where py >nul 2>nul
if errorlevel 1 goto :try_python
py -3 -c "import flask" >nul 2>nul
if errorlevel 1 goto :try_python
echo [INFO] Using: py -3
py -3 app.py
goto :end

:try_python
REM Try python on PATH.
where python >nul 2>nul
if errorlevel 1 goto :no_python
python -c "import flask" >nul 2>nul
if errorlevel 1 goto :no_python
echo [INFO] Using: python
python app.py
goto :end

:run_conda
echo [INFO] Using: %PYTHON_EXE%
"%PYTHON_EXE%" app.py
goto :end

:no_python
echo [ERROR] No Python environment with Flask was found.
echo.
echo Please install dependencies with one of these commands:
echo   "%USERPROFILE%\anaconda3\python.exe" -m pip install -r requirements.txt
echo   py -3 -m pip install -r requirements.txt
echo.
echo If Python is not installed, install Python 3.9+ and check
echo "Add Python to PATH" during installation.

:end
pause
