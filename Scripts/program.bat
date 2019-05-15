@echo off
echo Programming...
%bossa%\bossac.exe --port=%1 -b -U -e -w -v %2 -R
echo END