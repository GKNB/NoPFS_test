## Instructions to install this library on Perlmutter
* get the NoPFS library with: git clone git@github.com:KWang1998/NoPFS.git
* Install OpenCV:
  1. git clone https://github.com/opencv/opencv.git
  2. git -C opencv checkout 4.x
  3. mkdir build, cd build
  4. cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=~/opencv/4.x ../
  5. make -j64
  6. make install
  7. export OpenCV_DIR=$HOME/opencv/4.x/lib64/cmake/opencv4/
* Install texinfo:
  1. wget https://ftp.gnu.org/gnu/texinfo/texinfo-7.0.tar.gz
  2. tar -xvzf texinfo-7.0.tar.gz
  3. ./configure --prefix=/global/homes/k/kwf5687/texinfo/7.0
  4. make -j64
  5. make install
  6. export PATH=/global/homes/k/kwf5687/texinfo/7.0/bin:$PATH
* Install libconfig:
  1. git clone https://github.com/hyperrealm/libconfig.git
  2. vi configure.ac, del the line with "AC_CHECK_INCLUDES_DEFAULT"
  3. autoreconf
  4. ./configure --prefix=/global/homes/k/kwf5687/libconfig/install
  5. make -j64
  6. make install
  7. export CMAKE_PREFIX_PATH=/global/homes/k/kwf5687/libconfig/install:$CMAKE_PREFIX_PATH
  8. export CPATH=$CPATH:/global/homes/k/kwf5687/libconfig/install/include
* Modify the codes: add "#include <condition_variable>" in the libhdmlp/include/prefetcher/StagingBufferPrefetcher.h
* load hdf5 module: module load cray-hdf5/1.12.2.3
* In file /NoPFS/libhdmlp/CMakeLists.txt, add cmake_policy(SET CMP0074 NEW)
* Install the library:
  1. module load pytorch
  2. python3 setup.py install --prefix=~/HDMLP
  3. export PYTHONPATH=$PYTHONPATH:/global/homes/k/kwf5687/HDMLP/lib/python3.9/site-packages/hdmlp-0.1-py3.9.egg
  4. export LD_PRELOAD=/usr/lib64/libffi.so.7
* Run the benchmark
