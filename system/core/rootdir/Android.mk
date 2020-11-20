LOCAL_PATH:= $(call my-dir)

#######################################
# init.rc
# Only copy init.rc if the target doesn't have its own.
ifneq ($(TARGET_PROVIDES_INIT_RC),true)
include $(CLEAR_VARS)

LOCAL_MODULE := init.rc
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)

include $(BUILD_PREBUILT)
endif
#######################################
# init.environ.rc

include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE := init.environ.rc
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)

# Put it here instead of in init.rc module definition,
# because init.rc is conditionally included.
#
# create some directories (some are mount points)
LOCAL_POST_INSTALL_CMD := mkdir -p $(addprefix $(TARGET_ROOT_OUT)/, \
    sbin dev proc sys system data)

include $(BUILD_SYSTEM)/base_rules.mk

===========================================================================
wwxx
$< 表示依赖文件, 即$(LOCAL_PATH)/init.environ.rc.in, 等于system/core/rootdir/init.environ.rc.in
 
$@ 表示目标文件, 即$(LOCAL_BUILT_MODULE), 等于out/target/product/imx6/obj/ETC/init.environ.rc_intermediates/init.environ.rc
 
$(hide) = @, @加命令 表示只显示命令结果, 不显示命令本身
 
sed -e 's?%BOOTCLASSPATH%?$(PRODUCT_BOOTCLASSPATH)?g' $< > $@ 
这条命令表示将$<文件里面的%BOOTCLASSPATH%替换为$(PRODUCT_BOOTCLASSPATH)，并将结果输出到$@中。

PRODUCT_BOOTCLASSPATH 其实在build/core/dex_preopt.mk中定义
===========================================================================

$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/init.environ.rc.in
	@echo "Generate: $< -> $@"
	@mkdir -p $(dir $@)
	$(hide) sed -e 's?%BOOTCLASSPATH%?$(PRODUCT_BOOTCLASSPATH)?g' $< >$@

#######################################
