@echo off
setlocal
rem Forks may not have upstream annotated tags. Use a numeric CI build component.
set "BUILD_NUMBER=%GITHUB_RUN_NUMBER%"
if not defined BUILD_NUMBER set "BUILD_NUMBER=0"
powershell -NoProfile -Command "$build=[int]$env:BUILD_NUMBER; if ($build -lt 0 -or $build -gt 65535) { throw 'Invalid resource build number' }; $path='..\duckstation-qt\duckstation-qt.rc'; $text=Get-Content -LiteralPath $path -Raw; $text=$text.Replace('1,0,0,1',('0,0,{0},0' -f $build)).Replace('1.0.0.1',('0.0.{0}.0' -f $build)); Set-Content -LiteralPath $path -Value $text -Encoding ASCII"
exit /b %errorlevel%
