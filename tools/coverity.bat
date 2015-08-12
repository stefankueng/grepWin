@echo off
setlocal

pushd %~dp0

rem you can set the COVDIR variable to your coverity path
if not defined COVDIR set "COVDIR=C:\cov-analysis"
if defined COVDIR if not exist "%COVDIR%" (
  echo.
  echo ERROR: Coverity not found in "%COVDIR%"
  goto End
)

rem add the tools paths in PATH
set "NANT_PATH=C:\nant\bin"
set "PERL_PATH=C:\Perl\perl\bin"
set "PYTHON_PATH=C:\Python27"
set "PATH=%NANT_PATH%;%PERL_PATH%;%PYTHON_PATH%;%PATH%"


:cleanup
if exist "cov-int" rd /q /s "cov-int"
if exist "grepWin.lzma" del "grepWin.lzma"
if exist "grepWin.tar"  del "grepWin.tar"
if exist "grepWin.tgz"  del "grepWin.tgz"


:main
call "%VS140COMNTOOLS%\vsvars32.bat"
if %ERRORLEVEL% neq 0 (
  echo vsvars32.bat call failed.
  goto End
)

rem the actual coverity command
title "%COVDIR%\bin\cov-build.exe" --dir "cov-int" nant -buildfile:../default.build grepWin
"%COVDIR%\bin\cov-build.exe" --dir "cov-int" nant -buildfile:../default.build grepWin


:tar
rem try the tar tool in case it's in PATH
set PATH=C:\MSYS\bin;%PATH%
tar --version 1>&2 2>nul || (echo. & echo ERROR: tar not found & goto SevenZip)
title Creating "grepWin.lzma"...
tar caf "grepWin.lzma" "cov-int"
goto End


:SevenZip
call :SubDetectSevenzipPath

rem Coverity is totally bogus with lzma...
rem And since I cannot replicate the arguments with 7-Zip, just use tar/gzip.
if exist "%SEVENZIP%" (
  title Creating "grepWin.tar"...
  "%SEVENZIP%" a -ttar "grepWin.tar" "cov-int"
  "%SEVENZIP%" a -tgzip "grepWin.tgz" "grepWin.tar"
  if exist "grepWin.tar" del "grepWin.tar"
  goto End
)


:SubDetectSevenzipPath
for %%g in (7z.exe) do (set "SEVENZIP_PATH=%%~$path:g")
if exist "%SEVENZIP_PATH%" (set "SEVENZIP=%SEVENZIP_PATH%" & exit /b)

for %%g in (7za.exe) do (set "SEVENZIP_PATH=%%~$path:g")
if exist "%SEVENZIP_PATH%" (set "SEVENZIP=%SEVENZIP_PATH%" & exit /b)

for /f "tokens=2*" %%a in (
  'reg query "HKLM\SOFTWARE\7-Zip" /v "path" 2^>nul ^| find "REG_SZ" ^|^|
   reg query "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "path" 2^>nul ^| find "REG_SZ"') do set "SEVENZIP=%%b\7z.exe"
exit /b


:End
popd
echo. & echo Press any key to close this window...
pause >nul
endlocal
exit /b
