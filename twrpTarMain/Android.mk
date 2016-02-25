LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    twrpTarMain.cpp \
    ../gui/twmsg.cpp \
    ../progresstracking.cpp \
    ../tarWrite.c \
    ../twrp-functions.cpp \
    ../twrpDU.cpp \
    ../twrpTar.cpp

LOCAL_CFLAGS:= -g -c -W -DBUILD_TWRPTAR_MAIN -DHAVE_SELINUX

LOCAL_C_INCLUDES := \
    external/libselinux/include

LOCAL_STATIC_LIBRARIES := \
    libc \
    libselinux \
    libstdc++ \
    libtar_twrp_static

ifneq ($(RECOVERY_SDCARD_ON_DATA),)
    LOCAL_CFLAGS += -DRECOVERY_SDCARD_ON_DATA
endif
ifeq ($(TW_EXCLUDE_ENCRYPTED_BACKUPS), true)
    LOCAL_CFLAGS += -DTW_EXCLUDE_ENCRYPTED_BACKUPS
else
    LOCAL_STATIC_LIBRARIES += libopenaes_static
endif

LOCAL_MODULE:= twrpTar_static
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    twrpTarMain.cpp \
    ../gui/twmsg.cpp \
    ../progresstracking.cpp \
    ../tarWrite.c \
    ../twrp-functions.cpp \
    ../twrpDU.cpp \
    ../twrpTar.cpp

LOCAL_CFLAGS:= -g -c -W -DBUILD_TWRPTAR_MAIN -DHAVE_SELINUX

LOCAL_C_INCLUDES := \
    external/libselinux/include

LOCAL_SHARED_LIBRARIES := \
    libc \
    libselinux \
    libstdc++ \
    libtar_twrp

ifneq ($(RECOVERY_SDCARD_ON_DATA),)
    LOCAL_CFLAGS += -DRECOVERY_SDCARD_ON_DATA
endif
ifeq ($(TW_EXCLUDE_ENCRYPTED_BACKUPS), true)
    LOCAL_CFLAGS += -DTW_EXCLUDE_ENCRYPTED_BACKUPS
else
    LOCAL_SHARED_LIBRARIES += libopenaes
endif

LOCAL_MODULE:= twrpTar
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
include $(BUILD_EXECUTABLE)
