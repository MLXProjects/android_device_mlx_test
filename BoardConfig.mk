# Minimal config for generic ARM
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := false
TARGET_NO_RECOVERY := true
TARGET_NO_RADIOIMAGE := true
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a
TARGET_ARCH_VARIANT_CPU := cortex-a9
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
ARCH_ARM_HAVE_TLS_REGISTER := true
# set board name
TARGET_BOOTLOADER_BOARD_NAME := test
BOARD_KERNEL_CMDLINE := androidboot.hardware=test
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
# use EXT for images (depending on android version, the right flag is parsed)
TARGET_USERIMAGES_USE_EXT2 := true
TARGET_USERIMAGES_USE_EXT4 := true
# disable sparse - just in case for later versions
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
# enable classes.dex pre-optimization
WITH_DEXPREOPT := true
