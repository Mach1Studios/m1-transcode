#! /bin/bash

# MACH1 m1-transcode testing
# 
# TODO: 
# im empty!
# 
# USAGE:
# - Change the absolute paths and run
#

M1SDK_PATH=./libs/m1-sdk
DEBUGAUDIO_PATH=~/Desktop/Mach1-TestFiles

# build for macos
cmake . -B_builds/xcode -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 -DCMAKE_INSTALL_PREFIX=`pwd`/_install/xcode
cmake --build _builds/xcode --config Release --target install

cd $M1TT_PATH
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt 7.1.2_M -out-file $DEBUGAUDIO_PATH/m1-debug-output-adm.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt M1Horizon -out-file $DEBUGAUDIO_PATH/m1-debug-output-M1Horizon.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt ACNSN3DmaxRE1oa -out-file $DEBUGAUDIO_PATH/m1-debug-output-ACNSN3DmaxRE1oa.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt ACNSN3DO3A -out-file $DEBUGAUDIO_PATH/m1-debug-output-ACNSN3DO3A.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt TBE -out-file $DEBUGAUDIO_PATH/m1-debug-output-TBE.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt ACNSN3D -out-file $DEBUGAUDIO_PATH/m1-debug-output-ACNSN3D.wav -out-file-chans 0
_install/xcode/build/m1-transcode -in-fmt M1Spatial -in-file $DEBUGAUDIO_PATH/m1-debug-shrtpt-m1spatial.wav -out-fmt 5.1_C -out-file $DEBUGAUDIO_PATH/m1-debug-output-FiveOneFilm_Cinema.wav -out-file-chans 0
