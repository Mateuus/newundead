For /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set date=%%a-%%b-%%c)
"C:\Program Files (x86)\Windows Kits\8.0\Debuggers\x64\symstore.exe" add /r /f C:\WarZ\bin\*.* /s C:\symbol_cache /t "WarZ" /v "ver1.0" /c "%date% build"
"C:\Program Files (x86)\Windows Kits\8.0\Debuggers\x64\symstore.exe" add /r /f C:\WarZ\server\bin\*.* /s C:\symbol_cache /t "WarZServer" /v "ver1.0" /c "%date% build"

pause