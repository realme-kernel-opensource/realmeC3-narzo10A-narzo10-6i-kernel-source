/**************************************************************
* Copyright (c) 201X- 201X OPPO Mobile Comm Corp., Ltd.
*
* File : Op_bootprof.c
* Description: Source file for bootlog.
* Storage the boot log on proc.
* Version : 1.0
* Date : 2018-10-18
* Author : bright.zhang@oppo.com
* ---------------- Revision History: --------------------------
* <version> <date> < author > <desc>
****************************************************************/

/*=============================================================================

                            INCLUDE FILES FOR MODULE

=============================================================================*/
#ifdef VENDOR_EDIT
//Liang.Zhang@PSW.TECH.BOOTUP, 2018/10/19, Add for get bootup log
#ifdef HANG_OPPO_ALL

#define OPPO_PARTITION_OPPORESERVE1_EMMC "/dev/block/platform/soc/c0c4000.sdhci/by-name/opporeserve1"
#define OPPO_PARTITION_OPPORESERVE1_UFS "/dev/block/platform/soc/1d84000.ufshc/by-name/opporeserve1"

#define OPPO_PARTITION_OPPORESERVE1_LINK "/dev/block/by-name/opporeserve1"
#define OPPO_PARTITION_OPPORESERVE1_LINK2 "/dev/block/by-name/opporeserve3"

#define OPPO_FATAL_ERR_TO_RECOVERY "--fatal_err_to_recovery"
#define OPPO_FATAL_ERR_TO_WIPEDATA "--data_err_to_recovery"

#define OPPO_EMMC_PARTITION_BOOTLOG_OFFSET (8450 * 512)
#define OPPO_UFS_PARTITION_BOOTLOG_OFFSET (1080 * 4096)
#define OPPO_EMMC_PARTITION_SPEC_FLAG_OFFSET (254 * 512)   // start from last 2 block in opporeserve1(128KB)
#define OPPO_UFS_PARTITION_SPEC_FLAG_OFFSET (30 * 4096)    // start from last 2 block in opporeserve1(128KB)

#define FATAL_FLAG 0x7C
#define RECOVERY_MAGIC_LEN 16
#define RECOVERY_SUM 2   // for better user feelling
#define RECOVERY_FLAG 0x7C

#define RESTART_AND_RECOVERY 2

#ifndef O_RDONLY
#define O_RDONLY  00
#endif

#ifndef O_RDWR
#define O_RDWR    02
#endif

#ifndef O_CREAT
#define O_CREAT  0100
#endif

#ifndef EROFS
#define EROFS     30    /* Read-only file system */
#endif

#ifndef ENOENT
#define	ENOENT     2	/* No such file or directory */
#endif

#ifndef ENOSPC
#define	ENOSPC    28	/* No space left on device */
#endif

struct pon_struct
{
    char magic[RECOVERY_MAGIC_LEN]; //KernelRecovery
    int pon_state[RECOVERY_SUM];
    int write_count;    //total power on count
};

extern struct file *filp_open(const char *, int, umode_t);
extern int filp_close(struct file *, fl_owner_t id);
extern int hang_oppo_recovery_method;
extern int hang_oppo_main_on;

#define PON_STATE_DEFAULT 0x00
#define SIZE_OF_PON_STRUCT sizeof(struct pon_struct)

#endif  //HANG_OPPO_ALL
#endif

