#! /bin/bash

mkdir libs && cd libs

git clone --depth 1 git@github.com:Mach1Studios/m1-sdk.git

git clone --depth 1 git@github.com:ebu/libadm.git
cd libadm
mkdir build && cd build
cmake ..
make
sudo make install

cd ../../ && git clone --depth 1 git@github.com:irt-open-source/libbw64.git
cd libbw64
mkdir build && cd build
cmake ..
make
sudo make install