ifeq ($(TARGET_PRODUCT),sdm660_64)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=660
else ifeq ($(TARGET_PRODUCT),msm8953_64)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=8953
else ifeq ($(TARGET_PRODUCT),msm8952_64)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=8976
else ifeq ($(shell echo $(TARGET_PRODUCT) | sed -e 's/_1.*//g'),full_oppo6763)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6763
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
else ifeq ($(shell echo $(TARGET_PRODUCT) | sed -e 's/_1.*//g'),full_oppo6771)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6771
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
else ifeq ($(shell echo $(TARGET_PRODUCT) | sed -e 's/_1.*//g'),full_oppo6779)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6779
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
else ifeq ($(shell echo $(TARGET_PRODUCT) | sed -e 's/_1.*//g'),full_oppo6785)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6785
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
else ifeq ($(shell echo $(TARGET_PRODUCT) | sed -e 's/_1.*//g'),full_oppo6769)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6768
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
else ifeq ($(TARGET_PRODUCT),sdm710)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=710
else ifeq ($(TARGET_PRODUCT),msmnile)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=855
else ifeq ($(TARGET_PRODUCT),trinket)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6125
ccflags-y += $(DEFS_IRQ)
else ifeq ($(TARGET_PRODUCT),sm6150)
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=7150
else
DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=0
endif

#DEFS_PLATFORM = -DCONFIG_OPPO_FINGERPRINT_PLATFORM=6785
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_inc
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/asf_export_inc
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/masp/asfv2/module
ccflags-y += $(DEFS_PLATFORM)

obj-y     +=  oppo_fp_common.o

