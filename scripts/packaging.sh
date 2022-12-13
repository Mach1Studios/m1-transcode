#! /bin/bash

# 1st argument = input path
# 2nd argument = platform name
# 3rd argument = transcode binary input path

BIN_NAME=$(basename "$3")

cd "$1"
mkdir -p m1-transcode-$2/license/attribution
cp $1/$3 $1/m1-transcode-$2/$BIN_NAME
cd m1-transcode-$2/license
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/LICENSE.txt
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/Mach1SpatialSDK-RoyaltyFreeLicense.pdf
cd attribution
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/Mach1-EndUserLicenseAgreement-General.pdf
mkdir -p 01_LOGOMARK_SYMBOL
mkdir -p 02B_LOGO_POWEREDBY
mkdir -p 02_LOGO_LOCKUPS
cd 01_LOGOMARK_SYMBOL
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/01_LOGOMARK_SYMBOL/MACH1_LOGO_SYMBOL_EPS_BLK.eps
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/01_LOGOMARK_SYMBOL/MACH1_LOGO_SYMBOL_EPS_WHT.eps
cd ../02B_LOGO_POWEREDBY
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/02B_LOGO_POWEREDBY/MACH1_LOGO_HORZ_PB2_EPS_BLK.eps
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/02B_LOGO_POWEREDBY/MACH1_LOGO_HORZ_PB2_EPS_WHT.eps
cd ../02_LOGO_LOCKUPS
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/02_LOGO_LOCKUPS/MACH1_LOGO_LOCKUP_HORZ_EPS_BLK.eps
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/02_LOGO_LOCKUPS/MACH1_LOGO_LOCKUP_HORZ_EPS_WHT.eps
wget --quiet https://github.com/Mach1Studios/m1-sdk/raw/master/license/attribution/02_LOGO_LOCKUPS/MACH1_LOGO_LOCKUP_VERT_EPS_WHT.eps