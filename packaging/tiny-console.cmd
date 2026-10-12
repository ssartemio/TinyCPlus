@echo off
rem Copyright (c) 2026 TinyC+ contributors
rem SPDX-License-Identifier: Apache-2.0
rem Keep this environment active in the cmd /K console, even without user PATH integration.
set "PATH=%~dp0;%PATH%"
cd /d "%~dp0.."
if errorlevel 1 exit /b 1
"%~dp0tiny.exe" --version
if errorlevel 1 exit /b 1
echo.
echo Try: tiny run examples\hello.tc
echo Expected output: 30
echo Edit: tinyedit examples\hello.tc
echo.
