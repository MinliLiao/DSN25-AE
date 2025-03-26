cd ..
CC=gcc-12 CXX=g++-12 scons -j4 build/ARM/gem5.opt
cd util/m5
scons build/arm64/out/m5
