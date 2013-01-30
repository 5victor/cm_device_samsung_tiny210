LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.mini210

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SRC_FILES := audio_hw.c

LOCAL_C_INCLUDES += \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils) \
    $(call include-path-for, audio-effects)
LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libaudioutils libdl
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
