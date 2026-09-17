@echo off
rem Opens the benchmark video in a clean, isolated browser profile (no extensions, no other tabs).
rem --autoplay-policy lets the video start without a click so the run is repeatable.
set "URL=https://www.youtube.com/watch?v=V5Asw04DsD8"
set "PROFILE=%TEMP%\yt-bench-profile"
set "FLAGS=--user-data-dir="%PROFILE%" --no-first-run --no-default-browser-check --autoplay-policy=no-user-gesture-required --new-window --window-size=1040,760"
set "CHROME=%ProgramFiles%\Google\Chrome\Application\chrome.exe"
if exist "%CHROME%" (
    start "" "%CHROME%" %FLAGS% "%URL%"
) else (
    start "" msedge %FLAGS% "%URL%"
)
