
#usage:
#./build_gpu.sh "scene_viewer VERBOSE=1"
#time CLICOLOR_FORCE=1 ./build_gpu.sh scene_viewer |& head -n 50
#with tests:
#./build_gpu.sh scene_viewer "-DTESTS=ON"

# test
# cmake --build build_dir/ --target all_tests -j `nproc`
# cmake --build build_dir/ --target test -j `nproc`
#
# ctest --test-dir build_dir/tests

mkdir -p build_dir/
CXX=g++-12 CC=gcc-12 cmake -H. -Bbuild_dir $2
cd build_dir
make -j `nproc` $1
cd -
