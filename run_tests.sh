
set -e
cd build_dir/tests/
make -j `nproc` $1
ctest --output-on-failure
cd -
