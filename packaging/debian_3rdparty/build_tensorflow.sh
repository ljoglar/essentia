#!/bin/sh
. ../build_config.sh


rm -rf tmp
mkdir tmp
cd tmp


echo "Building Tensorflow $TENSORFLOW_VERSION"


curl -SLO https://github.com/tensorflow/tensorflow/archive/v$TENSORFLOW_VERSION.tar.gz
tar -xf v$TENSORFLOW_VERSION.tar.gz
cd tensorflow-$TENSORFLOW_VERSION


# Force using curl for the dependencies download
sed -i 's/\[\[ \"\$OSTYPE\" == \"darwin\"\* \]\]/true/g' tensorflow/contrib/makefile/download_dependencies.sh

tensorflow/contrib/makefile/download_dependencies.sh

# Add fPIC, otherwise nsync won't compile
sed -i 's/PLATFORM_CFLAGS=-std=c++11 -Werror -Wall -Wextra -pedantic/PLATFORM_CFLAGS=-std=c++11 -Werror -Wall -Wextra -pedantic -fPIC/g' tensorflow/contrib/makefile/compile_nsync.sh

# Compile the C API as it's curretly the recomended way to interface Tensorflow.  
sed -i 's/CORE_CC_ALL_SRCS := \\/&\n\$(wildcard tensorflow\/c\/c_api.cc) \\/' tensorflow/contrib/makefile/Makefile

# Define andrid to prevent errors in the Andrid API. Why?
sed -i 's/#ifndef __ANDROID__/#define __ANDROID__ 1\n&/' tensorflow/c/c_api.cc

# We don't want to re download dependencies on the build script as it will undo the previous patches
sed -i 's/rm -rf tensorflow\/contrib\/makefile\/downloads/# &/' tensorflow/contrib/makefile/build_all_linux.sh
sed -i 's/tensorflow\/contrib\/makefile\/download_dependencies.sh/# &/' tensorflow/contrib/makefile/build_all_linux.sh

# Add -lrt flag.
sed -i 's/HOST_CXXFLAGS=\"--std=c++11 -march=native\" \\/HOST_CXXFLAGS=\"--std=c++11 -march=native -lrt\" \\/' tensorflow/contrib/makefile/build_all_linux.sh

# Prevent compiling the example.
sed -i 's/all: \$(LIB_PATH) \$(BENCHMARK_NAME)/all: \$(LIB_PATH)/' tensorflow/contrib/makefile/Makefile

tensorflow/contrib/makefile/build_all_linux.sh

PREFIX_LIB=${PREFIX}/lib/tensorflow
PREFIX_INCLUDE=${PREFIX}/include

mkdir ${PREFIX_LIB}

cp tensorflow/contrib/makefile/gen/lib/libtensorflow-core.a ${PREFIX_LIB}
cp tensorflow/contrib/makefile/gen/protobuf/lib/libprotobuf.a ${PREFIX_LIB}
cp tensorflow/contrib/makefile/downloads/nsync/builds/default.linux.c++11/libnsync.a ${PREFIX_LIB}

cp tensorflow/c/c_api.h ${PREFIX_INCLUDE}

./tensorflow/c/generate-pc.sh -p ${PREFIX} -l ${PREFIX_LIB} -v $TENSORFLOW_VERSION



cd ../..
rm -r tmp
