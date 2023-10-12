module load pytorch
export OpenCV_DIR=/global/homes/t/tianle/software/opencv/install/lib64/cmake/opencv4
export PATH=/global/homes/t/tianle/software/texinfo/install/bin:$PATH
export CMAKE_PREFIX_PATH=/global/homes/t/tianle/software/libconfig/install:$CMAKE_PREFIX_PATH
export CPATH=$CPATH:/global/homes/t/tianle/software/libconfig/install/include
module load cray-hdf5/1.12.2.3

#python3 setup.py install --prefix=/global/homes/t/tianle/myWork/wkw_cache/HDMLP
export PYTHONPATH=$PYTHONPATH:/global/homes/t/tianle/myWork/wkw_cache/HDMLP/lib/python3.9/site-packages/hdmlp-0.1-py3.9.egg
export LD_PRELOAD=/usr/lib64/libffi.so.7
