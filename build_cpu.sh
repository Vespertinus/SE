mkdir -p build_dir/
CXX=g++-6 CC=gcc-6 cmake -H. -Bbuild_dir -DGPU=OFF -DBOOST_INCLUDEDIR:FILEPATH=/usr/include/boost154 -DBOOST_LIBRARYDIR:FILEPATH=/usr/lib64/boost154 -DGLIBCXX_USE_CXX11_ABI=0
cd build_dir
make -j `nproc`
cd -
