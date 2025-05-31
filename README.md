## client for `cmd_skt`
### compile
with android-ndk-r17c-windows-x86_64
```
export NDK="X:/android-ndk-r17c"
export PATH=$NDK:$PATH
export NDK_GCC_arm="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/windows-x86_64/bin/arm-linux-androideabi-gcc"
export NDK_GCC_arm_64="$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/aarch64-linux-android-gcc"
export NDK_CFIG_arm="--sysroot=$NDK/platforms/android-23/arch-arm -isystem $NDK/sysroot/usr/include -isystem $NDK/sysroot/usr/include/arm-linux-androideabi"
export NDK_CFIG_arm_64="--sysroot=$NDK/platforms/android-23/arch-arm64 -isystem $NDK/sysroot/usr/include -isystem $NDK/sysroot/usr/include/aarch64-linux-android"
$NDK_GCC_arm $NDK_CFIG_arm -pie cli.c -o cli32
$NDK_GCC_arm $NDK_CFIG_arm -pie clid.c -o clid32
$NDK_GCC_arm_64 $NDK_CFIG_arm_64 -pie cli.c -o cli64
$NDK_GCC_arm_64 $NDK_CFIG_arm_64 -pie clid.c -o clid64
```
