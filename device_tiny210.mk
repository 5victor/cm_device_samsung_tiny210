$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

# The gps config appropriate for this device
$(call inherit-product, device/common/gps/gps_us_supl.mk)

$(call inherit-product-if-exists, vendor/samsung/tiny210/tiny210-vendor.mk)

DEVICE_PACKAGE_OVERLAYS += device/samsung/tiny210/overlay

LOCAL_PATH := device/samsung/tiny210

#PRODUCT_COPY_FILES += \

$(call inherit-product, build/target/product/full.mk)

PRODUCT_BUILD_PROP_OVERRIDES += BUILD_UTC_DATE=0
PRODUCT_NAME := full_tiny210
PRODUCT_DEVICE := tiny210
PRODUCT_BRAND := Android
PRODUCT_MODEL := Full CM on tiny210
PRODUCT_RESTRICT_VENDOR_FILES := true

PRODUCT_COPY_FILES := \
	device/samsung/tiny210/init.mini210.rc:root/init.mini210.rc\
        device/samsung/tiny210/ueventd.mini210.rc:root/ueventd.mini210.rc \
        device/samsung/tiny210/vold.fstab:system/etc/vold.fstab \
        device/samsung/tiny210/ft5x0x_ts.idc:system/usr/idc/ft5x0x_ts.idc

PRODUCT_PACKAGES += \
        audio.primary.mini210 \
        tinyplay \
        tinycap \
        tinymix

$(call inherit-product, device/samsung/tiny210/hwopengl.mk)
