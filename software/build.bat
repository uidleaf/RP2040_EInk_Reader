@echo off
setlocal enabledelayedexpansion
:: RP2040 E-Ink build script - run from CMD, not PowerShell
:: If using PowerShell, run:  cmd /c build.bat

set "PROJECT_DIR=H:\file\墨水屏\RP2040"
set "BUILD_DIR=C:\temp\eink"
set "PICO_SDK_PATH=C:\temp\pico-sdk"

if "%1"=="" set "MODE=build"
if not "%1"=="" set "MODE=%1"

echo === RP2040 E-Ink Build Script ===

:: --- check tools ---
echo --- Checking tools...
where arm-none-eabi-gcc >nul 2>&1 || (echo ERROR: arm-none-eabi-gcc not found & exit /b 1)
where cmake >nul 2>&1           || (echo ERROR: cmake not found & exit /b 1)
where ninja >nul 2>&1           || (echo ERROR: ninja not found & exit /b 1)
where python >nul 2>&1          || (echo ERROR: python not found & exit /b 1)
if not exist "%PICO_SDK_PATH%" (
    echo ERROR: Pico SDK not found at %PICO_SDK_PATH%
    echo Run: git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git %PICO_SDK_PATH%
    exit /b 1
)
echo OK: all tools found

:: --- clean ---
if "%MODE%"=="clean" (
    echo --- Cleaning build directory...
    if exist "%BUILD_DIR%\build" rmdir /s /q "%BUILD_DIR%\build"
    echo Done.
    exit /b 0
)

:: --- sync sources ---
echo --- Syncing sources...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
copy /y "%PROJECT_DIR%\main.c"          "%BUILD_DIR%\main.c"          >nul 2>&1
copy /y "%PROJECT_DIR%\ui.c"            "%BUILD_DIR%\ui.c"            >nul 2>&1
copy /y "%PROJECT_DIR%\ui.h"            "%BUILD_DIR%\ui.h"            >nul 2>&1
copy /y "%PROJECT_DIR%\EPD_2in9.c"      "%BUILD_DIR%\EPD_2in9.c"      >nul 2>&1
copy /y "%PROJECT_DIR%\EPD_2in9.h"      "%BUILD_DIR%\EPD_2in9.h"      >nul 2>&1
copy /y "%PROJECT_DIR%\DEV_Config.c"    "%BUILD_DIR%\DEV_Config.c"    >nul 2>&1
copy /y "%PROJECT_DIR%\DEV_Config.h"    "%BUILD_DIR%\DEV_Config.h"    >nul 2>&1
copy /y "%PROJECT_DIR%\hw_config.c"     "%BUILD_DIR%\hw_config.c"     >nul 2>&1
copy /y "%PROJECT_DIR%\font_gb2312.c"   "%BUILD_DIR%\font_gb2312.c"   >nul 2>&1
copy /y "%PROJECT_DIR%\font_gb2312.h"   "%BUILD_DIR%\font_gb2312.h"   >nul 2>&1
copy /y "%PROJECT_DIR%\CMakeLists.txt"  "%BUILD_DIR%\CMakeLists.txt"  >nul 2>&1
copy /y "%PROJECT_DIR%\pico_sdk_import.cmake" "%BUILD_DIR%\pico_sdk_import.cmake" >nul 2>&1
if exist "%PROJECT_DIR%\lib" robocopy "%PROJECT_DIR%\lib" "%BUILD_DIR%\lib" /e /njh /njs /ndl /nc >nul 2>&1
echo OK: sources synced

:: --- cmake configure ---
if not exist "%BUILD_DIR%\build\build.ninja" (
    echo --- Running cmake configure...
    if not exist "%BUILD_DIR%\build" mkdir "%BUILD_DIR%\build"
    cd /d "%BUILD_DIR%\build"
    cmake -G "Ninja" -DPICO_SDK_PATH="%PICO_SDK_PATH%" -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON "%BUILD_DIR%"
    if errorlevel 1 (echo ERROR: cmake failed & exit /b 1)
    echo OK: cmake done
)

:: --- build ---
echo --- Building with ninja...
cd /d "%BUILD_DIR%\build"
ninja
if errorlevel 1 (echo ERROR: build failed & exit /b 1)
echo OK: build done

:: --- generate uf2 ---
echo --- Generating UF2...
python -c "import struct,os;d=open('eink_reader.bin','rb').read();f=open('eink_reader.uf2','wb');t=(len(d)+255)//256;[f.write(struct.pack('<8I',0x0A324655,0x9E5D5157,0x2000,0x10000000+i*256,256,i,t,0xE48BFF56)+d[i*256:i*256+256].ljust(256,b'\x00')[:256]+struct.pack('<I',0x0AB16F30)) for i in range(t)];print(f'UF2: {t} blocks, {os.path.getsize(\"eink_reader.uf2\")/1024:.1f} KB')"
copy /y "%BUILD_DIR%\build\eink_reader.uf2" "%PROJECT_DIR%\build\eink_reader.uf2" >nul 2>&1

:: --- flash ---
if "%MODE%"=="flash" (
    echo --- Waiting for RPI-RP2 drive...
    echo Hold BOOTSEL, plug USB, release BOOTSEL...
    for /l %%i in (1,1,60) do (
        for /f "tokens=*" %%d in ('powershell -c "(Get-WmiObject Win32_LogicalDisk | Where-Object { $_.VolumeName -eq 'RPI-RP2' }).DeviceID" 2^>nul') do set "D=%%d"
        if not "!D!"=="" (
            echo Found: !D!
            copy /y "%BUILD_DIR%\build\eink_reader.uf2" "!D!\" >nul 2>&1
            echo Flashed! RP2040 will reboot.
            goto done
        )
        timeout /t 1 /nobreak >nul
    )
    echo ERROR: RPI-RP2 not detected
    exit /b 1
)

:done
echo ==========================================
echo  BUILD SUCCESS
echo  UF2: %PROJECT_DIR%\build\eink_reader.uf2
echo ==========================================
exit /b 0
