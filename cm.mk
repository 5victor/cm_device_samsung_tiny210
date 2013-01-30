## Specify phone tech before including full_phone
$(call inherit-product, vendor/cm/config/gsm.mk)

# Release name
PRODUCT_RELEASE_NAME := tiny210

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_tablet_wifionly.mk)

# Inherit device configuration
$(call inherit-product, device/samsung/tiny210/device_tiny210.mk)

## Device identifier. This must come after all inclusions
PRODUCT_DEVICE := tiny210
PRODUCT_NAME := cm_tiny210
PRODUCT_BRAND := samsung
PRODUCT_MODEL := tiny210
PRODUCT_MANUFACTURER := samsung
