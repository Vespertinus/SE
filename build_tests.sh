
mkdir -p build_dir/
CXX=g++-12 CC=gcc-12  cmake --build build_dir/ --target all_tests all_functional_tests -j `nproc`
cmake --build build_dir/ --target test -j `nproc`
