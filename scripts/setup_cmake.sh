# cmake > 3.15
cmake_version=3.15.7

mkdir ~/temp
cd ~/temp
wget wget https://cmake.org/files/v3.15/cmake-3.15.7.tar.gz
tar -xzvf cmake-3.15.7.tar.gz
cd cmake-3.15.7/
./bootstrap
sudo make
sudo make install