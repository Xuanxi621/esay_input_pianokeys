@echo off
chcp 65001 >nul
title EasyInput Piano Web App
echo ====================================================
echo   ★ 正在启动 EasyInput Piano 炫酷网页控制台... ★
echo ====================================================
echo.
echo 正在为你打开 Chrome/Edge 浏览器...
start http://localhost:8080/index.html
cd /d "%~dp0\web"
python -m http.server 8080
pause
