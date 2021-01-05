#
# Makefile for kernel Silead Fingerprint drivers.
#

ifeq ($(CONFIG_OPPO_FINGERPRINT_MTK),y)
   #include $(srctree)/drivers/misc/mediatek/Makefile.custom
   ccflags-y += -DBSP_SIL_PLAT_MTK
   ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
   subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
   subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
   subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
   include $(srctree)/drivers/misc/mediatek/gud/Makefile.include

   obj-y += silead_fp.o
endif


