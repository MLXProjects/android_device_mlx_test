# AndroidBoard.mk for mlx_test

LOCAL_PATH := $(call my-dir)

# Use the prebuilt kernel in this device tree.
ifeq ($(TARGET_PREBUILT_KERNEL),)
TARGET_PREBUILT_KERNEL := $(LOCAL_PATH)/prebuilt/kernel
endif

# Copy the prebuilt kernel into the boot image.
file := $(INSTALLED_KERNEL_TARGET)
ALL_PREBUILT += $(file)
$(file): $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)
