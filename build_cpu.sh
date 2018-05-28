mkdir -p build_dir/
CXX=g++-7 CC=gcc-7 cmake -H. -Bbuild_dir -DGPU=OFF -DBOOST_INCLUDEDIR:FILEPATH=/usr/include/boost154 -DBOOST_LIBRARYDIR:FILEPATH=/usr/lib64/boost154 -DOLD_ABI=ON -DCMAKE_INSTALL_PREFIX=~/usr/ $2
cd build_dir
make -j `nproc` $1
cd -
