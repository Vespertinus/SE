mkdir -p build_dir/
CXX=g++-7 CC=gcc-7 cmake -H. -Bbuild_dir -DGPU=OFF -DBOOST_INCLUDEDIR:FILEPATH=/usr/include/boost160 -DBOOST_LIBRARYDIR:FILEPATH=/usr/lib64/boost160 -DOLD_ABI=OFF -DCMAKE_INSTALL_PREFIX=~/usr/ $2
cd build_dir
make -j `nproc` $1
cd -
