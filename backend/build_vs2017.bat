@echo off
echo ========================================
echo  量化选股工具 - 后端构建脚本 (VS 2017)
echo ========================================
echo.

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [错误] 未找到 cmake，请先安装 CMake
    pause
    exit /b 1
)

echo [1/3] 创建构建目录...
if not exist build mkdir build
cd build

echo.
echo [2/3] 生成项目 (Visual Studio 15 2017 Win64)...
cmake .. -G "Visual Studio 15 2017 Win64"
if %errorlevel% neq 0 (
    echo [错误] CMake 生成失败
    pause
    exit /b 1
)

echo.
echo [3/3] 编译项目...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo.
echo ========================================
echo  构建成功！
echo  可执行文件: build\Release\quant-server.exe
echo ========================================
pause
