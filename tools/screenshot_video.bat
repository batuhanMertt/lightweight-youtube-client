@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0screenshot.ps1" -Name video -Delay 3 > "%~dp0screenshot_log.txt" 2>&1
