# Mach1 Transcode Executable (m1-transcde)
m1-transcode executable commandline tool

## Setup
Install and compile other libs
 - `./scripts/setup.sh`

## Compiling

### CMAKE:
 - `cmake .`
 - `cmake --build .`

### MANUAL:
 - `cd src/`
 - `g++ main.cpp MatrixConvert.cpp ADMParse.cpp M1DSP/M1DSPDynamics.cpp M1DSP/M1DSPUtilities.cpp M1DSP/M1DSPFilters.cpp -static-libstdc++ -L/usr/lib/libsndfile.a -lsndfile -o m1-transcode`

## Development

### CMAKE:
The easiest way to setup a dev env for *m1-transcode* would be with the following command:
 - `./scripts/setup.sh # run in git-bash on windows`
 - `mkdir m1-transcode-dev && cd m1-transcode-dev`
Use `-G` cmake generator command for the appropriate target such as: 
 - Windows: `cmake -G "Visual Studio 15 2017" -A Win32 ..  -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_CPACK=OFF`
 - macOS: `cmake -G Xcode ..  -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_CPACK=OFF`
