#! /bin/bash

mkdir libs && cd libs

git clone git@github.com:Mach1Studios/m1-sdk.git

cd ../ && git clone git@github.com:ebu/libadm.git
cd libadm
mkdir build && cd build
cmake ..
make
sudo make install

cd ../../ && git clone git@github.com:irt-open-source/libbw64.git
cd libbw64
mkdir build && cd build
cmake ..
make
sudo make install