LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := upstart-property-watcher.c

LOCAL_C_INCLUDES := \
	bionic/libc/bionic

LOCAL_STATIC_LIBRARIES := \
	libc \
	libdl \
	liblog

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_MODULE := upstart-property-watcher
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
