# Inherit everything from the base generic product
$(call inherit-product, build/target/product/generic.mk)

# add device config
include $(LOCAL_PATH)/BoardConfig.mk

# override device-specific info
PRODUCT_NAME := mlx_test
PRODUCT_DEVICE := mlx_test
PRODUCT_MODEL := MLX test device
PRODUCT_BRAND := MLX
PRODUCT_MANUFACTURER := MLX

# custom init script
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootfs/init.rc:root/init.rc

# Live wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapers \
    LiveWallpapersPicker \
    VisualizationWallpapers \
    librs_jni

# custom sizes & locales
PRODUCT_LOCALES := ldpi hdpi mdpi nodpi en_US es_US
