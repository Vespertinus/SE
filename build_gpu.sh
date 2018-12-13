
#usage:
#./build_gpu.sh "scene_viewer VERBOSE=1"
#time CLICOLOR_FORCE=1 ./build_gpu.sh scene_viewer |& head -n 50

mkdir -p build_dir/
CXX=g++-7 CC=gcc-7 cmake -H. -Bbuild_dir
cd build_dir
make -j `nproc` $1
cd -
