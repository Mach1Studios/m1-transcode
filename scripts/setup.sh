#! /bin/bash

mkdir -p libs
cd libs

git clone --depth 1 git@github.com:Mach1Studios/m1-sdk.git
git clone --depth 1 git@github.com:libsndfile/libsndfile.git
git clone --depth 1 git@github.com:ebu/libadm.git
git clone --depth 1 git@github.com:irt-open-source/libbw64.git

# cd libadm
# mkdir build && cd build
# cmake ..
# make
# make install

# cd ../../

# cd libbw64
# mkdir build && cd build
# cmake ..
# make
# make install