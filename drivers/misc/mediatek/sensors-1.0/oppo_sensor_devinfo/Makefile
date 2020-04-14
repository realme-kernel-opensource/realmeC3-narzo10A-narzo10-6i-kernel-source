ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/sensorHub/inc_v1
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(CONFIG_MTK_PLATFORM)
#ifndef VENDOR_EDIT
#Fuchun.Liao@BSP.CHG.Basic 2018/04/21 modify for build error
#obj-y	:=  sensor_devinfo.o
#else
obj-$(CONFIG_NANOHUB)	+= sensor_devinfo.o
#endif /* VENDOR_EDIT */
