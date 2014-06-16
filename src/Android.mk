LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= bt-pdubuf.c \
                  bt-proto.c \
                  core.c \
                  core-io.c \
                  loop.c \
                  main.c \
                  service.c \
                  task.c
LOCAL_CFLAGS := -DANDROID_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_SHARED_LIBRARIES := libcutils liblog
LOCAL_MODULE:= bluetoothd
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXECUTABLE)

