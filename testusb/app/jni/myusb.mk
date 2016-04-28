LOCAL_PATH := $(call my-dir)

LIBUSB_ROOT_REL:= ../../..
LIBUSB_ROOT_ABS:= $(LOCAL_PATH)/../..

include $(CLEAR_VARS)

LOCAL_SRC_FILES := usbaudio_dump.c

LOCAL_C_INCLUDES += \
  $(LIBUSB_ROOT_ABS)

LOCAL_CFLAGS := -std=c11 --include jaemon.h
LOCAL_SHARED_LIBRARIES += usb-1.0 jaemon
LOCAL_LDFLAGS += -Wl,--export-dynamic


LOCAL_LDLIBS := -llog

LOCAL_MODULE	:= usbaudio

include $(BUILD_SHARED_LIBRARY)

