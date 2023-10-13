#! /bin/bash

mkdir -p libs
cd libs

#git -C m1-sdk pull || git clone --depth 1 git@github.com:Mach1Studios/m1-sdk.git m1-sdk
git -C libsndfile pull || git clone --depth 1 git@github.com:libsndfile/libsndfile.git libsndfile
git -C libadm pull || git clone --depth 1 git@github.com:ebu/libadm.git libadm
git -C libbw64 pull || git clone --depth 1 git@github.com:irt-open-source/libbw64.git libbw64