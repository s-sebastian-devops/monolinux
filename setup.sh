# Monolinux root folder.
export ML_ROOT=$(readlink -f .)

# Path to source code archives.
export ML_SOURCES=$ML_ROOT/sources

# Linux kernel version to use.
export ML_LINUX_VERSION=5.1.3

# Linux kernel configuration.
export ML_LINUX_CONFIG=$ML_ROOT/configs/x86_64-default.config

# Linux kernel URL.
export ML_LINUX_URL=https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-$ML_LINUX_VERSION.tar.xz

export PATH=$PATH:$ML_ROOT/bin
export PATH=$(readlink -f x86_64-linux-musl-cross/bin):$PATH

# Cross compilation.

export CROSS_COMPILE=x86_64-linux-musl-
# autotools: The system where built programs and libraries will run.
export ML_AUTOTOOLS_HOST=x86_64-linux-musl

# For ARM.
# export ARCH=arm
# export CROSS_COMPILE=arm-linux-musleabi-
# export ML_AUTOTOOLS_HOST=arm-linux-musleabi
