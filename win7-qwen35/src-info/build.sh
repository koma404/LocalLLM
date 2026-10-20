#!/bin/sh
# Ubuntu 24.04 + apt: g++-mingw-w64-x86-64-posix (GCC 13)
# llama.cpp commit dd002b91f583af0b80140f9a140dc04856bfc2e0 (2026-10-20)
git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp
git checkout dd002b91f583af0b80140f9a140dc04856bfc2e0
git apply ../win7-httplib.patch
cmake -S . -B build-win7 -DCMAKE_TOOLCHAIN_FILE=../mingw.cmake -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0601" -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0601" \
 -DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=OFF -DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON -DGGML_BMI2=ON \
 -DGGML_OPENMP=OFF -DLLAMA_OPENSSL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=ON \
 -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build-win7 -j4 --target llama-cli llama-bench llama-server
# 9B cluster build: add -DGGML_RPC=ON and also build ggml-rpc-server
