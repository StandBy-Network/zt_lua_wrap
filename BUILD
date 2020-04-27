export CC=clang
export CXX=clang++
export LIBRARY_PATH="./ext/libzt/lib/debug/linux-x86_64:./ext/lua-5.3.5/src":$LIBRARY_PATH
export LD_LIBRARY_PATH=$LIBRARY_PATH:$LD_LIBRARY_PATH
DIR="./build"
if [ -d "$DIR" ]; then
    cd build
else
    mkdir build && cd build
fi

cmake ../
make