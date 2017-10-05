mkdir -p build_dir/
CXX=g++-6 CC=gcc-6 cmake -H. -Bbuild_dir -DGPU=OFF -DBOOST_INCLUDEDIR:FILEPATH=/usr/include/boost154 -DBOOST_LIBRARYDIR:FILEPATH=/usr/lib64/boost154 -DOLD_ABI=ON
cd build_dir
make -j `nproc`
cd -
