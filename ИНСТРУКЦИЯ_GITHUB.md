================================================================================
🚀 ШПАРГАЛКА: КАК СОБИРАТЬ И ВЫПУСКАТЬ ОБНОВЛЕНИЯ WINWAL НА GITHUB
================================================================================

Репозиторий программы: https://github.com/whrr-fdev/WinWal
Репозиторий обоев:     https://github.com/whrr-fdev/wallpapers

--------------------------------------------------------------------------------
1. КАК ВЫПУСТИТЬ ОБНОВЛЕНИЕ ПРОГРАММЫ (ЧТОБЫ ПРИЛЕТЕЛО ВСЕМ ПОЛЬЗОВАТЕЛЯМ)
--------------------------------------------------------------------------------
Когда ты изменил код в C:\WinWal\winwal и хочешь выпустить новую версию
(например, 1.2.1), открой PowerShell и выполни:

# Шаг А: Собрать новый WinWal.exe и скопировать в портативную папку
$env:PATH = "C:\winwal-sdk\6.8.2\mingw_64\bin;C:\winwal-sdk\Tools\mingw1310_64\bin;C:\Program Files\Git\cmd;" + $env:PATH
cmake --build C:\WinWal\winwal\build --config Release -j 8
Copy-Item "C:\WinWal\winwal\build\WinWal.exe" "C:\WinWal\WinWal-Portable\WinWal.exe" -Force

# Шаг Б: Обновить номер версии в version.txt (например, на 1.2.1) и запушить
cd C:\WinWal
"1.2.1" | Set-Content "C:\WinWal\version.txt" -Encoding UTF8
"1.2.1" | Set-Content "C:\WinWal\WinWal-Portable\version.txt" -Encoding UTF8
git add -f version.txt WinWal-Portable/WinWal.exe about.md
git commit -m "Update v1.2.1"
git push

Готово! Сразу после этого у всех пользователей при нажатии «Проверить обновления»
в WinWal высветится новая версия v1.2.1, и она установится в 1 клик с сохранением
всех настроек.

--------------------------------------------------------------------------------
2. КАК ПЕРЕСОБРАТЬ УСТАНОВЩИК WinWal-Setup.exe (ДЛЯ НОВЫХ ПОЛЬЗОВАТЕЛЕЙ)
--------------------------------------------------------------------------------
Если нужно обновить сам инсталлятор C:\WinWal\WinWal-Setup.exe:

$zipPath = "C:\WinWal\WinWal-Portable.zip"
Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
tar.exe -a -cf $zipPath -C "C:\WinWal\WinWal-Portable" .
$csc = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
& $csc /nologo /target:winexe /out:"C:\WinWal\WinWal-Setup.exe" "/win32icon:C:\WinWal\winwal\assets\winwal.ico" /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll "/res:$zipPath,WinWalPayload.zip" "C:\WinWal\winwal\build\WinWalSetup.cs"

--------------------------------------------------------------------------------
3. КАК ОПУБЛИКОВАТЬ РЕЛИЗ (RELEASE) С УСТАНОВЩИКОМ НА GITHUB
--------------------------------------------------------------------------------
Вариант А (через браузер за 15 секунд):
  1) Открой: https://github.com/whrr-fdev/WinWal/releases/new?tag=v1.2.0&title=WinWal+v1.2.0
  2) Перетащи туда файлы:
     - C:\WinWal\WinWal-Setup.exe
     - C:\WinWal\WinWal-Portable\WinWal.exe
  3) Нажми зелёную кнопку «Publish release».

Вариант Б (через консоль с помощью GitHub CLI):
  gh release create v1.2.0 "C:\WinWal\WinWal-Setup.exe" "C:\WinWal\WinWal-Portable\WinWal.exe" --title "WinWal v1.2.0" --notes "Release v1.2.0"
================================================================================