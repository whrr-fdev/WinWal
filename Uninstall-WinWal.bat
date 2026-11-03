@echo off
chcp 65001 >nul
title Удаление WinWal — Windows 10 Control Center
echo Остановка запущенных процессов WinWal...
taskkill /F /IM WinWal.exe >nul 2>&1
timeout /t 1 /nobreak >nul

echo Сброс маркера первого запуска...
del /F /Q "%USERPROFILE%\.config\hypr\scripts\.winwal_first_init_v3.done" >nul 2>&1

echo Удаление автозапуска и записей реестра...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WinWal" /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\WinWal" /f >nul 2>&1

echo Удаление ярлыков с Рабочего стола и из меню Пуск...
del /F /Q "%USERPROFILE%\Desktop\WinWal.lnk" >nul 2>&1
rmdir /S /Q "%APPDATA%\Microsoft\Windows\Start Menu\Programs\WinWal" >nul 2>&1

if exist "%LOCALAPPDATA%\Programs\WinWal" (
    echo Удаление установленной копии из %LOCALAPPDATA%\Programs\WinWal...
    rmdir /S /Q "%LOCALAPPDATA%\Programs\WinWal" >nul 2>&1
)

echo WinWal успешно удалён из системы!
pause