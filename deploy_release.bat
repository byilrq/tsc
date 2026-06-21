@echo off
set DEPLOY_DIR=D:\tsc\build\release
set QT_DIR=C:\QT\5.14.2\mingw73_64

echo Deploying Qt files...

"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY_DIR%\extract.exe"

copy /Y "%QT_DIR%\bin\libstdc++-6.dll" "%DEPLOY_DIR%\"
copy /Y "%QT_DIR%\bin\libgcc_s_seh-1.dll" "%DEPLOY_DIR%\"
copy /Y "%QT_DIR%\bin\libwinpthread-1.dll" "%DEPLOY_DIR%\"

echo Deploy finished.