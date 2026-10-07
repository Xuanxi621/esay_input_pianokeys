@echo off
title Restore EasyInput
echo Flashing original easy_input firmware...
powershell -ExecutionPolicy Bypass -File "%~dp0restore_easy_input.ps1"
pause
