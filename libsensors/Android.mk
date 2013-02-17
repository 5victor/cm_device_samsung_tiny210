LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sensors.mini210

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SRC_FILES := \
	sensors_hw.c \
	acceler.c

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
