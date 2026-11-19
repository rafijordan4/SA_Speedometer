LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_MODULE := SASpeedometer64
LOCAL_SRC_FILES := main.cpp
LOCAL_CXXFLAGS += -O2 -DNDEBUG -std=c++17 -fno-exceptions -fno-rtti
LOCAL_LDLIBS += -llog
LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
include $(BUILD_SHARED_LIBRARY)
