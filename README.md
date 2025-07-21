# Mach1 Transcode Executable (m1-transcode)
m1-transcode executable commandline tool

## Compiling

### CMAKE:
 - `cmake . -Bbuild`
 - `cmake . --build build`

### MANUAL:
 - `cd src/`
 - `g++ main.cpp MatrixConvert.cpp ADMParse.cpp M1DSP/M1DSPDynamics.cpp M1DSP/M1DSPUtilities.cpp M1DSP/M1DSPFilters.cpp -static-libstdc++ -L/usr/lib/libsndfile.a -lsndfile -o m1-transcode`

## Development

### CMAKE:
Use `-G` cmake generator command for the appropriate target such as: 
 - Windows: `cmake -Bbuild -G "Visual Studio 15 2017" -A Win32 -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_CPACK=OFF`
 - macOS: `cmake -Bbuild -G Xcode -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_CPACK=OFF`

