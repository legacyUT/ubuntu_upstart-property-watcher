LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := watcher.c

LOCAL_C_INCLUDES := \
	bionic/libc/bionic

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libc

LOCAL_MODULE := watcher
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
