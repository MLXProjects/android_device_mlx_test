# Minimal config for generic ARM
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_NO_RECOVERY := true
TARGET_NO_RADIOIMAGE := true
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a
TARGET_ARCH_VARIANT_CPU := cortex-a9
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
# no camera hardware
USE_CAMERA_STUB := true
# generic/unknown audio driver
BOARD_USES_GENERIC_AUDIO := true
# use init.rc from device tree
TARGET_PROVIDES_INIT_RC := true
# disable wifi/bluetooth...
WPA_BUILD_SUPPLICANT := false
BOARD_HAVE_BLUETOOTH := false
# not sure if this is needed
HAVE_NO_RFKILL_SWITCH := true
# without this, everything segfaults
ARCH_ARM_HAVE_TLS_REGISTER := true
# use EXT2 for images
TARGET_USERIMAGES_USE_EXT2 := true
# disable sparse - not sure if it's even supported at Froyo
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
# enable classes.dex pre-optimization
WITH_DEXPREOPT := true
