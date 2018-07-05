rem Run me as Adminstrator!

@setlocal enableextensions
@cd /d "%~dp0"

net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Requires administrative privilege.
  pause
  exit
)

powershell.exe -NoP -NonI -Command "Expand-Archive -Force '.\LoftyCAD-release.zip' 'c:\Program Files (x86)\LoftyCAD\'
regedit /s LoftyCAD.reg
