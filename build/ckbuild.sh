#!/bin/bash
#
# Flopster's build script.
# Based on build script for Quicksilver, by Ghostrider.
# Copyright (C) 2020-2021 Adithya R. (original version)
# Copyright (C) 2022-2024 Flopster101 (rewrite)

## Vars
# Toolchains
AOSP_REPO="https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master"
AOSP_ARCHIVE="https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/master"
PC_REPO="https://github.com/kdrag0n/proton-clang"
LZ_REPO="https://gitlab.com/Jprimero15/lolz_clang.git"
SL_REPO="http://ftp.twaren.net/Unix/Kernel/tools/llvm/files/"

# Other
DEFAULT_DEFCONFIG="fpkernel_exynos2100-r9sxxx_defconfig"
KERNEL_URL="https://github.com/ProtonKernel/Proton"
AK3_URL="https://github.com/Flopster101/AnyKernel3-A25"
AK3_TEST=0
SECONDS=0 # builtin bash timer
DATE="$(date '+%Y%m%d-%H%M')"

# Workspace
if [ -d /workspace ]; then
    WP="/workspace"
    IS_GP=1
else
    IS_GP=0
fi
if [ -z "$WP" ]; then
    echo -e "\nERROR: Environment not Gitpod! Please set the WP env var...\n"
    exit 1
fi

if [ ! -d drivers ]; then
    echo -e "\nERROR: Please exec from top-level kernel tree\n"
    exit 1
fi

if [ "$IS_GP" = "1" ]; then
    export KBUILD_BUILD_USER="Flopster101"
    export KBUILD_BUILD_HOST="buildbot"
fi

export PATH="$(pwd)/build/bin:$PATH"

# Directories
AC_DIR="$WP/aospclang"
PC_DIR="$WP/protonclang"
LZ_DIR="$WP/lolzclang"
SL_DIR="$WP/slimllvm"
AK3_DIR="$WP/AK3-r9s"
AK3_BRANCH="r9s"
KDIR="$(readlink -f .)"

## Inherited paths
OUTDIR="$KDIR/out"
MOD_OUTDIR="$KDIR/modules_out"
TMPDIR="$KDIR/build/tmp"
IN_VBOOT="$KDIR/build/vboot"
IN_DTB="$OUTDIR/arch/arm64/boot/dts/exynos/exynos2100.dtb"
RAMDISK_DIR="$TMPDIR/vboot_ramdisk"
PREBUILT_RAMDISK="$KDIR/build/boot/ramdisk"
MODULES_DIR="$RAMDISK_DIR/lib/modules"
OUT_KERNEL="$OUTDIR/arch/arm64/boot/Image"
OUT_BOOTIMG="$KDIR/build/boot.img"
OUT_VENDORBOOTIMG="$KDIR/build/vendor_boot.img"
OUT_DTBIMAGE="$TMPDIR/dtb.img"
# Tools
MKBOOTIMG="$(pwd)/build/mkbootimg/mkbootimg.py"
MKDTBOIMG="$(pwd)/build/dtb/mkdtboimg.py"

# Dependencies
UB_DEPLIST="lz4 brotli flex bc cpio kmod ccache zip binutils-aarch64-linux-gnu"
if grep -q "Ubuntu" /etc/os-release; then
    sudo apt install $UB_DEPLIST -y
else
    echo -e "\nINFO: Your distro is not Ubuntu, skipping dependencies installation..."
    echo -e "INFO: Make sure you have these dependencies installed before proceeding: $UB_DEPLIST"
fi

## Customizable vars
# Kernel verison
K_VER="v2"

# Toggles
USE_CCACHE=1
DO_TAR="1"
DO_ZIP="1"

# Upload build log
BUILD_LOG=1

# Pick aosp, proton, lolz or slim
CLANG_TYPE=aosp

## Info message
LINKER=ld.lld
DEVICE="Galaxy S21 FE"
CODENAME="r9s"

if [ $DO_TG -eq 1 ]; then
IDS="../ids/"
## Secrets
if ! [ -d "$IDS" ]; then
    git clone https://github.com/ProtonKernel/ids $IDS
fi
TELEGRAM_CHAT_ID="$(cat ../ids/chat_ci)"
TELEGRAM_BOT_TOKEN=$(cat ../ids/bot_token)
fi

## Parse arguments
DO_KSU=0
DO_CLEAN=0
DO_MENUCONFIG=0
IS_RELEASE=1
DO_TG=0
DEFCONFIG=$DEFAULT_DEFCONFIG
for arg in "$@"
do
    if [[ "$arg" == *m* ]]; then
        echo -e "\nINFO: menuconfig argument passed, kernel configuration menu will be shown..."
        DO_MENUCONFIG=1
    fi
    if [[ "$arg" == *k* ]]; then
        echo -e "\nINFO: KernelSU argument passed, a KernelSU build will be made..."
        DO_KSU=1
    fi
    if [[ "$arg" == *c* ]]; then
        echo -e "\nINFO: clean argument passed, output directory will be wiped..."
        DO_CLEAN=1
    fi
    if [[ "$arg" == *R* ]]; then
        echo -e "\nINFO: Release argument passed, build marked as release"
        IS_RELEASE=1
    fi
    if [[ "$arg" == *t* ]]; then
        echo -e "\nINFO: Telegram argument passed, build will be uploaded to CI"
        DO_TG=1
    fi
    if [[ "$arg" == *o* ]]; then
        echo -e "\nINFO: oshi.at argument passed, build will be uploaded to oshi.at"
        DO_OSHI=1
    fi
done

if [[ "${IS_RELEASE}" = "1" ]]; then
    BUILD_TYPE="Release"
else
    echo -e "\nINFO: Build marked as testing"
    BUILD_TYPE="Testing"
fi

## Build type
LINUX_VER=$(make kernelversion 2>/dev/null)

FK_TYPE=""
if [ $DO_KSU -eq 1 ]; then
    FK_TYPE="KSU"
    DEFCONFIG="fpkernelksu_exynos2100-r9sxxx_defconfig"
else
    FK_TYPE="Vanilla"
fi
ZIP_PATH="$KDIR/build/Proton+_$K_VER-$FK_TYPE-$CODENAME-$DATE.zip"
TAR_PATH="$KDIR/build/Proton+_$K_VER-$FK_TYPE-$CODENAME-$DATE.tar"

echo -e "\nINFO: Build info:
- Device: $DEVICE ($CODENAME)
- Addons = $FK_TYPE
- Proton version: $K_VER
- Linux version: $LINUX_VER
- Defconfig: $DEFCONFIG
- Build date: $DATE
- Build type: $BUILD_TYPE
- Clean build = $DO_CLEAN
"

get_toolchain() {
    # AOSP Clang
    if [[ $1 = "aosp" ]]; then
        if ! [ -d "$AC_DIR" ]; then
        CURRENT_CLANG=$(curl $AOSP_REPO | grep -oE "clang-r[0-9a-f]+" | sort -u | tail -n1)
            echo -e "\nINFO: AOSP Clang not found! Cloning to $AC_DIR..."
            if ! curl -LSsO "$AOSP_ARCHIVE/$CURRENT_CLANG.tar.gz"; then
                echo -e "\nERROR: Cloning failed! Aborting..."
                exit 1
            fi
            mkdir -p $AC_DIR && tar -xf ./*.tar.gz -C $AC_DIR && rm ./*.tar.gz && rm -rf clang
            touch $AC_DIR/bin/aarch64-linux-gnu-elfedit && chmod +x $AC_DIR/bin/aarch64-linux-gnu-elfedit
            touch $AC_DIR/bin/arm-linux-gnueabi-elfedit && chmod +x $AC_DIR/bin/arm-linux-gnueabi-elfedit
            rm -rf $CURRENT_CLANG
        fi
    fi

    # Proton Clang
    if [[ $1 = "proton" ]]; then
        if ! [ -d "$PC_DIR" ]; then
            echo -e "\nINFO: Proton Clang not found! Cloning to $PC_DIR..."
            if ! git clone -q --depth=1 $PC_REPO $PC_DIR; then
                echo -e "\nERROR: Cloning failed! Aborting..."
                exit 1
            fi
        fi
    fi

    # Lolz Clang
    if [[ $1 = "lolz" ]]; then
        if ! [ -d "$LZ_DIR" ]; then
            echo -e "\nINFO: Lolz Clang not found! Cloning to $LZ_DIR..."
            if ! git clone -q --depth=1 $LZ_REPO $LZ_DIR; then
                echo -e "\nERROR: Cloning failed! Aborting..."
                exit 1
            fi
        fi
    fi

    # Slim LLVM
    if [[ $1 = "slim" ]]; then
        if ! [ -d "$SL_DIR" ]; then
            echo -e "\nINFO: Slim LLVM not found! Cloning to $SL_DIR..."

            # Ensure URL has a trailing slash
            SL_REPO="http://ftp.twaren.net/Unix/Kernel/tools/llvm/files/"

            # Fetch the directory listing and extract the tar.xz filenames
            FILENAMES=$(curl -s "$SL_REPO" | grep -oP 'llvm-[\d.]+-x86_64\.tar\.xz')

            # Find the latest filename by sorting and picking the last one
            LATEST_FILE=$(echo "$FILENAMES" | sort -V | tail -n 1)

            # Download the latest file
            wget -q --show-progress -O "$WP/${LATEST_FILE}" "${SL_REPO}${LATEST_FILE}"
            if [ $? -eq 0 ]; then
                echo "Downloaded: ${LATEST_FILE}"
                mkdir -p "$SL_DIR"

                # Determine the name of the extracted folder
                EXTRACTED_FOLDER=$(basename "$LATEST_FILE" .tar.xz)

                # Extract directly and move contents out of the subdirectory
                tar -xf "$WP/${LATEST_FILE}" -C "$SL_DIR"
                mv "$SL_DIR/$EXTRACTED_FOLDER"/* "$SL_DIR"

                # Remove the now-empty extracted directory
                rmdir "$SL_DIR/$EXTRACTED_FOLDER"

                # Delete the downloaded file
                rm "$WP/${LATEST_FILE}"

                echo "Extraction complete."
            else
                echo -e "\nERROR: Cloning failed! Aborting..."
                exit 1
            fi
        fi
    fi
}

prep_toolchain() {
    if [[ $1 = "aosp" ]]; then
        CLANG_DIR="$AC_DIR"
        CCARM64_PREFIX=aarch64-linux-gnu-
        echo -e "\nINFO: Using AOSP Clang..."
    elif [[ $1 = "proton" ]]; then
        CLANG_DIR="$PC_DIR"
        CCARM64_PREFIX=aarch64-linux-gnu-
        echo -e "\nINFO: Using Proton Clang..."
    elif [[ $1 = "lolz" ]]; then
        CLANG_DIR="$LZ_DIR"
        CCARM64_PREFIX=aarch64-linux-gnu-
        echo -e "\nINFO: Using Lolz Clang..."
    elif [[ $1 = "slim" ]]; then
        CLANG_DIR="$SL_DIR"
        CCARM64_PREFIX=aarch64-linux-gnu-
        echo -e "\nINFO: Using Slim LLVM Clang..."
    fi

    ## Set PATH
    export PATH="${CLANG_DIR}/bin:${PATH}"

    KBUILD_COMPILER_STRING=$("$CLANG_DIR"/bin/clang -v 2>&1 | head -n 1 | sed 's/(https..*//' | sed 's/ version//')
    export KBUILD_COMPILER_STRING
}

## Pre-build dependencies
get_toolchain $CLANG_TYPE
prep_toolchain $CLANG_TYPE

## Telegram info variables

CAPTION_BUILD="Build info:
*Device*: \`${DEVICE} [${CODENAME}]\`
*Kernel Version*: \`${LINUX_VER}\`
*Compiler*: \`${KBUILD_COMPILER_STRING}\`
*Linker*: \`$("$CLANG_DIR"/bin/${LINKER} -v | head -n1 | sed 's/(compatible with [^)]*)//' |
            head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')\`
*Branch*: \`$(git rev-parse --abbrev-ref HEAD)\`
*Commit*: [($(git rev-parse HEAD | cut -c -7))]($(echo $KERNEL_URL)/commit/$(git rev-parse HEAD))
*Build type*: \`$BUILD_TYPE\`
*Clean build*: \`$( [ "$DO_CLEAN" -eq 1 ] && echo Yes || echo No )\`
"

# Functions to send file(s) via Telegram's BOT api.
tgs() {
    MD5=$(md5sum "$1" | cut -d' ' -f1)
    curl -fsSL -X POST -F document=@"$1" https://api.telegram.org/bot"${TELEGRAM_BOT_TOKEN}"/sendDocument \
        -F "chat_id=${TELEGRAM_CHAT_ID}" \
        -F "parse_mode=Markdown" \
        -F "disable_web_page_preview=true" \
        -F "caption=${CAPTION_BUILD}*MD5*: \`$MD5\`" &>/dev/null
}

prep_build() {
    # Prepare ccache
    if [ "$USE_CCACHE" = "1" ]; then
        echo -e "\nINFO: Using ccache\n"
        if [ "$IS_GP" = "1" ]; then
            export CCACHE_DIR=$WP/.ccache
            ccache -M 10G
        else
            echo -e "INFO: Environment is not Gitpod, please make sure you setup your own ccache configuration!\n"
        fi
    fi

    # Show compiler information
    echo "Compiler information:"
    echo -e "\nINFO: $KBUILD_COMPILER_STRING\n"
}

build() {
    # Not that necessary anymore, but still export it just in case.
    export PLATFORM_VERSION=11
    export ANDROID_MAJOR_VERSION=r
    export TARGET_SOC=exynos2100

    export LLVM=1
    export LLVM_IAS=1
    export ARCH=arm64

    # Delete leftovers
    rm -f $OUT_KERNEL

    make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="$CCARM64_PREFIX" $DEFCONFIG 2>&1 | tee log.txt

    if [ $DO_MENUCONFIG = "1" ]; then
        make O=out menuconfig
    fi

    ## Start the build
    
    echo -e "\nINFO: Starting compilation...\n"

    make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="$CCARM64_PREFIX" dtbs 2>&1 | tee log.txt
    if [ $USE_CCACHE = "1" ]; then
        make -j$(nproc --all) O=out CC="ccache clang" CROSS_COMPILE="$CCARM64_PREFIX" 2>&1 | tee log.txt
    else
        make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="$CCARM64_PREFIX" 2>&1 | tee log.txt
    fi
    make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="$CCARM64_PREFIX" INSTALL_MOD_STRIP="--strip-debug --keep-section=.ARM.attributes" INSTALL_MOD_PATH="$MOD_OUTDIR" modules_install 2>&1 | tee log.txt
}

packing() {
    # # Build zip
    # if [ $DO_ZIP = 1 ]; then
    #     echo -e "\nINFO: Building zip..."
    #     cd "$(pwd)/build/zip"
    #     rm -f "$ZIP_PATH"
    #     brotli --quality=3 -c boot.img > boot.br
    #     brotli --quality=3 -c vendor_boot.img > vendor_boot.br
    #     zip -r9 -q "$ZIP_PATH" META-INF boot.br vendor_boot.br
    #     rm -f boot.br vendor_boot.br
    #     cd "$KDIR"
    #     echo -e "INFO: Done! \nINFO: Output: $ZIP_PATH\n"
    # fi

    # Make an AnyKernel3-based zip
    if [ $DO_ZIP = 1 ]; then
        if [ -d $AK3_DIR ]; then
            AK3_TEST=1
            echo -e "\nINFO: AK3_TEST flag set because local AnyKernel3 dir was found"
        else
            if ! git clone -q -b $AK3_BRANCH --depth=1 $AK3_URL $AK3_DIR; then
                echo -e "\nERROR: Failed to clone AnyKernel3!"
                exit 1
            fi
        fi
        echo -e "\nINFO: Building zip..."
        cd "$AK3_DIR"
        cp -f "$OUT_VENDORBOOTIMG" vendor_boot.img
        cp -f "$OUT_KERNEL" .
        zip -r9 -q "$ZIP_PATH" * -x .git .github README.md
        cd "$KDIR"
        echo -e "INFO: Done! \nINFO: Output: $ZIP_PATH\n"
        if [ $AK3_TEST = 1 ]; then
            echo -e "\nINFO: Skipping deletion of AnyKernel3 dir because test flag is set"
        else
            rm -rf $AK3_DIR
        fi
    fi

    # Build tar
    if [ $DO_TAR = 1 ]; then
        echo -e "\nINFO: Building tar..."
        cd "$(pwd)/build"
        rm -f "$TAR_PATH"
        lz4 -c -12 -B6 --content-size "$OUT_BOOTIMG" > boot.img.lz4 2>/dev/null
        lz4 -c -12 -B6 --content-size "$OUT_VENDORBOOTIMG" > vendor_boot.img.lz4 2>/dev/null
        tar -cf "$TAR_PATH" boot.img.lz4 vendor_boot.img.lz4
        rm -f boot.img.lz4 vendor_boot.img.lz4
        cd "$KDIR"
        echo -e "INFO: Done! \nINFO: Output: $TAR_PATH\n"
    fi
}

post_build() {
    ## Check if the kernel binaries were built.
    if [ -f "out/arch/arm64/boot/Image" ]; then
        echo -e "\nINFO: Kernel compiled succesfully!...\n"
    else
        echo -e "\nERROR: Kernel files not found! Compilation failed?"
        echo -e "\nINFO: Uploading log to oshi.at\n"
        curl -T log.txt oshi.at
        exit 1
    fi

    ## Post build setup
    rm -rf "$TMPDIR"
    rm -f "$OUT_BOOTIMG"
    rm -f "$OUT_VENDORBOOTIMG"
    rm -rf "$RAMDISK_DIR"
    mkdir "$TMPDIR"
    mkdir "$RAMDISK_DIR"
    mkdir -p "$MODULES_DIR/0.0"

    cp -rf "$IN_VBOOT"/* "$RAMDISK_DIR/"

    # Handle compiled modules
    if ! find "$MOD_OUTDIR/lib/modules" -mindepth 1 -type d | read; then
        echo -e "\nERROR: Unknown error!\n"
        exit 1
    fi

    missing_modules=""

    # If any module from modules.load was not compiled, abort.
    for module in $(cat "$IN_VBOOT/lib/modules/modules.load"); do
        i=$(find "$MOD_OUTDIR/lib/modules" -name $module);
        if [ -f "$i" ]; then
            cp -f "$i" "$MODULES_DIR/0.0/$module"
        else
        missing_modules="$missing_modules $module"
        fi
    done

    if [ "$missing_modules" != "" ]; then
            echo "ERROR: the following modules were not found: $missing_modules"
        exit 1
    fi

    # Prepare ramdisk
    depmod 0.0 -b "$RAMDISK_DIR"
    sed -i 's/\([^ ]\+\)/\/lib\/modules\/\1/g' "$MODULES_DIR/0.0/modules.dep"
    cd "$MODULES_DIR/0.0"
    for i in $(find . -name "modules.*" -type f); do
        if [ $(basename "$i") != "modules.dep" ] && [ $(basename "$i") != "modules.softdep" ] && [ $(basename "$i") != "modules.alias" ]; then
            rm -f "$i"
        fi
    done
    cd "$KDIR"

    cp -f "$IN_VBOOT/lib/modules/modules.load" "$MODULES_DIR/0.0/modules.load"
    mv "$MODULES_DIR/0.0"/* "$MODULES_DIR/"
    rm -rf "$MODULES_DIR/0.0"

    # Build the images
    echo -e "\nINFO: Building dtb image..."
    python "$MKDTBOIMG" create "$OUT_DTBIMAGE" --custom0=0x00000000 --custom1=0xff000000 --version=0 --page_size=2048 "$IN_DTB" || exit 1

    echo -e "\nINFO: Building boot image..."
    $MKBOOTIMG --header_version 3 \
        --kernel "$OUT_KERNEL" \
        --output "$OUT_BOOTIMG" \
        --ramdisk "$PREBUILT_RAMDISK" \
        --pagesize 4096 \
        --os_version 11.0.0 \
        --os_patch_level 2024-10 || exit 1
    echo -e "INFO: Done!"

    echo -e "\nINFO: Building vendor_boot image..."
    cd "$RAMDISK_DIR"
    find . | cpio --quiet -o -H newc -R root:root | gzip -9 > ../ramdisk.cpio.gz
    cd ..

    $MKBOOTIMG --header_version 3 \
        --vendor_boot "$OUT_VENDORBOOTIMG" \
        --vendor_cmdline "androidboot.selinux=permissive loop.max_part=7" \
        --dtb "$OUT_DTBIMAGE" \
        --vendor_ramdisk "$(pwd)/ramdisk.cpio.gz" \
        --os_version 11.0.0 \
        --os_patch_level 2024-10 \
        --board SRPUG16A011KU \
        --dtb_offset 0x101f00000 \
        --kernel_offset 0x00008000 \
        --ramdisk_offset 0x01000000 \
        --tags_offset 0x00000100 \
        --base 0x10000000 \
        --pagesize 2048 || exit 1glo

    cd "$KDIR"

    echo -e "INFO: Done!"

    packing
}

upload() {
    cd $KDIR
    if [[ "${DO_OSHI}" = "1" ]]; then
    echo -e "\nINFO: Uploading to oshi.at\n"
    curl -T $ZIP_PATH oshi.at; echo
    fi

    if [[ "${DO_TG}" = "1" ]]; then
            echo -e "\nINFO: Uploading to Telegram\n"
            tgs $ZIP_PATH
            echo "Done!"
    fi
    if [[ "${BUILD_LOG}" = "1" ]]; then
        echo -e "\nINFO: Uploading log to oshi.at\n"
        curl -T log.txt oshi.at
    fi
    # Delete any leftover zip files
    #rm -f $KDIR/build/*zip
}

clean() {
    make clean
    make mrproper
}

clean_tmp() {
    echo -e "INFO: Cleaning after build..."
    rm -rf "$TMPDIR"
    rm -rf "$MOD_OUTDIR"
    rm -f "${OUT_VENDORBOOTIMG}" "${OUT_BOOTIMG}"
}

# Do a clean build?
if [[ $DO_CLEAN = "1" ]]; then
    clean
fi
## Run build
prep_build
build
post_build
clean_tmp

upload
