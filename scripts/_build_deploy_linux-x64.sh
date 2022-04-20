#! /bin/bash

# MACH1 m1-transcode PACKAGE & DEPLOY
# 
# TODO: 
# im empty!
# 
# USAGE:
# - Setup your local vars: 
#   + M1SDK_PATH=
#   + M1TT_PATH=
#

M1TT_PATH=~/git/m1-transcodertools/m1-transcode
M1SDK_PATH=~/git/m1-sdk

# build for linux
~/git/polly/bin/polly --clear --install --config Release --toolchain gcc

cd $M1TT_PATH
rm -rf "m1-transcode-linux-x64"
rm "m1-transcode-linux-x64.zip"
mkdir -p "m1-transcode-linux-x64"
cd "m1-transcode-linux-x64"
mkdir -p license

cp -f ../_install/gcc/bin/m1-transcode $M1SDK_PATH/executables/linux/amazon-lambda-ami/m1-transcode
cp -f $M1SDK_PATH/license/LICENSE.txt $M1SDK_PATH/executables/linux/amazon-lambda-ami/LICENSE.txt

cp -f ../_install/gcc/bin/m1-transcode ./m1-transcode
cp -f $M1SDK_PATH/license/LICENSE.txt license/LICENSE.txt
cp -f $M1SDK_PATH/license/Mach1SpatialSDK-RoyaltyFreeLicense.pdf license/Mach1SpatialSDK-RoyaltyFreeLicense.pdf
cp -Rf $M1SDK_PATH/license/attribution license/attribution
cd license/attribution
rm -f *.docx

cd ../../../
find m1-transcode-linux-x64 -path '*/.*' -prune -o -type f -print | zip m1-transcode-linux-x64.zip -@

echo "### DEPLOYING TO S3 ###"
# version="USER INPUT"
read -p "Version: " version
aws s3 cp "m1-transcode-linux-x64.zip" "s3://mach1-releases/$version/transcode/m1-transcode-linux-x64.zip" --profile mach1 --content-disposition "m1-transcode-linux-x64.zip"
