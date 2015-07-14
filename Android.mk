LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ANDROID_VERSION_MAJOR := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
ANDROID_VERSION_MINOR := $(word 2, $(subst ., , $(PLATFORM_VERSION)))
ANDROID_VERSION_PATCH := $(word 3, $(subst ., , $(PLATFORM_VERSION)))
IS_ANDROID_5 := $(shell test $(ANDROID_VERSION_MAJOR) -eq 5 && echo true)

LOCAL_CFLAGS += \
	-DANDROID_VERSION_MAJOR=$(ANDROID_VERSION_MAJOR) \
	-DANDROID_VERSION_MINOR=$(ANDROID_VERSION_MINOR) \
	-DANDROID_VERSION_PATCH=$(ANDROID_VERSION_PATCH)

LOCAL_SRC_FILES := upstart-property-watcher.c

ifeq ($(IS_ANDROID_5),true)
LOCAL_C_INCLUDES := \
	prebuilts/ndk/current/platforms/android-19/arch-arm/usr/include
else
LOCAL_C_INCLUDES := \
	bionic/libc/bionic
endif

LOCAL_MODULE := upstart-property-watcher

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_SBIN_UNSTRIPPED)

LOCAL_STATIC_LIBRARIES := libcutils libc liblog
include $(BUILD_EXECUTABLE)
