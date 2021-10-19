#!/bin/bash
# 
# Copyright (C) 2020 RB INTERNATIONAL NETWORK
#
#            An Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# colours
BOLD='\033[1m'
GRN='\033[01;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[01;31m'
RST='\033[0m'
echo "Cloning dependencies if they don't exist...."
# colours

rm -rf CPH1859
rm -rf RMX1831
mkdir CPH1859
mkdir RMX1831
cd CPH1859 && mkdir final
cd ../RMX1831 && mkdir final
cd ..

if [ ! -d clang ]
then
git clone --depth=1 https://github.com/kardebayan/android_prebuilts_clang_host_linux-x86_clang-5696680 clang
fi

if [ ! -d gcc32 ]
then
git clone --depth=1 https://github.com/KudProject/arm-linux-androideabi-4.9 gcc32
fi

if [ ! -d gcc ]
then
git clone --depth=1 https://github.com/KudProject/aarch64-linux-android-4.9 gcc
fi

git clone https://gitlab.com/CyberJalagam/AnyKernel3 -b rm1 --depth=1 CPH1859/AnyKernel
git clone https://gitlab.com/CyberJalagam/AnyKernel3 -b rmu1 --depth=1 RMX1831/AnyKernel

echo "Env done"

KERNEL_DIR=$(pwd)
IMAGE="${KERNEL_DIR}/out/arch/arm64/boot/Image.gz-dtb"
TANGGAL=$(date +"%Y%m%d-%H")
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
PATH="${KERNEL_DIR}/clang/bin:${KERNEL_DIR}/gcc/bin:${KERNEL_DIR}/gcc32/bin:${PATH}"
export KBUILD_COMPILER_STRING="$(${KERNEL_DIR}/clang/bin/clang --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g')"
export ARCH=arm64
export KBUILD_BUILD_USER=jaishnav
export KBUILD_BUILD_HOST=rbinternational

#
# BEGIN REALME 1
#

# Start configs for Realme 1

function compile() {

    echo -e "${CYAN}"
    make -j$(nproc) O=out ARCH=arm64 oppo6771_17065_defconfig
    make -j$(nproc) O=out \
                    ARCH=arm64 \
                    CC=clang \
                    CLANG_TRIPLE=aarch64-linux-gnu- \
                    CROSS_COMPILE=aarch64-linux-android- \
                    CROSS_COMPILE_ARM32=arm-linux-androideabi-
   echo -e "${RST}"
SUCCESS=$?
	if [ $SUCCESS -eq 0 ]
        	then
		echo -e "${GRN}"
		echo "------------------------------------------------------------"
		echo "Compilation successful, Aliens invaded"
                echo "Image.gz-dt can be found at out/arch/arm64/boot/Image.gz-dtb"
          	cp out/arch/arm64/boot/Image.gz-dtb CPH1859/AnyKernel
		echo  "------------------------------------------------------------"
		echo -e "${RST}"
	else
		echo -e "${RED}"
                echo "------------------------------------------------------------"
		echo "Compilation failed, Earth is safe!"
                echo "------------------------------------------------------------"
		echo -e "${RST}"
	fi

}
# Zipping
function zipping-fast() {
    echo -e "${YELLOW}"
    echo "Alien invasion in progress... "
    cd CPH1859/AnyKernel || exit 1
    zip -r9 AlienKernel™️-v3.0-CPH1859-fastcharge-${TANGGAL}.zip * > /dev/null 2>&1
    cp -v AlienKernel™️-v3.0-CPH1859-fastcharge-${TANGGAL}.zip ../final
    cd ../..
    echo "Aliens stored at CPH1859/final/AlienKernel™️-v3.0-CPH1859-fastcharge-${TANGGAL}.zip"
    echo -e "${RST}"
}

# Zipping
function zipping-normal() {
    echo -e "${YELLOW}"
    echo "Alien invasion in progress... "
    cd CPH1859/AnyKernel || exit 1
    zip -r9 AlienKernel™️-v3.0-CPH1859-${TANGGAL}.zip * > /dev/null 2>&1
    cp -v AlienKernel™️-v3.0-CPH1859-${TANGGAL}.zip ../final
    cd ../..
    echo "Aliens stored at CPH1859/final/AlienKernel™️-v3.0-CPH1859-${TANGGAL}.zip"
    echo -e "${RST}"
}
# End configs for Realme 1

# Compile for CPH1859 (fast charge)
compile
zipping-fast

# Clean
rm -rf out
rm -rf CPH1859/AnyKernel
git clone https://gitlab.com/CyberJalagam/AnyKernel3 -b rm1 --depth=1 CPH1859/AnyKernel


# Nuke fast charging
wget https://github.com/CyberJalagam/kernel_oppo_mt6771/commit/0610a4fe15ffaac96e546fb0b7401536bbf5178c.patch
git am 0610a4fe15ffaac96e546fb0b7401536bbf5178c.patch
rm 0610a4fe15ffaac96e546fb0b7401536bbf5178c.patch

# Compile for CPH1859 (normal)
compile
zipping-normal

# Clean
rm -rf out

#
# END REALME 1
#

#
# BEGIN REALME U1
#
export OPPO_18611=1

# Compile plox
function compile-u1() {

    echo -e "${CYAN}"
    make -j$(nproc) O=out ARCH=arm64 oppo6771_18611_defconfig
    make -j$(nproc) O=out \
                    ARCH=arm64 \
                    CC=clang \
                    CLANG_TRIPLE=aarch64-linux-gnu- \
                    CROSS_COMPILE=aarch64-linux-android- \
                    CROSS_COMPILE_ARM32=arm-linux-androideabi-
   echo -e "${RST}"
SUCCESS=$?
	if [ $SUCCESS -eq 0 ]
        	then
		echo -e "${GRN}"
		echo "------------------------------------------------------------"
		echo "Compilation successful, Aliens invaded"
        	echo "Image.gz-dt can be found at out/arch/arm64/boot/Image.gz-dtb"
    		cp out/arch/arm64/boot/Image.gz-dtb RMX1831/AnyKernel
		echo  "------------------------------------------------------------"
		echo -e "${RST}"
	else
		echo -e "${RED}"
                echo "------------------------------------------------------------"
		echo "Compilation failed, Earth is safe!"
                echo "------------------------------------------------------------"
		echo -e "${RST}"
	fi

}
# Zipping
function zipping-u1-fast() {
    echo -e "${YELLOW}"
    echo "Alien invasion in progress... "
    cd RMX1831/AnyKernel || exit 1
    zip -r9 AlienKernel™️-v1.0-RMX1831-fastcharge-${TANGGAL}.zip * > /dev/null 2>&1
    cp -v AlienKernel™️-v1.0-RMX1831-fastcharge-${TANGGAL}.zip ../final
    cd ../..
    echo "Aliens stored at RMX1831/final/AlienKernel™️-v1.0-RMX1831-fastcharge-${TANGGAL}.zip"
    echo -e "${RST}"
}

}
# Zipping
function zipping-u1() {
    echo -e "${YELLOW}"
    echo "Alien invasion in progress... "
    cd RMX1831/AnyKernel || exit 1
    zip -r9 AlienKernel™️-v1.0-RMX1831-${TANGGAL}.zip * > /dev/null 2>&1
    cp -v AlienKernel™️-v1.0-RMX1831-${TANGGAL}.zip ../final
    cd ../..
    echo "Aliens stored at RMX1831/final/AlienKernel™️-v1.0-RMX1831-${TANGGAL}.zip"
    echo -e "${RST}"
}

# Compile for realme u1 (normal)
compile-u1
zipping-u1

# Clean
rm -rf out
rm -rf RMX1831/AnyKernel
git clone https://gitlab.com/CyberJalagam/AnyKernel3 -b rmu1 --depth=1 RMX1831/AnyKernel

# Add fast charging
git reset --hard HEAD~1

# Compile for realme u1 (fast charging)
compile-u1
zipping-u1-fast

#
# END REALME U1
#
