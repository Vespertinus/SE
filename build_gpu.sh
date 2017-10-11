mkdir -p build_dir/
CXX=g++-6 CC=gcc-6 cmake -H. -Bbuild_dir
cd build_dir
make -j `nproc`
cd -
