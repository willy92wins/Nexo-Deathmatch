@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0dayz-test.ps1" -Mod LFGungame -Mode all -Build -BaseMods "@CF;@LBmaster_Core;@LBmaster_Groups" %*
pause
