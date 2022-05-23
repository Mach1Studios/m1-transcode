@echo off
setlocal EnableDelayedExpansion

REM TODO:
REM - empty!
REM
REM USAGE:
REM - Requires M1-SDK repo
REM - Setup your local vars:
REM   + M1SDK_PATH=
REM   + M1TT_PATH=

python ../../polly/bin/polly.py --clear --install --config Release --toolchain vs-15-2017
python ../../polly/bin/polly.py --clear --install --config Release --toolchain vs-15-2017-win64

cd %M1TT_PATH%
DEL /Q /F /S "m1-transcode-win-x64"
DEL /Q /F /S "m1-transcode-win-x86"
DEL /F "m1-transcode-win-x64.zip"
DEL /F "m1-transcode-win-x86.zip"
MKDIR "m1-transcode-win-x64"
MKDIR "m1-transcode-win-x86"
MKDIR "m1-transcode-win-x64\license"
MKDIR "m1-transcode-win-x86\license"

COPY /Y _install\vs-15-2017\build\m1-transcode.exe %M1SDK_PATH%\executables\windows-x86\m1-transcode.exe
COPY /Y %M1SDK_PATH%\license\LICENSE.txt %M1SDK_PATH%\executables\windows-x86\LICENSE.txt
COPY /Y _install\vs-15-2017-win64\build\m1-transcode.exe %M1SDK_PATH%\executables\windows-x86_64\m1-transcode.exe
COPY /Y %M1SDK_PATH%\license\LICENSE.txt %M1SDK_PATH%\executables\windows-x86_64\LICENSE.txt

COPY /Y _install\vs-15-2017\build\m1-transcode.exe m1-transcode-win-x86\m1-transcode.exe
COPY /Y %M1SDK_PATH%\license\LICENSE.txt %M1TT_PATH%\m1-transcode-win-x86\license\LICENSE.txt
COPY /Y %M1SDK_PATH%\license\Mach1SpatialSDK-RoyaltyFreeLicense.pdf %M1TT_PATH%\m1-transcode-win-x86\license\Mach1SpatialSDK-RoyaltyFreeLicense.pdf
xcopy /S /Y %M1SDK_PATH%\license\attribution %M1TT_PATH%\m1-transcode-win-x86\license\attribution
cd %M1TT_PATH%\m1-transcode-win-x86\license\attribution
DEL /Q /F /S "*.docx"

cd %M1TT_PATH%
COPY /Y _install\vs-15-2017-win64\build\m1-transcode.exe m1-transcode-win-x64\m1-transcode.exe
COPY /Y %M1SDK_PATH%\license\LICENSE.txt %M1TT_PATH%\m1-transcode-win-x64\license\LICENSE.txt
COPY /Y %M1SDK_PATH%\license\Mach1SpatialSDK-RoyaltyFreeLicense.pdf %M1TT_PATH%\m1-transcode-win-x64\license\Mach1SpatialSDK-RoyaltyFreeLicense.pdf
xcopy /S /Y %M1SDK_PATH%\license\attribution %M1TT_PATH%\m1-transcode-win-x64\license\attribution
cd %M1TT_PATH%\m1-transcode-win-x64\license\attribution
DEL /Q /F /S "*.docx"

cd %M1TT_PATH%
"C:\Program Files\7-Zip\7z.exe" a -tzip m1-transcode-win-x64.zip %M1TT_PATH%\m1-transcode-win-x64\
"C:\Program Files\7-Zip\7z.exe" a -tzip m1-transcode-win-x86.zip %M1TT_PATH%\m1-transcode-win-x86\

echo "Deploy Installer?"
set yn=Y
set /P yn="[Y]/N? "
if /I !yn! EQU Y (
            echo "### DEPLOYING TO S3 ###"
            @echo off
            set /p version="Version: "
            echo "### Version: !version! ###"
            echo "link: s3://mach1-releases/!version!/transcode/m1-transcode-win-x64.zip"
            echo "link: s3://mach1-releases/!version!/transcode/m1-transcode-win-x86.zip"
            pause
            aws s3 cp "m1-transcode-win-x64.zip" "s3://mach1-releases/!version!/transcode/m1-transcode-win-x64.zip" --profile mach1 --content-disposition "m1-transcode-win-x64.zip"
            aws s3 cp "m1-transcode-win-x86.zip" "s3://mach1-releases/!version!/transcode/m1-transcode-win-x86.zip" --profile mach1 --content-disposition "m1-transcode-win-x86.zip"
)
pause
