# Enable classic Windows context menu (remove "Show more options")
reg.exe add "HKCU\Software\Classes\CLSID\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\InprocServer32" /f /ve
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Process explorer.exe
