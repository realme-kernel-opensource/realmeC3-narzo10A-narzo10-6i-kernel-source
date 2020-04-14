SDCARDFS_VERSION="0.1"

EXTRA_CFLAGS += -DSDCARDFS_VERSION=\"$(SDCARDFS_VERSION)\"

obj-$(CONFIG_SDCARD_FS) += sdcardfs.o

sdcardfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o packagelist.o derived_perm.o
#ifdef VENDOR_EDIT
#Jiemin.Zhu@PSW.Android.SdardFs, 2017/12/12, Add for sdcardfs delete dcim record
sdcardfs-y += dev.o
#Jiemin.Zhu@PSW.Android.SdcardFs, 2018/08/15, Add for using xattr to record uid
sdcardfs-y += xattr.o
sdcardfs-y += dellog.o
#endif /* VENDOR_EDIT */
