#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=11
export ANDROID_MAJOR_VERSION=r
export SEC_BUILD_CONF_VENDOR_BUILD_OS=13

make ARCH=arm64 exynos2100-r9sxxx_defconfig
make ARCH=arm64 -j16
