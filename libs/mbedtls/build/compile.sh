
#!/bin/bash

#ABI=armeabi-v7a
#ABI=x86
#ABI=arm64-v8a
ABI=x86_64

ANDROID_NDK=$HOME/Android/Sdk/ndk/29.0.14206865
TOOL_CHAIN=${ANDROID_NDK}/build/cmake/android.toolchain.cmake
CMAKE=$HOME/Android/Sdk/cmake/4.1.2/bin/cmake

MAKE=${ANDROID_NDK}/prebuilt/linux-x86_64/bin/make

mkdir -p ${ABI}
cd ${ABI}

${CMAKE} ../../mbedtls \
	-DCMAKE_SYSTEM_NAME=Android \
    	-DCMAKE_SYSTEM_VERSION=21 \
    	-DANDROID_ABI=${ABI} \
   	-DCMAKE_TOOLCHAIN_FILE=${TOOL_CHAIN} \
    	-DCMAKE_CXX_FLAGS="-DSPDLOG_COMPILED_LIB" \
    	-DCMAKE_MAKE_PROGRAM=${MAKE} \
    	-DENABLE_TESTING=Off \
    	-DENABLE_PROGRAMS=Off \
    	-DUSE_SHARED_MBEDTLS_LIBRARY=On \
    	-DUSE_STATIC_MBEDTLS_LIBRARY=Off

${CMAKE} --build .
    
