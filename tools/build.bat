@echo off
echo ---------------------------------------
echo   Vaadivasal NES - Build Process
echo ---------------------------------------

pushd ..

if not exist build mkdir build

echo Assembling...
ca65 src/main.s -o build/main.o -g -I src
IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Assembly failed!
    popd
    pause
    exit /b
)

echo Linking...
ld65 build/main.o -C nes.cfg -o build/Vaadivasal.nes -m build/map.txt -Ln build/labels.txt --dbgfile build/Vaadivasal.dbg
IF %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed!
    popd
    pause
    exit /b
)

echo ---------------------------------------
echo   SUCCESS! 
echo   ROM location: build/Vaadivasal.nes
echo ---------------------------------------

popd
pause