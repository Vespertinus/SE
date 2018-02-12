mkdir -p build_dir/
CXX=g++-7 CC=gcc-7 cmake -H. -Bbuild_dir
cd build_dir
make -j `nproc` $1
cd -
