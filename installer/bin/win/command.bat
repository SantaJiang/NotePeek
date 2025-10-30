@echo off
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
echo 正在复制文件到脚本所在目录: %SCRIPT_DIR%

xcopy "C:\Qt\Tools\QtInstallerFramework\4.10\bin\*.*" "%SCRIPT_DIR%" /Y /I /F

if %errorlevel% equ 0 (
    echo 文件复制完成！
) else (
    echo 复制过程中出现错误，错误代码：%errorlevel%
)

pause