#!/bin/bash
set -ev

if [ -z ${LOCAL_PKG+x} ] || [ -z "$LOCAL_PKG" ]; then
    echo "LOCAL_PKG is not set. Aborting..."
    exit 1
fi

echo "=== Updating the build environment in $LOCAL_PKG ==="

#echo "=== Installing from external package sources ==="
#wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
#echo "deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-3.9 main" | sudo tee -a /etc/apt/sources.list
#sudo add-apt-repository -y ppa:lttng/ppa
#sudo apt-get update -qq
#sudo apt-get install -y clang-3.9 clang-tidy-3.9

echo "=== Installing python packages ==="
pip install --user cpp-coveralls

# Increase the environment version to force a rebuild of the packages
# The version is writen to the cache file after every build of the dependencies
ENV_VERSION="1"
ENV_INSTALLED=""
if [ -e $LOCAL_PKG/.build_env ]; then
    echo "=== No cached build environment ==="
    read -r ENV_INSTALLED < $LOCAL_PKG/.build_env
fi

# travis caches the $LOCAL_PKG dir. If it is loaded, we don't need to reinstall the packages
if [ "$ENV_VERSION" = "$ENV_INSTALLED" ]; then
    echo "=== The build environment is current ==="
    # Print version numbers
    clang --version
    g++ --version
    cppcheck --version
    valgrind --version
    exit 0
fi

echo "=== The build environment is outdated ==="

# Clean up
# additional safety measure to avoid rm -rf on root
# only execute it on travis
if ! [ -z ${TRAVIS+x} ]; then
    rm -rf $LOCAL_PKG/*
fi

# Install newer valgrind
echo "=== Installing valgrind ==="
wget ftp://sourceware.org/pub/valgrind/valgrind-3.13.0.tar.bz2
tar xf valgrind-3.13.0.tar.bz2
cd valgrind-3.13.0
CC=clang CXX=clang++ ./configure --prefix=$LOCAL_PKG
make -s -j8 install
cd ..

# Install specific check version which is not yet in the apt package
echo "=== Installing check ==="
mkdir tmp_check
wget http://ftp.de.debian.org/debian/pool/main/c/check/check_0.10.0-3+b3_amd64.deb
dpkg -x check_0.10.0-3+b3_amd64.deb ./tmp_check
# change pkg-config file path
sed -i "s|prefix=/usr|prefix=${LOCAL_PKG}|g" ./tmp_check/usr/lib/x86_64-linux-gnu/pkgconfig/check.pc
sed -i 's|libdir=.*|libdir=${prefix}/lib|g' ./tmp_check/usr/lib/x86_64-linux-gnu/pkgconfig/check.pc
# move files to globally included dirs
cp -R ./tmp_check/usr/lib/x86_64-linux-gnu/* $LOCAL_PKG/lib/
cp -R ./tmp_check/usr/include/* $LOCAL_PKG/include/
cp -R ./tmp_check/usr/bin/* $LOCAL_PKG/


# Install newer cppcheck
echo "=== Installing cppcheck ==="
wget https://github.com/danmar/cppcheck/archive/1.73.tar.gz -O cppcheck-1.73.tar.gz
tar xf cppcheck-1.73.tar.gz
cd cppcheck-1.73
CC=clang CXX=clang++ make PREFIX="$LOCAL_PKG" SRCDIR=build CFGDIR="$LOCAL_PKG/cppcheck-cfg" HAVE_RULES=yes CXXFLAGS="-O2 -DNDEBUG -Wno-sign-compare -Wno-unused-function -std=gnu++0x" -j
CC=clang CXX=clang++ make PREFIX="$LOCAL_PKG" SRCDIR=build CFGDIR="$LOCAL_PKG/cppcheck-cfg" HAVE_RULES=yes install
cd ..

# create cached flag
echo "=== Store cache flag ==="
echo $ENV_VERSION > $LOCAL_PKG/.build_env

# Print version numbers
echo "=== Installed versions are ==="
clang --version
g++ --version
cppcheck --version
valgrind --version
