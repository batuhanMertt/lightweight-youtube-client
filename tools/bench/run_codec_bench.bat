@echo off
rem Runs the full codec benchmark (about 7 minutes). Keep your hands off the mouse.
rem Optional: pass a YouTube video id, e.g. run_codec_bench.bat V5Asw04DsD8
set "VID=%~1"
if "%VID%"=="" set "VID=aqz-KE-bpKQ"
if not exist "%~dp0results" mkdir "%~dp0results"
powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Minimized -File "%~dp0run_codec_bench.ps1" -VideoId %VID% > "%~dp0results\run_codec_bench_%VID%_log.txt" 2>&1
