# inherit everything from the base generic product
$(call inherit-product, build/target/product/generic.mk)

# add device config
include $(LOCAL_PATH)/BoardConfig.mk

# include any module here
include $(call all-subdir-makefiles)

# override device-specific info
PRODUCT_NAME := full_test
PRODUCT_MANUFACTURER := MLX
PRODUCT_BRAND := MLX
PRODUCT_DEVICE := test
PRODUCT_MODEL := Test device

# custom init script
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootfs/init.rc:root/init.rc 

# live wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapers \
    LiveWallpapersPicker \
    VisualizationWallpapers \
    librs_jni

# framebuffer-only gralloc HAL
PRODUCT_PACKAGES += \
    gralloc.test

# minimal lights HAL
PRODUCT_PACKAGES += \
    lights.test

# boot-time multi-touch to single-touch input proxy daemon
PRODUCT_PACKAGES += \
    vtouch

# custom sizes & locales
PRODUCT_LOCALES := en_US es_US ldpi hdpi mdpi nodpi

# set dpi - please change according to your screen size!
ADDITIONAL_BUILD_PROPERTIES += ro.sf.lcd_density=160
