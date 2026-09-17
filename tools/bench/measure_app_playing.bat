@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0measure.ps1" -Label app_playing -Seconds 30 -Delay 10
timeout /t 5
