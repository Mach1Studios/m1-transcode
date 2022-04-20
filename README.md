# Mach1 Transcode Executable (m1-transcde)
m1-transcode executable commandline tool

## Setup
Install and compile other libs
 - `./scripts/setup.sh`

## Compiling

### CMAKE:
 - `cmake .`
 - `cmake --build .`

### POLLY: 
 - `polly --clear --install --config Release --toolchain linux-gcc`
 - `polly --clear --install --config Release --toolchain linux-gcc-x64`

### MANUAL:
 - `cd src/`
 - `g++ main.cpp MatrixConvert.cpp ADMParse.cpp M1DSP/M1DSPDynamics.cpp M1DSP/M1DSPUtilities.cpp M1DSP/M1DSPFilters.cpp -static-libstdc++ -L/usr/lib/libsndfile.a -lsndfile -o m1-transcode`
