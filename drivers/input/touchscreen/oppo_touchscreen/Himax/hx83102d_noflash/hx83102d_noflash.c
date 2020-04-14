/**************************************************************
 * Copyright (c)  2008- 2019  Oppo Mobile communication Corp.ltd
 * VENDOR_EDIT
 * File       : hx83102d_noflash.c
 * Description: Source file for hx83102d driver
 * Version   : 1.0
 * Date        : 2019-04-12
 * Author    : Ping.Zhang@PSW.BSP.Tp
 * TAG         : BSP.TP.Init
 * ---------------- Revision History: --------------------------
 *   <version>    <date>          < author >                            <desc>
 ****************************************************************/
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/task_work.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/machine.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_FB
#include <linux/fb.h>
#include <linux/notifier.h>
#endif

#include <linux/spi/spi.h>


#include "hx83102d_noflash.h"

#define OPPO17001TRULY_TD4322_1080P_CMD_PANEL 29

struct timespec timeStart, timeEnd, timeDelta;
struct himax_core_fp g_core_fp;
extern int g_f_0f_updat;
int g_cfg_crc = -1;
int g_cfg_sz;
//uint8_t sram_min[4];
//unsigned char *FW_buf;
struct zf_operation *pzf_op;
bool g_auto_update_flag;
int G_POWERONOF = 1;
int hx83102d_NF_POWERONOF = 1;
int g_zero_event_count;

uint8_t IN_SELF_TEST = 0;
uint8_t IN_BASELINE_TEST = 0;



#define GEST_SUP_NUM 26
/* Setting cust key define (DF = double finger) */
/* {Double Tap, Up, Down, Left, Rright, C, Z, M,
 *	O, S, V, W, e, m, @, (reserve),
 *	Finger gesture, ^, >, <, f(R), f(L), Up(DF), Down(DF),
 *	Left(DF), Right(DF)}
 */
uint8_t gest_event[GEST_SUP_NUM] = {
    0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x81, 0x1D, 0x2D, 0x3D, 0x1F, 0x2F, 0x51, 0x52,
    0x53, 0x54
};

/*gest_event mapping to gest_key_def*/
u8 gest_key_def[GEST_SUP_NUM] = {
    DouTap, Down2UpSwip, Up2DownSwip, Right2LeftSwip, Left2RightSwip, 'C', 'Z', Mgestrue,
    Circle, 'S', UpVee, Wgestrue, 'E', 'm', '@', '?',
    'G', DownVee, LeftVee, RightVee, 'f', 'f', '1', DouSwip,
    '=', '='
};

void himax_set_SMWP_enable(uint8_t SMWP_enable, bool suspended);
void himax_sense_on(uint8_t FlashMode);
/*******Part1: Function Declearation*******/
static int hx83102d_power_control(void *chip_data, bool enable);
static int hx83102d_get_chip_info(void *chip_data);
static int hx83102d_mode_switch(void *chip_data, work_mode mode, bool flag);
static uint32_t himax_hw_check_CRC(uint8_t *start_addr, int reload_length);
static fw_check_state hx83102d_fw_check(void *chip_data,
                                        struct resolution_info *resolution_info, struct panel_info *panel_data);
static void himax_read_FW_ver(void);
static int hx83102d_resetgpio_set(struct hw_resource *hw_res, bool on);
void himax_register_read(uint8_t *read_addr, int read_length,
                         uint8_t *read_data, bool cfg_flag);
void himax_register_write(uint8_t *write_addr, int write_length,
                          uint8_t *write_data, bool cfg_flag);
static int hx83102d_enable_interrupt(struct chip_data_hx83102d *chip_info,
                                     bool enable);
bool himax_enter_safe_mode(void);
int himax_ic_reset(struct chip_data_hx83102d *chip_info, uint8_t loadconfig,
                   uint8_t int_off);

/*******Part2:Call Back Function implement*******/



/*******Part0:LOG TAG Declear********************/
#define TPD_DEVICE "himax,hx83102d_nf"
#define TPD_INFO(a, arg...)  pr_err("[TP]"TPD_DEVICE ": " a, ##arg)
#define TPD_DEBUG(a, arg...)\
do {\
	if (LEVEL_DEBUG == tp_debug)\
		pr_err("[TP]"TPD_DEVICE ": " a, ##arg);\
} while (0)

#define TPD_DETAIL(a, arg...)\
do {\
	if (LEVEL_BASIC != tp_debug)\
		pr_err("[TP]"TPD_DEVICE ": " a, ##arg);\
} while (0)

#define TPD_DEBUG_NTAG(a, arg...)\
do {\
	if (tp_debug)\
		printk(a, ##arg);\
} while (0)

struct himax_report_data *hx_touch_data;
struct chip_data_hx83102d *g_chip_info;
int himax_touch_data_size = 128;
int HX_HW_RESET_ACTIVATE;
static int HX_TOUCH_INFO_POINT_CNT;
int g_lcd_vendor;
int irq_en_cnt;


/*******Part0: SPI Interface***************/
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
const struct mtk_chip_config hx_spi_ctrdata = {
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
};
#else
const struct mt_chip_conf hx_spi_ctrdata = {
    .setuptime = 25,
    .holdtime = 25,
    .high_time = 3, /* 16.6MHz */
    .low_time = 3,
    .cs_idletime = 2,
    .ulthgh_thrsh = 0,

    .cpol = 0,
    .cpha = 0,

    .rx_mlsb = 1,
    .tx_mlsb = 1,

    .tx_endian = 0,
    .rx_endian = 0,

    .com_mod = DMA_TRANSFER,

    .pause = 0,
    .finish_intr = 1,
    .deassert = 0,
    .ulthigh = 0,
    .tckdly = 0,
};
#endif

static ssize_t himax_spi_sync(struct touchpanel_data *ts,
                              struct spi_message *message)
{
    int status;

    status = spi_sync(ts->s_client, message);

    if (status == 0) {
        status = message->status;
        if (status == 0)
            status = message->actual_length;
    }
    return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data,
                          uint32_t length, uint8_t toRetry)
{
    struct spi_message message;
    struct spi_transfer xfer[2];
    int retry = 0;
    int error = -1;

    spi_message_init(&message);
    memset(xfer, 0, sizeof(xfer));

    xfer[0].tx_buf = command;
    xfer[0].len = command_len;
    spi_message_add_tail(&xfer[0], &message);

    xfer[1].rx_buf = data;
    xfer[1].len = length;
    spi_message_add_tail(&xfer[1], &message);

    for (retry = 0; retry < toRetry; retry++) {
        error = spi_sync(private_ts->s_client, &message);
        if (error)
            TPD_INFO("SPI read error: %d\n", error);
        else
            break;
    }
    if (retry == toRetry) {
        TPD_INFO("%s: SPI read error retry over %d\n", __func__, toRetry);
        return -EIO;
    }

    return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{
    struct spi_transfer t = {
        .tx_buf     = buf,
        .len        = length,
    };
    struct spi_message    m;
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return himax_spi_sync(private_ts, &m);
}

static int himax_bus_read(uint8_t command, uint32_t length, uint8_t *data)
{
    int result = 0;
    uint8_t spi_format_buf[3];

    mutex_lock(&(g_chip_info->spi_lock));
    spi_format_buf[0] = 0xF3;
    spi_format_buf[1] = command;
    spi_format_buf[2] = 0x00;

    result = himax_spi_read(&spi_format_buf[0], 3, data, length, 10);
    mutex_unlock(&(g_chip_info->spi_lock));

    return result;
}

static int himax_bus_write(uint8_t command, uint32_t length, uint8_t *data)
{
    int result = 0;
    static uint8_t *spi_format_buf;
    int alloc_size = 49156;
    mutex_lock(&(g_chip_info->spi_lock));
    if (spi_format_buf == NULL)
        spi_format_buf = kzalloc((alloc_size + 2) * sizeof(uint8_t), GFP_KERNEL);

    if (spi_format_buf == NULL) {
        TPD_INFO("%s: Can't allocate enough buf\n", __func__);
        kfree(spi_format_buf);
        return -ENOMEM;
    }
    spi_format_buf[0] = 0xF2;
    spi_format_buf[1] = command;

    memcpy((uint8_t *)(&spi_format_buf[2]), data, length);
    result = himax_spi_write(spi_format_buf, length + 2);
    mutex_unlock(&(g_chip_info->spi_lock));
    return result;
}


/*add for himax*/
void himax_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
    uint8_t data_byte[8];
    int i = 0;
    int j = 0;

    for (i = 0; i < 4; i++) {
        data_byte[i] = reg_byte[i];
    }
    for (j = 4; j < 8; j++) {
        data_byte[j] = write_data[j - 4];
    }

    if (himax_bus_write(0x00, 8, data_byte) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return;
    }
}
static int hx83102d_0f_overlay(int ovl_type, int mode)
{
    int ret = 0;
    uint8_t sram_addr[4] = {0xE0, 0x8C, 0x00, 0x20};
    uint8_t gest_addr[4] = {0x10, 0x7F, 0x00, 0x10};
    uint8_t handshaking_addr[4] = {0xF0, 0x7F, 0x00, 0x10};
	/*uint8_t mpap_addr[4] = {0xEC, 0x73, 0x00, 0x10};*/
    uint8_t tmp_data[4] = {0};
    uint8_t reply = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t crc = 0;
    int retry = 0;
    if (ovl_type == 2) {
        reply = 0x22;
        offset = 0x1B100;
        size = 0x1300;
    } else {
        return 0;
    }

    if (g_chip_info->g_fw_entry == NULL) {
        TPD_INFO("Request TP firmware.\n");
        if (private_ts->fw_update_app_support) {
            ret = request_firmware_select(&g_chip_info->g_fw_entry,
                                          private_ts->panel_data.fw_name,
                                          private_ts->dev);
        } else {
            ret = request_firmware(&g_chip_info->g_fw_entry,
                                   private_ts->panel_data.fw_name,
                                   private_ts->dev);
        }
        if (ret < 0) {
            TPD_INFO("%s, fail in line%d error code=%d\n", __func__, __LINE__, ret);
            if (g_chip_info->g_fw_entry != NULL) {
                release_firmware(g_chip_info->g_fw_entry);
                g_chip_info->g_fw_entry = NULL;
            }
            return ret;
        }
    }

    /*
	himax_register_read(mpap_addr, 4, tmp_data, false);
    if (tmp_data[0] == 0 && tmp_data[1] == 0 && tmp_data[2] == 0
        && tmp_data[3] == 0) {
        TPD_INFO("mpap data is 0x%02X,0x%02X,0x%02X,0x%02X.\n",tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);
	*/
	if (IN_SELF_TEST == 0) {
        /*Erase gesture passwords */
        if (mode == 0) {
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_register_write(gest_addr, 4, tmp_data, false);
            //himax_sense_on(0x00);
            //himax_hx83102d_reload_to_active();

        } else {
            /*FW didn't offer CRC for now`*/
            hx83102d_enable_interrupt(g_chip_info, false);
            himax_enter_safe_mode();
            /*himax_set_SMWP_enable(1, 1);*/
            /*write gesture passwords */
            tmp_data[3] = 0xA5;
            tmp_data[2] = 0x5A;
            tmp_data[1] = 0xA5;
            tmp_data[0] = 0x5A;
            himax_register_write(gest_addr, 4, tmp_data, false);
            do {
                g_core_fp.fp_write_sram_0f(g_chip_info->g_fw_entry,
                                            sram_addr, offset, size);
                crc = himax_hw_check_CRC(sram_addr, size);
                if (crc == 0) {
                    TPD_DETAIL("%s, HW CRC ok in %d time \n", __func__, retry);
                    break;
                } else {
                    TPD_INFO("%s, HW CRC fail in %d time !\n", __func__, retry);
                }
                retry++;
            } while ((crc != 0 && retry < 10));

            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = reply;

            himax_register_write(handshaking_addr, 4, tmp_data, false);

            himax_sense_on(0x00);
            hx83102d_enable_interrupt(g_chip_info, true);

        }
    }

    return 0;
}

void himax_flash_write_burst_length(uint8_t *reg_byte, uint8_t *write_data,
                                    int length)
{
    static uint8_t *data_byte;
    int alloc_size = 49152;
    if (data_byte == NULL) {
        data_byte = kzalloc(sizeof(uint8_t) * (alloc_size + 4),
                            GFP_KERNEL);/*APPly for memcory only once*/
        if (data_byte == NULL) {
            TPD_INFO("%s: Can't allocate enough buf\n", __func__);
            kfree(data_byte);
            return;
        }
    }

    memcpy(data_byte, reg_byte, 4); /* assign addr 4bytes */
    memcpy(data_byte + 4, write_data, length); /* assign data n bytes */

    if (himax_bus_write(0x00, length + 4, data_byte) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return;
    }
}

void himax_burst_enable(uint8_t auto_add_4_byte)
{
    uint8_t tmp_data[4];
    tmp_data[0] = 0x31;

    if (himax_bus_write(0x13, 1, tmp_data) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return;
    }

    tmp_data[0] = (0x10 | auto_add_4_byte);
    if (himax_bus_write(0x0D, 1, tmp_data) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return;
    }
}

void himax_register_read(uint8_t *read_addr, int read_length,
                         uint8_t *read_data, bool cfg_flag)
{
    uint8_t tmp_data[4];
    int ret;
    if (cfg_flag == false) {
        if (read_length > 256) {
            TPD_INFO("%s: read len over 256!\n", __func__);
            return;
        }
        if (read_length > 4)
            himax_burst_enable(1);
        else
            himax_burst_enable(0);

        tmp_data[0] = read_addr[0];
        tmp_data[1] = read_addr[1];
        tmp_data[2] = read_addr[2];
        tmp_data[3] = read_addr[3];
        ret = himax_bus_write(0x00, 4, tmp_data);
        if (ret < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
        tmp_data[0] = 0x00;
        ret = himax_bus_write(0x0C, 1, tmp_data);
        if (ret < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }

        if (himax_bus_read(0x08, read_length, read_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
        if (read_length > 4)
            himax_burst_enable(0);
    } else if (cfg_flag == true) {
        if (himax_bus_read(read_addr[0], read_length, read_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
    } else {
        TPD_INFO("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
        return;
    }
}


void himax_register_write(uint8_t *write_addr, int write_length,
                          uint8_t *write_data, bool cfg_flag)
{
    int i = 0;
    int address = 0;
    if (cfg_flag == false) {
        address = (write_addr[3] << 24) + (write_addr[2] << 16) +
                  (write_addr[1] << 8) + write_addr[0];
        for (i = address; i < address + write_length; i++) {
            if (write_length > 4)
                himax_burst_enable(1);
            else
                himax_burst_enable(0);

            himax_flash_write_burst_length(write_addr, write_data, write_length);
        }
    } else if (cfg_flag == true) {
        if (himax_bus_write(write_addr[0], write_length, write_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
    } else {
        TPD_INFO("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
        return;
    }
}
void himax_mcu_register_write(uint8_t *write_addr, int write_length,
                              uint8_t *write_data, bool cfg_flag)
{
    if (cfg_flag == false) {
        if (write_length > 4)
            himax_burst_enable(1);
        else
            himax_burst_enable(0);

        himax_flash_write_burst_length(write_addr, write_data, write_length);

    } else if (cfg_flag == true) {
        if (himax_bus_write(write_addr[0], write_length, write_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
    } else {
        TPD_INFO("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
    }
    return;
}

bool himax_sense_off(void)
{
    uint8_t cnt = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    /*
    tmp_addr[3] = 0x90;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x5C;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0xA5;
    himax_flash_write_burst(tmp_addr, tmp_data);

    msleep(20);
    */
    do {
        if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00
                         && tmp_data[0] != 0x87)) {
            tmp_addr[3] = 0x90;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x5c;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0xA5;

            himax_register_write(tmp_addr, 4, tmp_data, 0);
        }
        /*msleep(20);*/
        usleep_range(10000, 10001);
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;

        /* check fw status */
        himax_register_read(tmp_addr, 4, tmp_data, 0);

        if (tmp_data[0] != 0x05) {
            TPD_INFO("%s: Do not need wait FW, Status = 0x%02X!\n", __func__, tmp_data[0]);
            break;
        }

        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x5c;

        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: cnt = %d, data[0] = 0x%02X!\n", __func__, cnt, tmp_data[0]);
    } while (tmp_data[0] != 0x87 && (++cnt < 10));
    cnt = 0;

    do {
        /*
        ===========================================
        0x31 ==> 0x27
        ===========================================
        */
        tmp_data[0] = 0x27;
        if (himax_bus_write(0x31, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x32 ==> 0x95
        ===========================================
        */
        tmp_data[0] = 0x95;
        if (himax_bus_write(0x32, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }

        /*
        ======================
        Check enter_save_mode
        ======================
        */
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        himax_register_read(tmp_addr, 4, tmp_data, false);

        TPD_INFO("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

        if (tmp_data[0] == 0x0C) {
            /*
            =====================================
            Reset TCON
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x20;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            /*
            =====================================
            Reset ADC
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x94;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            return true;
        } else {
            msleep(10);

            himax_ic_reset(g_chip_info, false, false);
        }
    } while (cnt++ < 15);

    return false;
}

bool himax_sense_off_mptest(void)
{
    uint8_t cnt = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    do {
        if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00
                         && tmp_data[0] != 0x87)) {
            tmp_addr[3] = 0x90;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x5C;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0xA5;
            himax_flash_write_burst(tmp_addr, tmp_data);
        }
        msleep(20);
        himax_register_read(tmp_addr, 4, tmp_data, false);
        cnt++;
        TPD_INFO("%s: save mode lock cnt = %d, data[0] = %2X!\n", __func__, cnt,
                 tmp_data[0]);
    } while (tmp_data[0] != 0x87 && (cnt < 50));

    cnt = 0;

    do {
        /*
        ===========================================
        0x31 ==> 0x27
        ===========================================
        */
        tmp_data[0] = 0x27;
        if (himax_bus_write(0x31, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x32 ==> 0x95
        ===========================================
        */
        tmp_data[0] = 0x95;
        if (himax_bus_write(0x32, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ======================
        Check enter_save_mode
        ======================
        */
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        himax_register_read(tmp_addr, 4, tmp_data, false);

        TPD_INFO("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

        if (tmp_data[0] == 0x0C) {
            /*
            =====================================
            Reset TCON
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x20;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            /*
            =====================================
            Reset ADC
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x94;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            return true;
        } else {
            msleep(10);
            himax_ic_reset(g_chip_info, false, false);
        }
    } while (cnt++ < 15);

    return false;
}

bool himax_enter_safe_mode(void)
{
    uint8_t cnt = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    tmp_addr[3] = 0x90;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x5C;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0xA5;
    himax_flash_write_burst(tmp_addr, tmp_data);

    msleep(20);

    do {
        /*
        ===========================================
        0x31 ==> 0x27
        ===========================================
        */
        tmp_data[0] = 0x27;
        if (himax_bus_write(0x31, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x32 ==> 0x95
        ===========================================
        */
        tmp_data[0] = 0x95;
        if (himax_bus_write(0x32, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x31 ==> 0x00
        ===========================================
        */
        tmp_data[0] = 0x00;
        if (himax_bus_write(0x31, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x31 ==> 0x27
        ===========================================
        */
        tmp_data[0] = 0x27;
        if (himax_bus_write(0x31, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ===========================================
        0x32 ==> 0x95
        ===========================================
        */
        tmp_data[0] = 0x95;
        if (himax_bus_write(0x32, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return false;
        }
        /*
        ======================
        Check enter_save_mode
        ======================
        */
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        himax_register_read(tmp_addr, 4, tmp_data, false);

        TPD_DETAIL("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

        if (tmp_data[0] == 0x0C) {
            /*
            =====================================
            Reset TCON
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x20;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            /*
            =====================================
            Reset ADC
            =====================================
            */
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x94;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(tmp_addr, tmp_data);
            return true;
        } else {
            msleep(10);
            himax_ic_reset(g_chip_info, false, false);
        }
    } while (cnt++ < 20);

    return false;
}

void himax_interface_on(void)
{
    uint8_t tmp_data[5];
    uint8_t tmp_data2[2];
    int cnt = 0;

    /* Read a dummy register to wake up I2C.*/
    if (himax_bus_read(0x08, 4, tmp_data) < 0) { /*to knock I2C*/
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return;
    }

    do {
        /*
        ===========================================
        Enable continuous burst mode : 0x13 ==> 0x31
        ===========================================
        */
        tmp_data[0] = 0x31;
        if (himax_bus_write(0x13, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }
        /*
        ===========================================
        AHB address auto +4 : 0x0D ==> 0x11
        Do not AHB address auto +4 : 0x0D ==> 0x10
        ===========================================
        */
        tmp_data[0] = (0x10);
        if (himax_bus_write(0x0D, 1, tmp_data) < 0) {
            TPD_INFO("%s: i2c access fail!\n", __func__);
            return;
        }

        /* Check cmd*/
        himax_bus_read(0x13, 1, tmp_data);
        himax_bus_read(0x0D, 1, tmp_data2);

        if (tmp_data[0] == 0x31 && tmp_data2[0] == 0x10) {
            /*isBusrtOn = true;*/
            break;
        }
        msleep(1);
    } while (++cnt < 10);

    if (cnt > 0)
        TPD_INFO("%s:Polling burst mode: %d times", __func__, cnt);

}

void himax_diag_register_set(uint8_t diag_command)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    TPD_INFO("diag_command = %d\n", diag_command);

    himax_interface_on();

    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x02;
    tmp_addr[1] = 0x04;
    tmp_addr[0] = 0xF4;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = diag_command;
    himax_flash_write_burst(tmp_addr, tmp_data);

    himax_register_read(tmp_addr, 4, tmp_data, false);
    TPD_INFO("%s: tmp_data[3]=0x%02X, tmp_data[2]=0x%02X, tmp_data[1]=0x%02X, tmp_data[0]=0x%02X!\n",
             __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);

}


bool wait_wip(int Timing)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t in_buffer[10];
    int retry_cnt = 0;

    /*
    =====================================
    SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
    =====================================
    */
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x10;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x02;
    tmp_data[1] = 0x07;
    tmp_data[0] = 0x80;
    himax_flash_write_burst(tmp_addr, tmp_data);

    //in_buffer[0] = 0x01;

    do {
        /*
        =====================================
        SPI Transfer Control : 0x8000_0020 ==> 0x4200_0003
        =====================================
        */
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x20;
        tmp_data[3] = 0x42;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x03;
        himax_flash_write_burst(tmp_addr, tmp_data);
        /*
        =====================================
        SPI Command : 0x8000_0024 ==> 0x0000_0005
        read 0x8000_002C for 0x01, means wait success
        =====================================
        */
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x24;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x05;
        himax_flash_write_burst(tmp_addr, tmp_data);

        in_buffer[0] = 0xFF;
        in_buffer[1] = 0xFF;
        in_buffer[2] = 0xFF;
        in_buffer[3] = 0xFF;
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x2C;
        himax_register_read(tmp_addr, 4, in_buffer, false);

        if ((in_buffer[0] & 0x01) == 0x00)
            return true;

        retry_cnt++;

        if (in_buffer[0] != 0x00 || in_buffer[1] != 0x00 || in_buffer[2] != 0x00
            || in_buffer[3] != 0x00) {
            TPD_INFO("%s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d \n",
                     __func__,
                     retry_cnt, in_buffer[0], in_buffer[1], in_buffer[2], in_buffer[3]);
        }

        if (retry_cnt > 100) {
            TPD_INFO("%s: Wait wip error!\n", __func__);
            return false;
        }
        msleep(Timing);
    } while ((in_buffer[0] & 0x01) == 0x01);
    return true;
}

void himax_hx83102d_reload_to_active(void)
{
    uint8_t addr[4] = {0};
    uint8_t data[4] = {0};
    uint8_t retry_cnt = 0;

    addr[3] = 0x90;
    addr[2] = 0x00;
    addr[1] = 0x00;
    addr[0] = 0x48;

    do {
        data[3] = 0x00;
        data[2] = 0x00;
        data[1] = 0x00;
        data[0] = 0xEC;
        himax_register_write(addr, 4, data, 0);

        himax_register_read(addr, 4, data, 0);
        TPD_INFO("%s: data[1]=%d, data[0]=%d, retry_cnt=%d\n", __func__, data[1],
                 data[0], retry_cnt);
        retry_cnt++;
    } while ((data[1] != 0x01 || data[0] != 0xEC) && retry_cnt < 5);
}



void himax_sense_on(uint8_t FlashMode)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int retry = 0;

    TPD_DETAIL("Enter %s  \n", __func__);

    himax_interface_on();
    tmp_addr[3] = 0x90;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x5C;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_flash_write_burst(tmp_addr, tmp_data);

    if (!FlashMode) {
        /*AHBI2C_SystemReset*/
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x18;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x55;
        himax_register_write(tmp_addr, 4, tmp_data, false);
        himax_hx83102d_reload_to_active();
    } else {
        himax_hx83102d_reload_to_active();
        do {
            tmp_addr[3] = 0x90;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x98;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x53;
            himax_register_write(tmp_addr, 4, tmp_data, false);

            tmp_addr[0] = 0xE4;
            himax_register_read(tmp_addr, 4, tmp_data, false);

            TPD_DETAIL("%s:Read status from IC = %X, %X\n", __func__, tmp_data[0],
                       tmp_data[1]);
        } while ((tmp_data[1] != 0x01 || tmp_data[0] != 0x00) && retry++ < 5);

        if (retry >= 5) {
            TPD_INFO("%s: Fail:\n", __func__);

            /*AHBI2C_SystemReset*/
            tmp_addr[3] = 0x90;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x18;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x55;
            himax_register_write(tmp_addr, 4, tmp_data, false);
            himax_hx83102d_reload_to_active();
        } else {
            TPD_DETAIL("%s:OK and Read status from IC = %X,%X\n", __func__, tmp_data[0],
                       tmp_data[1]);

            /* reset code*/
            tmp_data[0] = 0x00;
            if (himax_bus_write(0x31, 1, tmp_data) < 0)
                TPD_INFO("%s: i2c access fail!\n", __func__);
            if (himax_bus_write(0x32, 1, tmp_data) < 0)
                TPD_INFO("%s: i2c access fail!\n", __func__);

            tmp_addr[3] = 0x90;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x98;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_register_write(tmp_addr, 4, tmp_data, false);
        }
    }
}

/*
 * hx83102d_enable_interrupt -   Device interrupt ability control.
 * @chip_info: struct include i2c resource.
 * @enable: disable or enable control purpose.
 * Return  0: succeed, -1: failed.
 */
static int hx83102d_enable_interrupt(struct chip_data_hx83102d *chip_info,
                                     bool enable)
{
    TPD_DETAIL("%s enter, enable = %d.\n", __func__, enable);
    if (enable == true && irq_en_cnt == 0) {
        enable_irq(chip_info->hx_irq);
        irq_en_cnt = 1;
    } else if (enable == false && irq_en_cnt == 1) {
        disable_irq_nosync(chip_info->hx_irq);
        irq_en_cnt = 0;
    } else {
        TPD_DETAIL("irq is not pairing! enable= %d, cnt = %d\n", enable, irq_en_cnt);
    }

    return 0;
}

void himax_in_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len)
{
    /*TPD_INFO("%s: Entering!\n", __func__);*/
    switch (len) {
    case 1:
        cmd[0] = addr;
        /*TPD_INFO("%s: cmd[0] = 0x%02X\n", __func__, cmd[0]);*/
        break;

    case 2:
        cmd[0] = addr % 0x100;
        cmd[1] = (addr >> 8) % 0x100;
        /*TPD_INFO("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X\n", __func__, cmd[0], cmd[1]);*/
        break;

    case 4:
        cmd[0] = addr % 0x100;
        cmd[1] = (addr >> 8) % 0x100;
        cmd[2] = (addr >> 16) % 0x100;
        cmd[3] = addr / 0x1000000;
        /*  TPD_INFO("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X,cmd[2] = 0x%02X,cmd[3] = 0x%02X\n",
        __func__, cmd[0], cmd[1], cmd[2], cmd[3]);
        */
        break;

    default:
        TPD_INFO("%s: input length fault,len = %d!\n", __func__, len);
    }
}

void hx_update_dirly_0f(void)
{
    TPD_INFO("It will update fw after esd event in zero flash mode!\n");
    g_core_fp.fp_0f_operation_dirly();
}

void himax_mcu_sys_reset(void)
{
    himax_register_write (pzf_op->addr_system_reset,  4,  pzf_op->data_system_reset,
                          false);
}

int hx_dis_rload_0f(int disable)
{
    /*Diable Flash Reload*/
    int retry = 10;
    int check_val = 0;
    uint8_t tmp_data[4] = {0};

    TPD_DETAIL("%s: Entering !\n", __func__);

    do {
        himax_flash_write_burst (pzf_op->addr_dis_flash_reload,
                                 pzf_op->data_dis_flash_reload);
        himax_register_read(pzf_op->addr_dis_flash_reload, 4, tmp_data, false);
        TPD_DETAIL("Now data: tmp_data[3] = 0x%02X || tmp_data[2] = 0x%02X || tmp_data[1] = 0x%02X || tmp_data[0] = 0x%02X\n",
                   tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
        if (tmp_data[3] != 0x00 || tmp_data[2] != 0x00 || tmp_data[1] != 0x9A
            || tmp_data[0] != 0xA9) {
            TPD_INFO("Now data: tmp_data[3] = 0x%02X || tmp_data[2] = 0x%02X || tmp_data[1] = 0x%02X || tmp_data[0] = 0x%02X\n",
                     tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
            TPD_INFO("Not Same,Write Fail, there is %d retry times!\n", retry);
        } else {
            check_val = 1;
            TPD_DETAIL("It's same! Write success!\n");
        }
        msleep(5);
    } while (check_val == 0 && retry-- > 0);

    TPD_DETAIL("%s: END !\n", __func__);

    return check_val;
}

void himax_mcu_clean_sram_0f(uint8_t *addr, int write_len, int type)
{
    int total_read_times = 0;
    int max_bus_size = MAX_TRANS_SZ;
    int total_size_temp = 0;
    int total_size = 0;
    int address = 0;
    int i = 0;

    uint8_t fix_data = 0x00;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[MAX_TRANS_SZ] = {0};

    TPD_DETAIL("%s, Entering \n", __func__);

    total_size = write_len;

    if (total_size > 4096)
        max_bus_size = 4096;

    total_size_temp = write_len;

    himax_burst_enable(1);

    tmp_addr[3] = addr[3];
    tmp_addr[2] = addr[2];
    tmp_addr[1] = addr[1];
    tmp_addr[0] = addr[0];
    TPD_DETAIL("%s,  write addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
               __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

    switch (type) {
    case 0:
        fix_data = 0x00;
        break;
    case 1:
        fix_data = 0xAA;
        break;
    case 2:
        fix_data = 0xBB;
        break;
    }

    for (i = 0; i < MAX_TRANS_SZ; i++) {
        tmp_data[i] = fix_data;
    }

    TPD_DETAIL("%s,  total size=%d\n", __func__, total_size);

    if (total_size_temp % max_bus_size == 0)
        total_read_times = total_size_temp / max_bus_size;
    else
        total_read_times = total_size_temp / max_bus_size + 1;

    for (i = 0; i < (total_read_times); i++) {
        /*TPD_DETAIL("[log]write %d time start!\n", i);*/
        if (total_size_temp >= max_bus_size) {
            himax_flash_write_burst_length(tmp_addr, tmp_data,  max_bus_size);
            total_size_temp = total_size_temp - max_bus_size;
        } else {
            TPD_DETAIL("last total_size_temp=%d\n", total_size_temp);
            himax_flash_write_burst_length(tmp_addr, tmp_data,
                                           total_size_temp % max_bus_size);
        }
        address = ((i + 1) * max_bus_size);
        tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);
        tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

        msleep (10);
    }

    TPD_DETAIL("%s, END \n", __func__);
}

void himax_mcu_write_sram_0f(const struct firmware *fw_entry, uint8_t *addr,
                             int start_index, uint32_t write_len)
{
    int total_read_times = 0;
    int max_bus_size = MAX_TRANS_SZ;
    int total_size_temp = 0;
    int total_size = 0;
    int address = 0;
    int i = 0;

    uint8_t tmp_addr[4];
    uint32_t now_addr;

    TPD_DETAIL("%s, ---Entering \n", __func__);

    total_size = fw_entry->size;

    total_size_temp = write_len;

    if (write_len > 49152)
        max_bus_size = 49152;
    else
        max_bus_size = write_len;

    himax_burst_enable(1);

    tmp_addr[3] = addr[3];
    tmp_addr[2] = addr[2];
    tmp_addr[1] = addr[1];
    tmp_addr[0] = addr[0];
    TPD_DETAIL("%s,  write addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
               __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
    now_addr = (addr[3] << 24) + (addr[2] << 16) + (addr[1] << 8) + addr[0];
    TPD_DETAIL("now addr= 0x%08X\n", now_addr);

    TPD_DETAIL("%s,  total size=%d\n", __func__, total_size);

    if (g_chip_info->tmp_data == NULL) {
        /*TPD_INFO("%s,  enteralloc g_chip_info->tmp_data\n", __func__);*/
        g_chip_info->tmp_data = kzalloc (sizeof (uint8_t) * firmware_update_space,
                                         GFP_KERNEL);
        if (g_chip_info->tmp_data == NULL) {
            TPD_INFO("%s, alloc g_chip_info->tmp_data failed\n", __func__);
            return ;
        }
        /*TPD_INFO("%s, end---------alloc g_chip_info->tmp_data\n", __func__);*/
    }
    memcpy (g_chip_info->tmp_data, fw_entry->data, total_size);
    /*
    for(i = 0;i<10;i++)
    {
    	TPD_INFO("[%d] 0x%2.2X", i, tmp_data[i]);
    }
    TPD_INFO("\n");
    */
    if (total_size_temp % max_bus_size == 0)
        total_read_times = total_size_temp / max_bus_size;
    else
        total_read_times = total_size_temp / max_bus_size + 1;

    for (i = 0; i < (total_read_times); i++) {
        /*
        TPD_INFO("[log]write %d time start!\n", i);
        TPD_INFO("[log]addr[3]=0x%02X, addr[2]=0x%02X, addr[1]=0x%02X, addr[0]=0x%02X!\n", tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
        */
        if (total_size_temp >= max_bus_size) {
            himax_flash_write_burst_length(tmp_addr,
                                           &(g_chip_info->tmp_data[start_index + i * max_bus_size]),  max_bus_size);
            total_size_temp = total_size_temp - max_bus_size;
        } else {
            TPD_DETAIL("last total_size_temp=%d\n", total_size_temp);
            himax_flash_write_burst_length(tmp_addr,
                                           &(g_chip_info->tmp_data[start_index + i * max_bus_size]),
                                           total_size_temp % max_bus_size);
        }

        /*TPD_INFO("[log]write %d time end!\n", i);*/
        address = ((i + 1) * max_bus_size);
        tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

        if (tmp_addr[0] <  addr[0])
            tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF) + 1;
        else
            tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);

        udelay (100);
    }
    TPD_DETAIL("%s, ----END \n", __func__);
    memset(g_chip_info->tmp_data, 0, total_size);
}



int hx83102d_nf_parse_bin_cfg_data(const struct firmware *fw_entry,
                                   struct zf_info *zf_info_arr, uint8_t *FW_buf, uint8_t *sram_min)
{
    int part_num = 0;
    int i = 0;
    uint8_t buf[16];
    int i_max = 0;
    int i_min = 0;
    uint32_t dsram_base = 0xFFFFFFFF;
    uint32_t dsram_max = 0;
    int cfg_sz = 0;

    /*1. get number of partition*/
    part_num = fw_entry->data[HX64K + 12];
    TPD_DETAIL("%s, Number of partition is %d\n", __func__, part_num);
    if (part_num <= 1) {
        TPD_INFO("%s, size of cfg part failed! part_num = %d\n", __func__, part_num);
        return LENGTH_FAIL;
    }
    /*2. check struct of array*/
    if (zf_info_arr == NULL || FW_buf == NULL) {
        TPD_INFO("%s, Struct is NULL, %p,%p\n", __func__, zf_info_arr, FW_buf);
        return MEM_ALLOC_FAIL;
    }

    /* i = 0 is fw entity, it need to update by itself*/
    for (i = 1; i < part_num; i++) {
        /*3. get all partition*/
        memcpy(buf, &fw_entry->data[i * 0x10 + HX64K], 16);
        memcpy(zf_info_arr[i].sram_addr, buf, 4);
        zf_info_arr[i].write_size = buf[5] << 8 | buf[4];
        zf_info_arr[i].fw_addr = buf[9] << 8 | buf[8];
        zf_info_arr[i].cfg_addr = zf_info_arr[i].sram_addr[0];
        zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[1] << 8;
        zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[2] << 16;
        zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[3] << 24;
        /*overlay*/
        if (zf_info_arr[i].cfg_addr == 0x20008cE0)
            continue;
        TPD_DETAIL("%s,[%d] SRAM addr = %08X\n", __func__, i, zf_info_arr[i].cfg_addr);
        TPD_DETAIL("%s,[%d] fw_addr = %04X!\n", __func__, i, zf_info_arr[i].fw_addr);
        TPD_DETAIL("%s,[%d] write_size = %d!\n", __func__, i,
                   zf_info_arr[i].write_size);
        if (dsram_base > zf_info_arr[i].cfg_addr) {
            dsram_base = zf_info_arr[i].cfg_addr;
            i_min = i;
        }
        if (dsram_max < zf_info_arr[i].cfg_addr) {
            dsram_max = zf_info_arr[i].cfg_addr;
            i_max = i;
        }
    }
    for (i = 0; i < 4; i++)
        sram_min[i] = zf_info_arr[i_min].sram_addr[i];
    cfg_sz = (dsram_max - dsram_base) + zf_info_arr[i_max].write_size;
    if (cfg_sz % 16 != 0)
        cfg_sz = cfg_sz + 16 - (cfg_sz % 16);

    TPD_DETAIL("%s, cfg_sz = %d!, dsram_base = %X, dsram_max = %X\n", __func__,
               cfg_sz, dsram_base, dsram_max);

    memset(FW_buf, 0x00, sizeof(uint8_t) * cfg_sz);
    for (i = 1; i < part_num; i++) {
        /*overlay*/
        if (zf_info_arr[i].cfg_addr == 0x20008cE0)
            continue;
        if (zf_info_arr[i].cfg_addr % 4 != 0)
            zf_info_arr[i].cfg_addr = zf_info_arr[i].cfg_addr - (zf_info_arr[i].cfg_addr %
                                      4);
        memcpy(&FW_buf[zf_info_arr[i].cfg_addr - dsram_base],
               &fw_entry->data[zf_info_arr[i].fw_addr], zf_info_arr[i].write_size);
    }
    /* debug info */
    /*for (i = 1; i < part_num; i++) {
    different = 0;
    I("--cfg_addr = 0x%08X\n",zf_info_arr[i].cfg_addr);
    I("--bin_addr = 0x%08X\n",zf_info_arr[i].fw_addr);
    I("--write size = %d\n",zf_info_arr[i].write_size);
    for ( j = 0; j < zf_info_arr[i].write_size; j++) {
    if (FW_buf[zf_info_arr[i].cfg_addr - dsram_base + j] != fw_entry->data[zf_info_arr[i].fw_addr + j])
    different++;
    }
    I("--There are %d different\n", different);
    }*/

    return cfg_sz;
}


static int hx83102d_nf_mcu_Calculate_CRC_with_AP(unsigned char *FW_content,
        int CRC_from_FW, int len)
{
    int i, j, length = 0;
    int fw_data;
    int fw_data_2;
    int CRC = 0xFFFFFFFF;
    int PolyNomial = 0x82F63B78;

    length = len / 4;

    for (i = 0; i < length; i++) {
        fw_data = FW_content[i * 4];

        for (j = 1; j < 4; j++) {
            fw_data_2 = FW_content[i * 4 + j];
            fw_data += (fw_data_2) << (8 * j);
        }
        CRC = fw_data ^ CRC;
        for (j = 0; j < 32; j++) {
            if ((CRC % 2) != 0)
                CRC = ((CRC >> 1) & 0x7FFFFFFF) ^ PolyNomial;
            else
                CRC = (((CRC >> 1) & 0x7FFFFFFF)/*& 0x7FFFFFFF*/);
        }
    }

    return CRC;
}


static int hx83102d_nf_zf_part_info(const struct firmware *fw_entry)
{
    int part_num = 0;
    int ret = 0;
    uint8_t buf[16];
    struct zf_info *zf_info_arr;
    uint8_t *FW_buf;
    uint8_t sram_min[4];
    int cfg_crc_sw = 0;
    int cfg_crc_hw = 0;
    int cfg_sz = 0;
    int retry = 3;
    int crc = -1;
	/*
    uint8_t mpap_addr[4] = {0xEC, 0x73, 0x00, 0x10};
    uint8_t recv_data[4] = {0};
	*/
    /*1. get number of partition*/
    part_num = fw_entry->data[HX64K + 12];
    TPD_DETAIL("%s, Number of partition is %d\n", __func__, part_num);
    if (part_num <= 0)
        part_num = 1;

    /*2. initial struct of array*/
    zf_info_arr = kzalloc(part_num * sizeof(struct zf_info), GFP_KERNEL);
    if (zf_info_arr == NULL) {
        TPD_INFO("%s, Allocate ZF info array failed!\n", __func__);
        ret =  MEM_ALLOC_FAIL;
        goto ALOC_ZF_INFO_ARR_FAIL;
    }
    FW_buf = kzalloc(sizeof(uint8_t) * FW_BIN_16K_SZ, GFP_KERNEL);
    if (FW_buf == NULL) {
        TPD_INFO("%s, Allocate FW_buf array failed!\n", __func__);
        ret =  MEM_ALLOC_FAIL;
        goto ALOC_FW_BUF_FAIL;
    }

    /*3. Find the data in the bin file */
    /*CFG*/
    cfg_sz = hx83102d_nf_parse_bin_cfg_data(fw_entry, zf_info_arr, FW_buf,
                                            sram_min);
    TPD_DETAIL("Now cfg_sz=%d\n", cfg_sz);
    if (cfg_sz < 0) {
        ret = LENGTH_FAIL;
        goto END_FUNC;
    } else {
        cfg_crc_sw = hx83102d_nf_mcu_Calculate_CRC_with_AP(FW_buf, 0, cfg_sz);
        TPD_DETAIL("Now cfg_crc_sw=%X\n", cfg_crc_sw);
    }
    /*Fw Entity*/
    memcpy(buf, &fw_entry->data[0 * 0x10 + HX64K], 16);
    memcpy(zf_info_arr[0].sram_addr, buf, 4);
    zf_info_arr[0].write_size = buf[5] << 8 | buf[4];
    zf_info_arr[0].fw_addr = buf[9] << 8 | buf[8];

    /* 4. write to sram*/
    if (hx83102d_NF_POWERONOF == 1) {
        /* FW entity */
        retry = 0;
        do {
            g_core_fp.fp_write_sram_0f(fw_entry, zf_info_arr[0].sram_addr,
                                       zf_info_arr[0].fw_addr, zf_info_arr[0].write_size);
            crc = himax_hw_check_CRC(zf_info_arr[0].sram_addr,  zf_info_arr[0].write_size);
            if (crc == 0) {
                TPD_DETAIL("%s, HW CRC ok in %d time \n", __func__, retry);
                break;
            } else {
                TPD_INFO("%s, HW CRC fail in %d time !\n", __func__, retry);
            }
            retry++;
        } while ((crc != 0 && retry < 30) || (g_chip_info->using_headfile
                                              && retry < 15));

        if (crc != 0)
            TPD_INFO("config info CRC Fail!\n");

        retry = 0;
        crc = -1;

        /*for overlay area, using border for default*/
		/*
        himax_register_read(mpap_addr, 4, recv_data, 0);
        if (recv_data[3] == 0 && recv_data[2] == 0 && recv_data[1] == 0
            && recv_data[0] == 0) {
		*/
		if (IN_BASELINE_TEST == 0 ) {

        zf_info_arr[0].fw_addr = 0x1C400;
        zf_info_arr[0].write_size = 0x1100;

        zf_info_arr[0].sram_addr[3] = 0x20;
        zf_info_arr[0].sram_addr[2] = 0x00;
        zf_info_arr[0].sram_addr[1] = 0x8C;
        zf_info_arr[0].sram_addr[0] = 0xE0;

        TPD_INFO("fw_addr = 0x%08X, write_size = 0x%08X\n", zf_info_arr[0].fw_addr,
                 zf_info_arr[0].write_size);

        /*FW didn't offer the CRC for now*/
        do {
            g_core_fp.fp_write_sram_0f(fw_entry, zf_info_arr[0].sram_addr,
                                       zf_info_arr[0].fw_addr, zf_info_arr[0].write_size);
            crc = himax_hw_check_CRC(zf_info_arr[0].sram_addr, zf_info_arr[0].write_size);
            if (crc == 0) {
                TPD_DETAIL("%s, HW CRC ok in %d time \n", __func__, retry);
                break;
            } else {
                TPD_INFO("%s, HW CRC fail in %d time !\n", __func__, retry);
            }
            retry++;
        } while ((crc != 0 && retry < 10));
        }

        TPD_DETAIL("Now sram_min[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
                   sram_min[0], sram_min[1], sram_min[2], sram_min[3]);
        retry = 3;
        do {
            /*
            g_core_fp.fp_register_write(sram_min, cfg_sz, FW_buf, 0);
            cfg_crc_hw = g_core_fp.fp_check_CRC(sram_min, cfg_sz);
            */
            himax_mcu_register_write(sram_min, cfg_sz, FW_buf, 0);
            cfg_crc_hw = himax_hw_check_CRC(sram_min, cfg_sz);
            if (cfg_crc_hw != cfg_crc_sw)
                TPD_INFO("Config CRC FAIL, HW CRC = %X,SW CRC = %X, retry = %d\n", cfg_crc_hw,
                         cfg_crc_sw, retry);
            else
                TPD_DETAIL("Config CRC Pass\n");

        } while (cfg_crc_hw != cfg_crc_sw && retry-- > 0);
    } else {
        g_core_fp.fp_clean_sram_0f(zf_info_arr[0].sram_addr, zf_info_arr[0].write_size,
                                   2);
    }

END_FUNC:
    kfree(FW_buf);
ALOC_FW_BUF_FAIL:
    kfree(zf_info_arr);
ALOC_ZF_INFO_ARR_FAIL:
    return ret;
}


void himax_mcu_firmware_update_0f(const struct firmware *fw_entry)
{
    int ret = 0;

    struct firmware *request_fw_headfile = NULL;
    const struct firmware *tmp_fw_entry = NULL;
    bool reload = false;

    TPD_DETAIL("%s, Entering \n", __func__);

    if (fw_entry == NULL || reload) {
        TPD_INFO("Get FW from headfile\n");
        request_fw_headfile = kzalloc(sizeof(struct firmware), GFP_KERNEL);
        if (request_fw_headfile == NULL) {
            TPD_INFO("%s kzalloc failed!\n", __func__);
            return;
        }
        if (g_chip_info->p_firmware_headfile->firmware_data) {
            request_fw_headfile->size = g_chip_info->p_firmware_headfile->firmware_size;
            request_fw_headfile->data = g_chip_info->p_firmware_headfile->firmware_data;
            tmp_fw_entry = request_fw_headfile;
            g_chip_info->using_headfile = true;
        } else {
            TPD_INFO("firmware_data is NULL! exit firmware update!\n");
            if (request_fw_headfile != NULL) {
                kfree(request_fw_headfile);
                request_fw_headfile = NULL;
            }
            return;
        }
    } else {
        tmp_fw_entry = fw_entry;
    }

    himax_register_write(pzf_op->addr_system_reset,  4,  pzf_op->data_system_reset,
                         false);

    himax_enter_safe_mode();
    if ((int)tmp_fw_entry->size > HX64K) {
        ret = hx83102d_nf_zf_part_info(tmp_fw_entry);
    } else {
        TPD_INFO("The fw size if small, not support!\n");
        return ;
    }
    /* switch mode*/
    if (G_POWERONOF == 1)
        g_core_fp.fp_write_sram_0f(tmp_fw_entry, pzf_op->data_mode_switch, 0xC30C, 4);
    else
        g_core_fp.fp_clean_sram_0f(pzf_op->data_mode_switch, 4, 2);

    hx83102d_fw_check(private_ts->chip_data, &private_ts->resolution_info,
                      &private_ts->panel_data);

    if (request_fw_headfile != NULL) {
        kfree(request_fw_headfile);
        request_fw_headfile = NULL;
    }
    TPD_DETAIL("%s, END \n", __func__);
}
int hx_0f_op_file_dirly(char *file_name)
{
    int err = NO_ERR;
    const struct firmware *fw_entry = NULL;
    TPD_INFO("%s, Entering \n", __func__);
    TPD_INFO("file name = %s\n", file_name);
    if (private_ts->fw_update_app_support) {
        err = request_firmware_select (&fw_entry, file_name, private_ts->dev);
    } else {
        err = request_firmware (&fw_entry, file_name, private_ts->dev);
    }
    if (err < 0) {
        TPD_INFO("%s, fail in line%d error code=%d,file maybe fail\n", __func__,
                 __LINE__, err);
        return err;
    }

    if (g_f_0f_updat == 1) {
        TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
        release_firmware (fw_entry);
        err = -1;
        return err;
    } else {
        TPD_INFO("%s:Entering Update Flow!\n", __func__);
        g_f_0f_updat = 1;
    }

    hx83102d_enable_interrupt(g_chip_info, false);

    /* trigger reset */
    hx83102d_resetgpio_set(g_chip_info->hw_res, false); /*reset gpio*/
    hx83102d_resetgpio_set(g_chip_info->hw_res, true);

    g_core_fp.fp_firmware_update_0f (fw_entry);
    release_firmware (fw_entry);

    g_f_0f_updat = 0;
    TPD_INFO("%s, END \n", __func__);
    return err;
}
int himax_mcu_0f_operation_dirly(void)
{
    int err = NO_ERR;

    TPD_DETAIL("%s, Entering \n", __func__);
    TPD_DETAIL("file name = %s\n", private_ts->panel_data.fw_name);
    if (g_chip_info->g_fw_entry == NULL) {
        TPD_INFO("Request TP firmware.\n");
        if (private_ts->fw_update_app_support) {
            err = request_firmware_select (&g_chip_info->g_fw_entry,
                                           private_ts->panel_data.fw_name, private_ts->dev);
        } else {
            err = request_firmware (&g_chip_info->g_fw_entry,
                                    private_ts->panel_data.fw_name, private_ts->dev);
        }
        if (err < 0) {
            TPD_INFO("%s, fail in line%d error code=%d, file maybe fail\n", __func__,
                     __LINE__, err);
            if (g_chip_info->g_fw_entry != NULL) {
                release_firmware(g_chip_info->g_fw_entry);
                g_chip_info->g_fw_entry = NULL;
            }
            return err;
        }
    } else {
        TPD_DETAIL("TP firmware has been requested.\n");
    }

    if (g_f_0f_updat == 1) {
        TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
        err = -1;
        return err;
    } else {
        TPD_INFO("%s:Entering Update Flow!\n", __func__);
        g_f_0f_updat = 1;
    }

    g_core_fp.fp_firmware_update_0f (g_chip_info->g_fw_entry);

    g_f_0f_updat = 0;
    TPD_DETAIL("%s, END \n", __func__);
    return err;
}

int himax_mcu_0f_operation_test_dirly(char *fw_name)
{
    int err = NO_ERR;
    const struct firmware *fw_entry = NULL;

    TPD_DETAIL("%s, Entering \n", __func__);
    TPD_DETAIL("file name = %s\n", fw_name);
    TPD_INFO("Request TP firmware.\n");
    err = request_firmware (&fw_entry, fw_name, private_ts->dev);
    if (err < 0) {
        TPD_INFO("%s, fail in line%d error code=%d,file maybe fail\n", __func__,
                 __LINE__, err);
        if (fw_entry != NULL) {
            release_firmware(fw_entry);
            fw_entry = NULL;
        }
        return err;
    }

    hx83102d_enable_interrupt(g_chip_info, false);

    g_core_fp.fp_firmware_update_0f (fw_entry);
	IN_BASELINE_TEST = 0;
    release_firmware (fw_entry);

    TPD_DETAIL("%s, END \n", __func__);
    return err;
}

void himax_mcu_0f_operation(struct work_struct *work)
{
    int err = NO_ERR;
    TPD_INFO("%s, Entering \n", __func__);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
    if (private_ts->boot_mode != RECOVERY_BOOT)
#else
    if (private_ts->boot_mode != MSM_BOOT_MODE__RECOVERY)
#endif
    {
        if (0 == is_oem_unlocked()) {
            TPD_INFO("file name = %s\n", private_ts->panel_data.fw_name);
            if (g_chip_info->g_fw_entry == NULL) {
                TPD_INFO("Request TP firmware.\n");
                if (private_ts->fw_update_app_support) {
                    err = request_firmware_select (&g_chip_info->g_fw_entry,
                                                   private_ts->panel_data.fw_name, private_ts->dev);
                } else {
                    err = request_firmware (&g_chip_info->g_fw_entry,
                                            private_ts->panel_data.fw_name, private_ts->dev);
                }
                if (err < 0) {
                    TPD_INFO("%s, fail in line%d error code=%d, file maybe fail\n", __func__,
                             __LINE__, err);
                    if (g_chip_info->g_fw_entry != NULL) {
                        release_firmware(g_chip_info->g_fw_entry);
                        g_chip_info->g_fw_entry = NULL;
                    }
                    return ;
                }
            }
        }
    } else {
        TPD_INFO("TP firmware has been requested.\n");
    }

    if (g_f_0f_updat == 1) {
        TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
        return ;
    } else {
        TPD_INFO("%s:Entering Update Flow!\n", __func__);
        g_f_0f_updat = 1;
    }

    hx83102d_enable_interrupt(g_chip_info, false);

    /* trigger reset */
    hx83102d_resetgpio_set(g_chip_info->hw_res, false); /*reset gpio*/
    hx83102d_resetgpio_set(g_chip_info->hw_res, true);

    g_core_fp.fp_firmware_update_0f (g_chip_info->g_fw_entry);
    g_core_fp.fp_reload_disable(0);
    msleep (10);
    himax_read_FW_ver();
    msleep (10);
    himax_sense_on(0x00);
    msleep (10);
    TPD_INFO("%s:End \n", __func__);

    hx83102d_enable_interrupt(g_chip_info, true);

    g_f_0f_updat = 0;
    TPD_INFO("%s, END \n", __func__);
    return ;
}

#ifdef HX_0F_DEBUG
void himax_mcu_read_sram_0f(const struct firmware *fw_entry, uint8_t *addr,
                            int start_index, int read_len)
{
    int total_read_times = 0;
    int max_bus_size = MAX_TRANS_SZ;
    int total_size_temp = 0;
    int total_size = 0;
    int address = 0;
    int i = 0, j = 0;
    int not_same = 0;

    uint8_t tmp_addr[4];
    uint8_t *temp_info_data;
    int *not_same_buff;

    TPD_INFO("%s, Entering \n", __func__);

    himax_burst_enable(1);

    total_size = read_len;

    total_size_temp = read_len;

    if (read_len > 2048)
        max_bus_size = 2048;
    else
        max_bus_size = read_len;

    temp_info_data = kzalloc (sizeof (uint8_t) * total_size, GFP_KERNEL);
    not_same_buff = kzalloc (sizeof (int) * total_size, GFP_KERNEL);

    tmp_addr[3] = addr[3];
    tmp_addr[2] = addr[2];
    tmp_addr[1] = addr[1];
    tmp_addr[0] = addr[0];
    TPD_INFO("%s,  read addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
             __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

    TPD_INFO("%s,  total size=%d\n", __func__, total_size);

    himax_burst_enable(1);

    if (total_size % max_bus_size == 0)
        total_read_times = total_size / max_bus_size;
    else
        total_read_times = total_size / max_bus_size + 1;

    for (i = 0; i < (total_read_times); i++) {
        if (total_size_temp >= max_bus_size) {
            himax_register_read(tmp_addr, max_bus_size, &temp_info_data[i * max_bus_size],
                                false);
            total_size_temp = total_size_temp - max_bus_size;
        } else {
            himax_register_read(tmp_addr, total_size_temp % max_bus_size,
                                &temp_info_data[i * max_bus_size], false);
        }

        address = ((i + 1) * max_bus_size);
        tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);
        if (tmp_addr[0] < addr[0])
            tmp_addr[1] = addr[1] + (uint8_t) ((address >> 8) & 0x00FF) + 1;
        else
            tmp_addr[1] = addr[1] + (uint8_t) ((address >> 8) & 0x00FF);

        msleep (10);
    }
    TPD_INFO("%s, READ Start \n", __func__);
    TPD_INFO("%s, start_index = %d \n", __func__, start_index);
    j = start_index;
    for (i = 0; i < read_len; i++, j++) {
        if (fw_entry->data[j] != temp_info_data[i]) {
            not_same++;
            not_same_buff[i] = 1;
        }

        pr_cont("0x%2.2X, ", temp_info_data[i]);

        if (i > 0 && i % 16 == 15) {
            pr_cont("\n");
        }
    }
    TPD_INFO("%s, READ END \n", __func__);
    TPD_INFO("%s, Not Same count=%d\n", __func__, not_same);
    if (not_same != 0) {
        j = start_index;
        for (i = 0; i < read_len; i++, j++) {
            if (not_same_buff[i] == 1) {
                TPD_INFO("bin = [%d] 0x%2.2X\n", i, fw_entry->data[j]);
            }
        }
        for (i = 0; i < read_len; i++, j++) {
            if (not_same_buff[i] == 1) {
                TPD_INFO("sram = [%d] 0x%2.2X \n", i, temp_info_data[i]);
            }
        }
    }
    TPD_INFO("%s, READ END \n", __func__);
    TPD_INFO("%s, Not Same count=%d\n", __func__, not_same);
    TPD_INFO("%s, END \n", __func__);

    kfree(not_same_buff);
    kfree(temp_info_data);
}

void himax_mcu_read_all_sram(uint8_t *addr, int read_len)
{
    int total_read_times = 0;
    int max_bus_size = MAX_RECVS_SZ;
    int total_size_temp = 0;
    int total_size = 0;
    int address = 0;
    int i = 0;
    /*
    struct file *fn;
    struct filename *vts_name;
    */

    uint8_t tmp_addr[4];
    uint8_t *temp_info_data;

    TPD_INFO("%s, Entering \n", __func__);

    himax_burst_enable(1);

    total_size = read_len;

    total_size_temp = read_len;

    temp_info_data = kzalloc(sizeof (uint8_t) * total_size, GFP_KERNEL);


    tmp_addr[3] = addr[3];
    tmp_addr[2] = addr[2];
    tmp_addr[1] = addr[1];
    tmp_addr[0] = addr[0];
    TPD_INFO("%s,  read addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
             __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

    TPD_INFO("%s,  total size=%d\n", __func__, total_size);

    if (total_size % max_bus_size == 0)
        total_read_times = total_size / max_bus_size;
    else
        total_read_times = total_size / max_bus_size + 1;

    for (i = 0; i < (total_read_times); i++) {
        if (total_size_temp >= max_bus_size) {
            himax_register_read(tmp_addr,  max_bus_size,  &temp_info_data[i * max_bus_size],
                                false);
            total_size_temp = total_size_temp - max_bus_size;
        } else {
            himax_register_read(tmp_addr,  total_size_temp % max_bus_size,
                                &temp_info_data[i * max_bus_size],  false);
        }

        address = ((i + 1) * max_bus_size);
        tmp_addr[1] = addr[1] + (uint8_t) ((address >> 8) & 0x00FF);
        tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);

        msleep(10);
    }
    TPD_INFO("%s,  NOW addr tmp_addr[3]=0x%2.2X,  tmp_addr[2]=0x%2.2X,  tmp_addr[1]=0x%2.2X,  tmp_addr[0]=0x%2.2X\n",
             __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

    TPD_INFO("%s, END \n", __func__);

    kfree (temp_info_data);
}

void himax_mcu_firmware_read_0f(const struct firmware *fw_entry, int type)
{
    uint8_t tmp_addr[4] = {0};

    TPD_INFO("%s, Entering \n", __func__);
    if (type == 0) { /* first 48K */
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_sram_start_addr, 0,
                                   HX_48K_SZ);
        g_core_fp.fp_read_all_sram (tmp_addr, 0xC000);
    } else { /*last 16k*/
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_cfg_info, 0xC000, 132);
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_fw_cfg, 0xC0FE, 512);
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_adc_cfg_1, 0xD000, 376);
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_adc_cfg_2, 0xD178, 376);
        g_core_fp.fp_read_sram_0f (fw_entry, pzf_op->data_adc_cfg_3, 0xD000, 376);
        g_core_fp.fp_read_all_sram (pzf_op->data_sram_clean, HX_32K_SZ);
    }
    TPD_INFO("%s, END \n", __func__);
}

void himax_mcu_0f_operation_check(int type)
{
    int err = NO_ERR;
    /* char *firmware_name = "himax.bin"; */


    TPD_INFO("%s, Entering \n", __func__);
    TPD_INFO("file name = %s\n", private_ts->panel_data.fw_name);

    if (g_chip_info->g_fw_entry == NULL) {
        TPD_INFO("Request TP firmware.\n");
        if (private_ts->fw_update_app_support)
            err = request_firmware_select(&g_chip_info->g_fw_entry,
                                          private_ts->panel_data.fw_name, private_ts->dev);
        else
            err = request_firmware(&g_chip_info->g_fw_entry,
                                   private_ts->panel_data.fw_name, private_ts->dev);
        if (err < 0) {
            TPD_INFO("%s, fail in line%d error code=%d\n", __func__, __LINE__, err);
            if (g_chip_info->g_fw_entry != NULL) {
                release_firmware(g_chip_info->g_fw_entry);
                g_chip_info->g_fw_entry = NULL;
            }
            return ;
        }
    } else {
        TPD_INFO("TP firmware has been requested.\n");
    }

    TPD_INFO("first 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
             g_chip_info->g_fw_entry->data[0], g_chip_info->g_fw_entry->data[1],
             g_chip_info->g_fw_entry->data[2], g_chip_info->g_fw_entry->data[3]);
    TPD_INFO("next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
             g_chip_info->g_fw_entry->data[4], g_chip_info->g_fw_entry->data[5],
             g_chip_info->g_fw_entry->data[6], g_chip_info->g_fw_entry->data[7]);
    TPD_INFO("and next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
             g_chip_info->g_fw_entry->data[8], g_chip_info->g_fw_entry->data[9],
             g_chip_info->g_fw_entry->data[10], g_chip_info->g_fw_entry->data[11]);

    g_core_fp.fp_firmware_read_0f(g_chip_info->g_fw_entry, type);
    TPD_INFO("%s, END \n", __func__);
    return ;
}
#endif


int hx_0f_init(void)
{
    pzf_op = kzalloc(sizeof(struct zf_operation), GFP_KERNEL);

    g_core_fp.fp_reload_disable = hx_dis_rload_0f;
    g_core_fp.fp_sys_reset = himax_mcu_sys_reset;
    g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
    g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
    g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
    g_core_fp.fp_0f_operation = himax_mcu_0f_operation;
    g_core_fp.fp_0f_operation_dirly = himax_mcu_0f_operation_dirly;
    g_core_fp.fp_0f_op_file_dirly = hx_0f_op_file_dirly;
#ifdef HX_0F_DEBUG
    g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
    g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
    g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
    g_core_fp.fp_0f_operation_check = himax_mcu_0f_operation_check;
#endif

    himax_in_parse_assign_cmd(zf_addr_dis_flash_reload,
                              pzf_op->addr_dis_flash_reload, sizeof(pzf_op->addr_dis_flash_reload));
    himax_in_parse_assign_cmd(zf_data_dis_flash_reload,
                              pzf_op->data_dis_flash_reload, sizeof(pzf_op->data_dis_flash_reload));
    himax_in_parse_assign_cmd(zf_addr_system_reset, pzf_op->addr_system_reset,
                              sizeof(pzf_op->addr_system_reset));
    himax_in_parse_assign_cmd(zf_data_system_reset, pzf_op->data_system_reset,
                              sizeof(pzf_op->data_system_reset));
    himax_in_parse_assign_cmd(zf_data_sram_start_addr, pzf_op->data_sram_start_addr,
                              sizeof(pzf_op->data_sram_start_addr));
    himax_in_parse_assign_cmd(zf_data_sram_clean, pzf_op->data_sram_clean,
                              sizeof(pzf_op->data_sram_clean));
    himax_in_parse_assign_cmd(zf_data_cfg_info, pzf_op->data_cfg_info,
                              sizeof(pzf_op->data_cfg_info));
    himax_in_parse_assign_cmd(zf_data_fw_cfg_p1, pzf_op->data_fw_cfg_p1,
                              sizeof(pzf_op->data_fw_cfg_p1));
    himax_in_parse_assign_cmd(zf_data_fw_cfg_p2, pzf_op->data_fw_cfg_p2,
                              sizeof(pzf_op->data_fw_cfg_p2));
    himax_in_parse_assign_cmd(zf_data_fw_cfg_p3, pzf_op->data_fw_cfg_p3,
                              sizeof(pzf_op->data_fw_cfg_p3));
    himax_in_parse_assign_cmd(zf_data_adc_cfg_1, pzf_op->data_adc_cfg_1,
                              sizeof(pzf_op->data_adc_cfg_1));
    himax_in_parse_assign_cmd(zf_data_adc_cfg_2, pzf_op->data_adc_cfg_2,
                              sizeof(pzf_op->data_adc_cfg_2));
    himax_in_parse_assign_cmd(zf_data_adc_cfg_3, pzf_op->data_adc_cfg_3,
                              sizeof(pzf_op->data_adc_cfg_3));
    himax_in_parse_assign_cmd(zf_data_map_table, pzf_op->data_map_table,
                              sizeof(pzf_op->data_map_table));
    himax_in_parse_assign_cmd(zf_data_mode_switch, pzf_op->data_mode_switch,
                              sizeof(pzf_op->data_mode_switch));
    himax_in_parse_assign_cmd(0x800204f4, pzf_op->addr_raw_out_sel,
                              sizeof(pzf_op->addr_raw_out_sel));

    return 0;
}


bool himax_ic_package_check(void)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    hx83102d_resetgpio_set(g_chip_info->hw_res, true); /*reset gpio*/
    hx83102d_resetgpio_set(g_chip_info->hw_res, false);
    hx83102d_resetgpio_set(g_chip_info->hw_res, true);
    msleep(5);

    himax_enter_safe_mode();

    /*
    Product ID
    Touch
    */
    tmp_addr[3] = 0x90;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0xD0;
    himax_register_read(tmp_addr, 4, tmp_data, false);

    TPD_INFO("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],
             tmp_data[2], tmp_data[1]);

    hx_0f_init();

    TPD_INFO("Himax IC package 83112_in\n");


    return true;
}


void himax_power_on_init(void)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    TPD_INFO("%s\n", __func__);

    /*RawOut select initial*/
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x02;
    tmp_addr[1] = 0x04;
    tmp_addr[0] = 0xF4;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_register_write(tmp_addr, 4, tmp_data, false);

    /*DSRAM func initial*/
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x07;
    tmp_addr[0] = 0xFC;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_register_write(tmp_addr, 4, tmp_data, false);

    himax_sense_on(0x00);
}


static void himax_read_FW_ver(void)
{
    uint8_t cmd[4];
    uint8_t data[64];
    uint8_t data2[64];
    int retry = 200;
    int reload_status = 0;

    himax_sense_on(0);

    while (reload_status == 0) {
        cmd[3] = 0x10;/*oppo fw id bin address : 0xc014   , 49172    Tp ic address : 0x 10007014*/
        cmd[2] = 0x00;
        cmd[1] = 0x7f;
        cmd[0] = 0x00;
        himax_register_read(cmd, 4, data, false);

        cmd[3] = 0x10;
        cmd[2] = 0x00;
        cmd[1] = 0x72;
        cmd[0] = 0xc0;
        himax_register_read(cmd, 4, data2, false);

        if ((data[1] == 0x3A && data[0] == 0xA3) || (data2[1] == 0x72
                && data2[0] == 0xc0)) {
            TPD_INFO("reload OK! \n");
            reload_status = 1;
            break;
        } else if (retry == 0) {
            TPD_INFO("reload 20 times! fail \n");
            return;
        } else {
            retry--;
            TPD_INFO("reload fail,delay 10ms retry=%d\n", retry);
        }
    }
    TPD_INFO("%s : data[0]=0x%2.2X, data[1]=0x%2.2X, data[2]=0x%2.2X, data[3]=0x%2.2X\n",
             __func__, data[0], data[1], data[2], data[3]);
    TPD_INFO("reload_status=%d\n", reload_status);

    himax_sense_off();

    /*
    =====================================
    Read FW version : 0x1000_7004  but 05,06 are the real addr for FW Version
    =====================================
    */
    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x04;
    himax_register_read(cmd, 4, data, false);


    TPD_INFO("PANEL_VER : %X \n", data[0]);
    TPD_INFO("FW_VER : %X \n", data[1] << 8 | data[2]);

    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x84;
    himax_register_read(cmd, 4, data, false);

    TPD_INFO("CFG_VER : %X \n", data[2] << 8 | data[3]);
    TPD_INFO("TOUCH_VER : %X \n", data[2]);
    TPD_INFO("DISPLAY_VER : %X \n", data[3]);

    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x00;
    himax_register_read(cmd, 4, data, false);

    TPD_INFO("CID_VER : %X \n", (data[2] << 8 | data[3]));
    return;
}


void himax_read_OPPO_FW_ver(void)
{
    uint8_t cmd[4];
    uint8_t data[4];
    uint32_t touch_ver = 0;
    /*oppo fw id bin address : 0xc014 Tp ic address : 0x 10007014*/
    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x14;
    himax_register_read(cmd, 4, data, false);
    TPD_INFO("%s : data[0]=0x%2.2X, data[1]=0x%2.2X, data[2]=0x%2.2X, data[3]=0x%2.2X\n",
             __func__, data[0], data[1], data[2], data[3]);
    /*oppo fw id bin address : 0xc014 Tp ic address : 0x 10007014*/
    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x84;
    himax_register_read(cmd, 4, data, false);
    touch_ver = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    TPD_INFO("%s :touch_ver=0x%08X\n", __func__, touch_ver);
    return;
}

uint32_t himax_hw_check_CRC(uint8_t *start_addr, int reload_length)
{
    uint32_t result = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int cnt = 0;
    int length = reload_length / 4;

    /*CRC4 // 0x8005_0020 <= from, 0x8005_0028 <= 0x0099_length*/
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x20;
    /*tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0xFB; tmp_data[0] = 0x00;*/
    himax_flash_write_burst(tmp_addr, start_addr);

    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x28;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x99;
    tmp_data[1] = (length >> 8);
    tmp_data[0] = length;
    himax_flash_write_burst(tmp_addr, tmp_data);

    cnt = 0;
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    do {
        himax_register_read(tmp_addr, 4, tmp_data, false);

        if ((tmp_data[0] & 0x01) != 0x01) {
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x05;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x18;
            himax_register_read(tmp_addr, 4, tmp_data, false);
            TPD_DETAIL("%s: tmp_data[3]=%X, tmp_data[2]=%X, tmp_data[1]=%X, tmp_data[0]=%X  \n",
                       __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
            result = ((tmp_data[3] << 24) + (tmp_data[2] << 16) + (tmp_data[1] << 8) +
                      tmp_data[0]);
            break;
        }
    } while (cnt++ < 100);

    return result;
}

int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max)
{
    int RawDataLen;
    if (raw_cnt_rmd != 0x00)
        RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
    else
        RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;
    return RawDataLen;
}


int himax_report_data_init(int max_touch_point, int tx_num, int rx_num)
{
    if (hx_touch_data->hx_coord_buf != NULL)
        kfree(hx_touch_data->hx_coord_buf);
    if (hx_touch_data->diag_mutual != NULL)
        kfree(hx_touch_data->diag_mutual);
    hx_touch_data->event_size = 128;

    /*himax_get_touch_data_size();*/
    hx_touch_data->touch_all_size = 128;

    HX_TOUCH_INFO_POINT_CNT = max_touch_point * 4 ;

    if ((max_touch_point % 4) == 0)
        HX_TOUCH_INFO_POINT_CNT += (max_touch_point / 4) * 4 ;
    else
        HX_TOUCH_INFO_POINT_CNT += ((max_touch_point / 4) + 1) * 4 ;

    hx_touch_data->raw_cnt_max = max_touch_point / 4;
    hx_touch_data->raw_cnt_rmd = max_touch_point % 4;
    /*more than 4 fingers*/
    if (hx_touch_data->raw_cnt_rmd != 0x00) {
        hx_touch_data->rawdata_size = cal_data_len(hx_touch_data->raw_cnt_rmd,
                                      max_touch_point, hx_touch_data->raw_cnt_max);
        hx_touch_data->touch_info_size = (max_touch_point + hx_touch_data->raw_cnt_max +
                                          2) * 4;
        /*less than 4 fingers*/
    } else {
        hx_touch_data->rawdata_size = cal_data_len(hx_touch_data->raw_cnt_rmd,
                                      max_touch_point, hx_touch_data->raw_cnt_max);
        hx_touch_data->touch_info_size = (max_touch_point + hx_touch_data->raw_cnt_max +
                                          1) * 4;
    }

    if ((tx_num * rx_num + tx_num + rx_num) % hx_touch_data->rawdata_size == 0)
        hx_touch_data->rawdata_frame_size = (tx_num * rx_num + tx_num + rx_num) /
                                            hx_touch_data->rawdata_size;
    else
        hx_touch_data->rawdata_frame_size = (tx_num * rx_num + tx_num + rx_num) /
                                            hx_touch_data->rawdata_size + 1;
    TPD_INFO("%s: rawdata_frame_size = %d ", __func__,
             hx_touch_data->rawdata_frame_size);
    TPD_INFO("%s: max_touch_point:%d, hx_raw_cnt_max:%d, hx_raw_cnt_rmd:%d, g_hx_rawdata_size:%d, hx_touch_data->touch_info_size:%d\n",
             __func__, max_touch_point, hx_touch_data->raw_cnt_max,
             hx_touch_data->raw_cnt_rmd, hx_touch_data->rawdata_size,
             hx_touch_data->touch_info_size);

    hx_touch_data->hx_coord_buf = kzalloc(sizeof(uint8_t) *
                                          (hx_touch_data->touch_info_size), GFP_KERNEL);
    if (hx_touch_data->hx_coord_buf == NULL)
        goto mem_alloc_fail;

    hx_touch_data->diag_mutual = kzalloc(tx_num * rx_num * sizeof(int32_t),
                                         GFP_KERNEL);
    if (hx_touch_data->diag_mutual == NULL)
        goto mem_alloc_fail;

    hx_touch_data->hx_rawdata_buf = kzalloc(sizeof(uint8_t) *
                                            (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size), GFP_KERNEL);
    if (hx_touch_data->hx_rawdata_buf == NULL)
        goto mem_alloc_fail;

    hx_touch_data->hx_event_buf = kzalloc(sizeof(uint8_t) *
                                          (hx_touch_data->event_size), GFP_KERNEL);
    if (hx_touch_data->hx_event_buf == NULL)
        goto mem_alloc_fail;

    return NO_ERR;

mem_alloc_fail:
    kfree(hx_touch_data->hx_coord_buf);
    kfree(hx_touch_data->hx_rawdata_buf);
    kfree(hx_touch_data->hx_event_buf);

    TPD_INFO("%s: Memory allocate fail!\n", __func__);
    return MEM_ALLOC_FAIL;

}

bool himax_read_event_stack(uint8_t *buf, uint8_t length)
{
    uint8_t cmd[4];

    /*AHB_I2C Burst Read Off*/
    cmd[0] = 0x00;
    if (himax_bus_write(0x11, 1, cmd) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return 0;
    }

    himax_bus_read(0x30, length, buf);

    /*AHB_I2C Burst Read On*/
    cmd[0] = 0x01;
    if (himax_bus_write(0x11, 1, cmd) < 0) {
        TPD_INFO("%s: i2c access fail!\n", __func__);
        return 0;
    }
    return 1;
}

int himax_ic_esd_recovery(int hx_esd_event, int hx_zero_event, int length)
{
    if (hx_esd_event == length) {
        g_zero_event_count = 0;
        goto checksum_fail;
    } else if (hx_zero_event == length) {
        g_zero_event_count++;
        TPD_INFO("[HIMAX TP MSG]: ALL Zero event is %d times.\n", g_zero_event_count);
        if (g_zero_event_count > 5) {
            g_zero_event_count = 0;
            TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
            goto checksum_fail;
        }
        goto err_workqueue_out;
    }

checksum_fail:
    return CHECKSUM_FAIL;
err_workqueue_out:
    return WORK_OUT;
}

static int hx83102d_resetgpio_set(struct hw_resource *hw_res, bool on)
{
    int ret = 0;
    if (gpio_is_valid(hw_res->reset_gpio)) {
        TPD_DETAIL("Set the reset_gpio on=%d \n", on);
        ret = gpio_direction_output(hw_res->reset_gpio, on);
        if (ret)
            TPD_INFO("Set the reset_gpio on=%d fail\n", on);
        else
            HX_RESET_STATE = on;

        msleep(RESET_TO_NORMAL_TIME);
        TPD_DETAIL("%s hw_res->reset_gpio = %d\n", __func__, hw_res->reset_gpio);
    }

    return ret;
}

void himax_esd_hw_reset(struct chip_data_hx83102d *chip_info)
{
    int ret = 0;
    int load_fw_times = 10;

    TPD_DETAIL("START_Himax TP: ESD - Reset\n");
    HX_ESD_RESET_ACTIVATE = 1;

    hx83102d_enable_interrupt(g_chip_info, false);

    do {
        /*reset gpio*/
        hx83102d_resetgpio_set(chip_info->hw_res, true);
        hx83102d_resetgpio_set(chip_info->hw_res, false);
        hx83102d_resetgpio_set(chip_info->hw_res, true);
        TPD_DETAIL("%s: ESD reset finished\n", __func__);

        TPD_DETAIL("It will update fw after esd event in zero flash mode!\n");

        load_fw_times--;
        g_core_fp.fp_0f_operation_dirly();
        ret = g_core_fp.fp_reload_disable(0);/*success return 1, fail return 0*/
    } while (!ret && load_fw_times > 0);

    if (!load_fw_times)
        TPD_INFO("%s: load_fw_times over 10 times\n", __func__);

    himax_sense_on(0x00);
    /*
    need_modify
    report all leave event
    himax_report_all_leave_event(private_ts);
    */

    hx83102d_enable_interrupt(g_chip_info, true);
}


int himax_checksum_cal(struct chip_data_hx83102d *chip_info, uint8_t *buf,
                       int ts_status)
{
    int hx_EB_event = 0;
    int hx_EC_event = 0;
    int hx_ED_event = 0;
    int hx_esd_event = 0;
    int hx_zero_event = 0;
    int shaking_ret = 0;

    uint16_t check_sum_cal = 0;
    int32_t loop_i = 0;
    int length = 0;

    /* Normal */
    if (ts_status == HX_REPORT_COORD)
        length = hx_touch_data->touch_info_size;

    /* SMWP */
    else if (ts_status == HX_REPORT_SMWP_EVENT)
        length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
    else
        TPD_INFO("%s, Neither Normal Nor SMWP error!\n", __func__);
    /*TPD_INFO("Now status=%d,length=%d\n",ts_status,length);*/
    for (loop_i = 0; loop_i < length; loop_i++) {
        check_sum_cal += buf[loop_i];
        if (ts_status == HX_REPORT_COORD) {
            /* case 1 ESD recovery flow */
            if (buf[loop_i] == 0xEB) {
                hx_EB_event++;
            } else if (buf[loop_i] == 0xEC) {
                hx_EC_event++;
            } else if (buf[loop_i] == 0xED) {
                hx_ED_event++;
            } else if (buf[loop_i] == 0x00) {/* case 2 ESD recovery flow-Disable */
                hx_zero_event++;
            } else {
                hx_EB_event = 0;
                hx_EC_event = 0;
                hx_ED_event = 0;
                hx_zero_event = 0;
                g_zero_event_count = 0;
            }

            if (hx_EB_event == length) {
                hx_esd_event = length;
                hx_EB_event_flag++;
                TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xEB.\n");
            } else if (hx_EC_event == length) {
                hx_esd_event = length;
                hx_EC_event_flag++;
                TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xEC.\n");
            } else if (hx_ED_event == length) {
                hx_esd_event = length;
                hx_ED_event_flag++;
                TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
            } else {
                hx_esd_event = 0;
            }
        }
    }

    if (ts_status == HX_REPORT_COORD) {
        if ((hx_esd_event == length || hx_zero_event == length)
            && (HX_HW_RESET_ACTIVATE == 0) && (HX_ESD_RESET_ACTIVATE == 0)) {
            shaking_ret = himax_ic_esd_recovery(hx_esd_event, hx_zero_event, length);
            if (shaking_ret == CHECKSUM_FAIL) {
                himax_esd_hw_reset(chip_info);
                goto checksum_fail;
            } else if (shaking_ret == ERR_WORK_OUT) {
                goto err_workqueue_out;
            } else {
                /*TPD_INFO("I2C running. Nothing to be done!\n");*/
                goto workqueue_out;
            }
        } else if (HX_ESD_RESET_ACTIVATE) {
            /* drop 1st interrupts after chip reset */
            HX_ESD_RESET_ACTIVATE = 0;
            TPD_INFO("[HX_ESD_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n",
                     __func__);
            goto checksum_fail;
        } else if (HX_HW_RESET_ACTIVATE) {
            /* drop 1st interrupts after chip reset */
            HX_HW_RESET_ACTIVATE = 0;
            TPD_INFO("[HX_HW_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n",
                     __func__);
            goto ready_to_serve;
        }
    }
    if ((check_sum_cal % 0x100 != 0)) {
        TPD_INFO("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n",
                 check_sum_cal);
        goto workqueue_out;
    }

    /* TPD_INFO("%s:End\n",__func__); */
    return NO_ERR;

ready_to_serve:
    return READY_TO_SERVE;
checksum_fail:
    return CHECKSUM_FAIL;
err_workqueue_out:
    return ERR_WORK_OUT;
workqueue_out:
    return WORK_OUT;
}

void himax_log_touch_data(uint8_t *buf, struct himax_report_data *hx_touch_data)
{
    int loop_i = 0;
    int print_size = 0;

    if (!hx_touch_data->diag_cmd) {
        print_size = hx_touch_data->touch_info_size;
    } else {
        print_size = hx_touch_data->touch_all_size;
    }

    for (loop_i = 0; loop_i < print_size; loop_i++) {
        pr_cont("0x%02X ",  buf[loop_i]);
        if ((loop_i + 1) % 8 == 0) {
            pr_cont("\n");
        }
        if (loop_i == (print_size - 1)) {
            pr_cont("\n");
        }
    }
}

void himax_idle_mode(int disable)
{
    int retry = 20;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t switch_cmd = 0x00;

    TPD_INFO("%s:entering\n", __func__);
    do {
        TPD_INFO("%s,now %d times\n!", __func__, retry);

        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x70;
        tmp_addr[0] = 0x88;
        himax_register_read(tmp_addr, 4, tmp_data, false);

        if (disable)
            switch_cmd = 0x17;
        else
            switch_cmd = 0x1F;

        tmp_data[0] = switch_cmd;
        himax_flash_write_burst(tmp_addr, tmp_data);

        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s:After turn ON/OFF IDLE Mode [0] = 0x%02X,[1] = 0x%02X,[2] = 0x%02X,[3] = 0x%02X\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        retry--;
        msleep(10);
    }  while ((tmp_data[0] != switch_cmd) && retry > 0);

    TPD_INFO("%s: setting OK!\n", __func__);
}

int himax_get_rawdata(struct chip_data_hx83102d *chip_info, uint32_t *RAW,
                      uint32_t datalen)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t *tmp_rawdata;
    uint8_t retry = 0;
    uint16_t checksum_cal;
    uint32_t i = 0;

    uint8_t max_i2c_size = MAX_RECVS_SZ;
    int address = 0;
    int total_read_times = 0;
    int total_size = datalen * 2 + 4;
    int total_size_temp;

    uint32_t j = 0;
    uint32_t index = 0;
    uint32_t Min_DATA = 0xFFFFFFFF;
    uint32_t Max_DATA = 0x00000000;

    /* 1 Set Data Ready PWD*/
    while (retry < 200) {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = Data_PWD1;
        tmp_data[0] = Data_PWD0;
        himax_flash_write_burst_length(tmp_addr, tmp_data, 4);

        himax_register_read(tmp_addr, 4, tmp_data, false);
        if ((tmp_data[0] == Data_PWD0 && tmp_data[1] == Data_PWD1) ||
            (tmp_data[0] == Data_PWD1 && tmp_data[1] == Data_PWD0)) {
            break;
        }
        retry++;
        msleep(1);
    }

    if (retry >= 200)
        return RESULT_ERR;
    else
        retry = 0;

    while (retry < 200) {
        if (tmp_data[0] == Data_PWD1 && tmp_data[1] == Data_PWD0) {
            break;
        }

        retry++;
        msleep(1);
        himax_register_read(tmp_addr, 4, tmp_data, false);
    }

    if (retry >= 200)
        return RESULT_ERR;
    else
        retry = 0;

    tmp_rawdata = kzalloc(sizeof(uint8_t) * (datalen * 2), GFP_KERNEL);
    if (!tmp_rawdata) {
        return RESULT_ERR;
    }

    /*2 Read Data from SRAM*/
    while (retry < 10) {
        checksum_cal = 0;
        total_size_temp = total_size;
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;

        if (total_size % max_i2c_size == 0)
            total_read_times = total_size / max_i2c_size;
        else
            total_read_times = total_size / max_i2c_size + 1;

        for (i = 0; i < (total_read_times); i++) {
            if (total_size_temp >= max_i2c_size) {
                himax_register_read(tmp_addr, max_i2c_size, &tmp_rawdata[i * max_i2c_size],
                                    false);
                total_size_temp = total_size_temp - max_i2c_size;
            } else {
                /*TPD_INFO("last total_size_temp=%d\n",total_size_temp);*/
                himax_register_read(tmp_addr, total_size_temp % max_i2c_size,
                                    &tmp_rawdata[i * max_i2c_size], false);
            }

            address = ((i + 1) * max_i2c_size);
            tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
            tmp_addr[0] = (uint8_t)((address) & 0x00FF);
        }

        /*3 Check Checksum*/
        for (i = 2; i < datalen * 2 + 4; i = i + 2)
            checksum_cal += tmp_rawdata[i + 1] * 256 + tmp_rawdata[i];

        if (checksum_cal == 0)
            break;

        retry++;
    }
    if (retry >= 10) {
        TPD_INFO("Retry over 10 times: do recovery\n");
        himax_esd_hw_reset(chip_info);
        return RESULT_RETRY;
    }

    /*4 Copy Data*/
    for (i = 0; i < chip_info->hw_res->RX_NUM * chip_info->hw_res->TX_NUM; i++) {
        RAW[i] = tmp_rawdata[(i * 2) + 1 + 4] * 256 + tmp_rawdata[(i * 2) + 4];
    }

    /*def RAWDATA_DEBUG_PF*/
    for (j = 0; j < chip_info->hw_res->TX_NUM; j++) {
        if (j == 0) {
            pr_cont("      RX%2d", j + 1);
        } else {
            pr_cont("  RX%2d", j + 1);
        }
    }
    pr_cont("\n");

    for (i = 0; i < chip_info->hw_res->RX_NUM; i++) {
        pr_cont("TX%2d", i + 1);
        for (j = 0; j < chip_info->hw_res->TX_NUM; j++) {

            index = ((chip_info->hw_res->RX_NUM * chip_info->hw_res->TX_NUM - i) -
                     chip_info->hw_res->RX_NUM * j) - 1;

            pr_cont("%6d", RAW[index]);

            if (RAW[index] > Max_DATA)
                Max_DATA = RAW[index];

            if (RAW[index] < Min_DATA)
                Min_DATA = RAW[index];
        }
        pr_cont("\n");
    }

    TPD_INFO("Max = %5d, Min = %5d \n", Max_DATA, Min_DATA);

    kfree(tmp_rawdata);

    return RESULT_OK;
}

void himax_switch_data_type(uint8_t checktype)
{
    uint8_t datatype = 0x00;

    switch (checktype) {
    case HIMAX_INSPECTION_OPEN:
        datatype = DATA_OPEN;
        break;
    case HIMAX_INSPECTION_MICRO_OPEN:
        datatype = DATA_MICRO_OPEN;
        break;
    case HIMAX_INSPECTION_SHORT:
        datatype = DATA_SHORT;
        break;
    case HIMAX_INSPECTION_RAWDATA:
        datatype = DATA_RAWDATA;
        break;
    case HIMAX_INSPECTION_NOISE:
        datatype = DATA_NOISE;
        break;
    case HIMAX_INSPECTION_BACK_NORMAL:
        datatype = DATA_BACK_NORMAL;
        break;
    case HIMAX_INSPECTION_LPWUG_RAWDATA:
        datatype = DATA_LPWUG_RAWDATA;
        break;
    case HIMAX_INSPECTION_LPWUG_NOISE:
        datatype = DATA_LPWUG_NOISE;
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
        datatype = DATA_DOZE_RAWDATA;
        break;
    case HIMAX_INSPECTION_DOZE_NOISE:
        datatype = DATA_DOZE_NOISE;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
        datatype = DATA_LPWUG_IDLE_RAWDATA;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        datatype = DATA_LPWUG_IDLE_NOISE;
        break;
    default:
        TPD_INFO("Wrong type=%d\n", checktype);
        break;
    }
    himax_diag_register_set(datatype);
}

int himax_switch_mode(int mode)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    TPD_INFO("%s: Entering\n", __func__);

    /*Stop Handshaking*/
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_flash_write_burst_length(tmp_addr, tmp_data, 4);

    /*Swtich Mode*/
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x7F;
    tmp_addr[0] = 0x04;
    switch (mode) {
    case HIMAX_INSPECTION_SORTING:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_SORTING_START;
        tmp_data[0] = PWD_SORTING_START;
        break;
    case HIMAX_INSPECTION_OPEN:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_OPEN_START;
        tmp_data[0] = PWD_OPEN_START;
        break;
    case HIMAX_INSPECTION_MICRO_OPEN:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_OPEN_START;
        tmp_data[0] = PWD_OPEN_START;
        break;
    case HIMAX_INSPECTION_SHORT:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_SHORT_START;
        tmp_data[0] = PWD_SHORT_START;
        break;
    case HIMAX_INSPECTION_RAWDATA:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_RAWDATA_START;
        tmp_data[0] = PWD_RAWDATA_START;
        break;
    case HIMAX_INSPECTION_NOISE:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_NOISE_START;
        tmp_data[0] = PWD_NOISE_START;
        break;
    case HIMAX_INSPECTION_LPWUG_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_NOISE:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_LPWUG_START;
        tmp_data[0] = PWD_LPWUG_START;
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
    case HIMAX_INSPECTION_DOZE_NOISE:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_DOZE_START;
        tmp_data[0] = PWD_DOZE_START;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = PWD_LPWUG_IDLE_START;
        tmp_data[0] = PWD_LPWUG_IDLE_START;
        break;
    }
    himax_flash_write_burst_length(tmp_addr, tmp_data, 4);

    TPD_INFO("%s: End of setting!\n", __func__);

    return 0;

}


void himax_set_N_frame(uint16_t Nframe, uint8_t checktype)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x72;
    tmp_addr[0] = 0x94;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = (uint8_t)((Nframe & 0xFF00) >> 8);
    tmp_data[0] = (uint8_t)(Nframe & 0x00FF);
    himax_flash_write_burst_length(tmp_addr, tmp_data, 4);

    /*SKIP FRMAE*/
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x70;
    tmp_addr[0] = 0xF4;
    himax_register_read(tmp_addr, 4, tmp_data, false);

    switch (checktype) {
    case HIMAX_INSPECTION_LPWUG_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_NOISE:
        tmp_data[0] = BS_LPWUG;
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
    case HIMAX_INSPECTION_DOZE_NOISE:
        tmp_data[0] = BS_DOZE;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        tmp_data[0] = BS_LPWUG_dile;
        break;
    case HIMAX_INSPECTION_RAWDATA:
    case HIMAX_INSPECTION_NOISE:
        tmp_data[0] = BS_RAWDATANOISE;
        break;
    default:
        tmp_data[0] = BS_OPENSHORT;
        break;
    }
    himax_flash_write_burst_length(tmp_addr, tmp_data, 4);
}


uint32_t himax_check_mode(uint8_t checktype)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t wait_pwd[2];

    wait_pwd[0] = PWD_NONE;
    wait_pwd[1] = PWD_NONE;

    switch (checktype) {
    case HIMAX_INSPECTION_OPEN:
        wait_pwd[0] = PWD_OPEN_END;
        wait_pwd[1] = PWD_OPEN_END;
        break;
    case HIMAX_INSPECTION_MICRO_OPEN:
        wait_pwd[0] = PWD_OPEN_END;
        wait_pwd[1] = PWD_OPEN_END;
        break;
    case HIMAX_INSPECTION_SHORT:
        wait_pwd[0] = PWD_SHORT_END;
        wait_pwd[1] = PWD_SHORT_END;
        break;
    case HIMAX_INSPECTION_RAWDATA:
        wait_pwd[0] = PWD_RAWDATA_END;
        wait_pwd[1] = PWD_RAWDATA_END;
        break;
    case HIMAX_INSPECTION_NOISE:
        wait_pwd[0] = PWD_NOISE_END;
        wait_pwd[1] = PWD_NOISE_END;
        break;
    case HIMAX_INSPECTION_LPWUG_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_NOISE:
        wait_pwd[0] = PWD_LPWUG_END;
        wait_pwd[1] = PWD_LPWUG_END;
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
    case HIMAX_INSPECTION_DOZE_NOISE:
        wait_pwd[0] = PWD_DOZE_END;
        wait_pwd[1] = PWD_DOZE_END;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        wait_pwd[0] = PWD_LPWUG_IDLE_END;
        wait_pwd[1] = PWD_LPWUG_IDLE_END;
        break;
    default:
        TPD_INFO("Wrong type=%d\n", checktype);
        break;
    }

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x7F;
    tmp_addr[0] = 0x04;
    himax_register_read(tmp_addr, 4, tmp_data, false);
    TPD_INFO("%s: himax_wait_sorting_mode, tmp_data[0]=%x, tmp_data[1]=%x\n",
             __func__, tmp_data[0], tmp_data[1]);

    if (wait_pwd[0] == tmp_data[0] && wait_pwd[1] == tmp_data[1]) {
        TPD_INFO("Change to mode=%s\n", g_himax_inspection_mode[checktype]);
        return 0;
    } else
        return 1;
}

void himax_get_noise_base(void)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t ratio, threshold, threshold_LPWUG;

    /*
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x70;
    tmp_addr[0] = 0x8C;
    */
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x70;
    tmp_addr[0] = 0x94; /*ratio*/
    himax_register_read(tmp_addr, 4, tmp_data, false);
    ratio = tmp_data[1];

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x70;
    tmp_addr[0] = 0xA0; /*threshold_LPWUG*/
    himax_register_read(tmp_addr, 4, tmp_data, false);
    threshold_LPWUG = tmp_data[0];

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x70;
    tmp_addr[0] = 0x9C; /*threshold*/
    himax_register_read(tmp_addr, 4, tmp_data, false);
    threshold = tmp_data[0];

    /*
    TPD_INFO("tmp_data[0]=0x%x tmp_data[1]=0x%x tmp_data[2]=0x%x tmp_data[3]=0x%x\n",
          tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
    */
    /*NOISEMAX = tmp_data[3]*(NOISE_P/256);*/
    NOISEMAX = ratio * threshold;
    LPWUG_NOISEMAX = ratio * threshold_LPWUG;
    TPD_INFO("NOISEMAX=%d LPWUG_NOISE_MAX=%d \n", NOISEMAX, LPWUG_NOISEMAX);
}

uint16_t himax_get_noise_weight(void)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint16_t weight;

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x72;
    tmp_addr[0] = 0xC8;
    himax_register_read(tmp_addr, 4, tmp_data, false);
    weight = (tmp_data[1] << 8) | tmp_data[0];
    TPD_INFO("%s: weight = %d ", __func__, weight);

    return weight;
}

uint32_t himax_wait_sorting_mode(uint8_t checktype)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t wait_pwd[2];
    uint8_t count = 0;

    wait_pwd[0] = PWD_NONE;
    wait_pwd[1] = PWD_NONE;

    switch (checktype) {
    case HIMAX_INSPECTION_OPEN:
        wait_pwd[0] = PWD_OPEN_END;
        wait_pwd[1] = PWD_OPEN_END;
        break;
    case HIMAX_INSPECTION_MICRO_OPEN:
        wait_pwd[0] = PWD_OPEN_END;
        wait_pwd[1] = PWD_OPEN_END;
        break;
    case HIMAX_INSPECTION_SHORT:
        wait_pwd[0] = PWD_SHORT_END;
        wait_pwd[1] = PWD_SHORT_END;
        break;
    case HIMAX_INSPECTION_RAWDATA:
        wait_pwd[0] = PWD_RAWDATA_END;
        wait_pwd[1] = PWD_RAWDATA_END;
        break;
    case HIMAX_INSPECTION_NOISE:
        wait_pwd[0] = PWD_NOISE_END;
        wait_pwd[1] = PWD_NOISE_END;
        break;
    case HIMAX_INSPECTION_LPWUG_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_NOISE:
        wait_pwd[0] = PWD_LPWUG_END;
        wait_pwd[1] = PWD_LPWUG_END;
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
    case HIMAX_INSPECTION_DOZE_NOISE:
        wait_pwd[0] = PWD_DOZE_END;
        wait_pwd[1] = PWD_DOZE_END;
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        wait_pwd[0] = PWD_LPWUG_IDLE_END;
        wait_pwd[1] = PWD_LPWUG_IDLE_END;
        break;
    default:
        TPD_INFO("Wrong type=%d\n", checktype);
        break;
    }

    do {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x04;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: himax_wait_sorting_mode, tmp_data[0]=%x, tmp_data[1]=%x\n",
                 __func__, tmp_data[0], tmp_data[1]);

        if (wait_pwd[0] == tmp_data[0] && wait_pwd[1] == tmp_data[1]) {
            return 0;
        }
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 0x900000A8, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xE4;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 0x900000E4, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xF8;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 0x900000F8, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
        TPD_INFO("Now retry %d times!\n", count++);
        msleep(50);
    } while (count < 50);

    return 1;
}

int himax_check_notch(int index)
{
    int tx = g_chip_info->hw_res->TX_NUM;
    int rx = g_chip_info->hw_res->RX_NUM;
    if (((tx - 4) / 2 <= index / 36 && index / 36 <= (tx - 4) / 2 + 3)
        && (index % 36 >= rx - 1)) {
        return 1;
    } else {
        return 0;
    }
}

int mpTestFunc(struct chip_data_hx83102d *chip_info, uint8_t checktype,
               uint32_t datalen)
{
    uint8_t tmp_addr[4] = {0};
    uint8_t tmp_data[4] = {0};

    uint32_t i = 0;
    uint16_t weight = 0;
    uint32_t RAW[datalen];

    int ret = 0;
    if (himax_check_mode(checktype)) {
        TPD_INFO("Need Change Mode, target=%s", g_himax_inspection_mode[checktype]);

        himax_sense_off_mptest();

        himax_switch_mode(checktype);

        if (checktype == HIMAX_INSPECTION_NOISE) {
            himax_set_N_frame(NOISEFRAME, checktype);
            /*himax_get_noise_base();*/
        } else if (checktype == HIMAX_INSPECTION_DOZE_RAWDATA
                   || checktype == HIMAX_INSPECTION_DOZE_NOISE) {
            TPD_INFO("N frame = %d\n", 10);
            himax_set_N_frame(10, checktype);
        } else if (checktype >= HIMAX_INSPECTION_LPWUG_RAWDATA) {
            TPD_INFO("N frame = %d\n", 1);
            himax_set_N_frame(1, checktype);
        } else {
            himax_set_N_frame(2, checktype);
        }


        himax_sense_on(1);

        ret = himax_wait_sorting_mode(checktype);
        if (ret) {
            TPD_INFO("%s: himax_wait_sorting_mode FAIL\n", __func__);
            return ret;
        }
    }

    himax_switch_data_type(checktype);

    ret = himax_get_rawdata(chip_info, RAW, datalen);
    if (ret) {
        TPD_INFO("%s: himax_get_rawdata FAIL\n", __func__);

        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 900000A8: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X,\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x40;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 10007F40: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X,\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 10000000: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X,\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x04;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 10007F04: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X,\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x02;
        tmp_addr[1] = 0x04;
        tmp_addr[0] = 0xF4;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: 800204B4: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X,\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

        /*900000A8,10007F40,10000000,10007F04,800204B4*/
        return ret;
    }

    /* back to normal */
    himax_switch_data_type(HIMAX_INSPECTION_BACK_NORMAL);

    /*Check Data*/
    switch (checktype) {
    case HIMAX_INSPECTION_OPEN:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > OPENMAX || RAW[i] < OPENMIN) {
                /*
                if (RAW[i]>temp_max)
                temp_max=RAW[i];
                if (RAW[i]<temp_min)
                temp_min=RAW[i];
                TPD_INFO("%s: open test FAIL\n", __func__);
                */
                return RESULT_ERR;
            }
        }
        TPD_INFO("%s: open test PASS\n", __func__);
        break;

    case HIMAX_INSPECTION_MICRO_OPEN:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i)) {
                continue;
            }
            if (RAW[i] > M_OPENMAX || RAW[i] < M_OPENMIN) {
                TPD_INFO("%s: micro open test FAIL\n", __func__);
                return RESULT_ERR;
            }
        }
        TPD_INFO("M_OPENMAX = %d,M_OPENMIN = %d\n", M_OPENMAX, M_OPENMIN);
        TPD_INFO("%s: micro open test PASS\n", __func__);
        break;

    case HIMAX_INSPECTION_SHORT:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > SHORTMAX || RAW[i] < SHORTMIN) {
                TPD_INFO("%s: short test FAIL\n", __func__);
                return RESULT_ERR;
            }
        }
        TPD_INFO("%s: short test PASS\n", __func__);
        break;

    case HIMAX_INSPECTION_RAWDATA:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i)) {
                continue;
            }
            if (RAW[i] > RAWMAX || RAW[i] < RAWMIN) {
                TPD_INFO("%s: rawdata test FAIL\n", __func__);
                return RESULT_ERR;
            }
        }
        TPD_INFO("%s: rawdata test PASS\n", __func__);
        break;

    case HIMAX_INSPECTION_NOISE:
        /*
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
        		if (himax_check_notch(i)) {
        		continue;
        	}
        	if (RAW[i] > NOISEMAX) {
        		TPD_INFO("%s: noise test FAIL\n", __func__);
        		return RESULT_ERR;
        	}
        }
        */
        himax_get_noise_base();
        weight = himax_get_noise_weight();
        if (weight > NOISEMAX) {
            TPD_INFO("%s: noise test FAIL\n", __func__);
            return RESULT_ERR;
        }
        TPD_INFO("%s: noise test PASS\n", __func__);

        break;

    case HIMAX_INSPECTION_LPWUG_RAWDATA:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i)) {
                continue;
            }
            if (RAW[i] > LPWUG_RAWDATA_MAX || RAW[i] < LPWUG_RAWDATA_MIN) {
                TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_RAWDATA FAIL\n", __func__);
                return THP_AFE_INSPECT_ERAW;
            }
        }
        TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_RAWDATA PASS\n", __func__);
        break;
    case HIMAX_INSPECTION_LPWUG_NOISE:
        /*
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
        if (himax_check_notch(i)) {
        continue;
        }
        if (RAW[i] > LPWUG_NOISE_MAX || RAW[i] < LPWUG_NOISE_MIN) {
        TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_NOISE FAIL\n", __func__);
        return THP_AFE_INSPECT_ENOISE;
        }
        }
        */
        himax_get_noise_base();
        weight = himax_get_noise_weight();
        if (weight > LPWUG_NOISEMAX || weight < LPWUG_NOISE_MIN) {
            TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_NOISE FAIL\n", __func__);
            return THP_AFE_INSPECT_ENOISE;
        }
        TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_NOISE PASS\n", __func__);
        break;
    case HIMAX_INSPECTION_DOZE_RAWDATA:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > DOZE_RAWDATA_MAX || RAW[i] < DOZE_RAWDATA_MIN) {
                TPD_INFO("%s: HIMAX_INSPECTION_DOZE_RAWDATA FAIL\n", __func__);
                return THP_AFE_INSPECT_ERAW;
            }
        }
        TPD_INFO("%s: HIMAX_INSPECTION_DOZE_RAWDATA PASS\n", __func__);
        break;
    case HIMAX_INSPECTION_DOZE_NOISE:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > DOZE_NOISE_MAX || RAW[i] < DOZE_NOISE_MIN) {
                TPD_INFO("%s: HIMAX_INSPECTION_DOZE_NOISE FAIL\n", __func__);
                return THP_AFE_INSPECT_ENOISE;
            }
        }
        TPD_INFO("%s: HIMAX_INSPECTION_DOZE_NOISE PASS\n", __func__);
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > LPWUG_IDLE_RAWDATA_MAX || RAW[i] < LPWUG_IDLE_RAWDATA_MIN) {
                TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA FAIL\n", __func__);
                return THP_AFE_INSPECT_ERAW;
            }
        }
        TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA PASS\n", __func__);
        break;
    case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (himax_check_notch(i))
                continue;
            if (RAW[i] > LPWUG_IDLE_NOISE_MAX || RAW[i] < LPWUG_IDLE_NOISE_MIN) {
                TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_NOISE FAIL\n", __func__);
                return THP_AFE_INSPECT_ENOISE;
            }
        }
        TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_NOISE PASS\n", __func__);
        break;

    default:
        TPD_INFO("Wrong type=%d\n", checktype);
        break;
    }

    return RESULT_OK;
}

static void hx83102d_black_screen_test(void *chip_data, char *message)
{
    int error = 0;
    int error_num = 0;
    int retry_cnt = 3;
    char buf[128] = {0};
	/*
    uint8_t mpap_addr[4] = {0xEC, 0x73, 0x00, 0x10};
    uint8_t leave_data[4] = {0x00, 0x00, 0x00, 0x00};
	*/
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;

    TPD_INFO("%s\n", __func__);

    /*6. LPWUG RAWDATA*/
    TPD_INFO("[MP_LPWUG_TEST_RAW]\n");

    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_RAWDATA,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "6. MP_LPWUG_TEST_RAW: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    sprintf(message, "%s\n", buf);
    if (error != 0)
        error_num++;

    /*7. LPWUG NOISE*/
    retry_cnt = 3;
    TPD_INFO("[MP_LPWUG_TEST_NOISE]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_NOISE,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "7. MP_LPWUG_TEST_NOISE: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    sprintf(message, "%s\n", buf);
    if (error != 0)
        error_num++;

    /*8. LPWUG IDLE RAWDATA*/
    retry_cnt = 3;
    TPD_INFO("[MP_LPWUG_IDLE_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "8. MP_LPWUG_IDLE_TEST_RAW: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    sprintf(message, "%s\n", buf);
    if (error != 0)
        error_num++;

    /*9. LPWUG IDLE RAWDATA*/
    retry_cnt = 3;
    TPD_INFO("[MP_LPWUG_IDLE_TEST_NOISE]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_IDLE_NOISE,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "9. MP_LPWUG_IDLE_TEST_NOISE: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    sprintf(message, "%s\n", buf);
    if (error != 0)
        error_num++;

    sprintf(message, "%d errors. %s", error_num,
            error_num ? "" : "All test passed.");
    TPD_INFO("%d errors. %s\n", error_num, error_num ? "" : "All test passed.");
	/*himax_register_write(mpap_addr, 4, leave_data, 0);*/
	IN_SELF_TEST = 0;

}

int himax_chip_self_test(struct seq_file *s,
                         struct chip_data_hx83102d *chip_info)
{
    int error = 0;
    int error_num = 0;
    char buf[128] = {0};
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t back_data[4];
    uint8_t retry_cnt = 3;

    TPD_INFO("%s:Entering\n", __func__);

    /*1. Open Test*/
    TPD_INFO("[MP_OPEN_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_OPEN,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "1. Open Test: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    seq_printf(s, buf);
    if (error != 0)
        error_num++;

    /*2. Micro-Open Test*/
    retry_cnt = 3;
    TPD_INFO("[MP_MICRO_OPEN_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_MICRO_OPEN,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "2. Micro Open Test: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    seq_printf(s, buf);
    if (error != 0)
        error_num++;

    /*3. Short Test*/
    retry_cnt = 3;
    TPD_INFO("[MP_SHORT_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_SHORT,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "3. Short Test: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    seq_printf(s, buf);
    if (error != 0)
        error_num++;

#ifndef RAWDATA_NOISE
    /*4. RawData Test*/
    retry_cnt = 3;
    TPD_INFO("[MP_RAW_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_RAWDATA,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "4. Raw data Test: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    seq_printf(s, buf);
    if (error != 0)
        error_num++;
#endif

    /*5. Noise Test*/
    retry_cnt = 3;
    TPD_INFO("[MP_NOISE_TEST_RAW]\n");
    do {
        error = mpTestFunc(chip_info, HIMAX_INSPECTION_NOISE,
                           (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM) +
                           chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM);
        retry_cnt--;
    } while ((error == RESULT_RETRY) && (retry_cnt > 0));
    snprintf(buf, 128, "5. Noise Test: %s\n", error ? "Error" : "Ok");
    TPD_INFO("%s", buf);
    seq_printf(s, buf);
    if (error != 0)
        error_num++;
    retry_cnt = 0;
    do {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x10;
        tmp_data[3] = 0xA5;
        tmp_data[2] = 0x5A;
        tmp_data[1] = 0xA5;
        tmp_data[0] = 0x5A;
        himax_flash_write_burst(tmp_addr, tmp_data);
        back_data[3] = 0XA5;
        back_data[2] = 0X5A;
        back_data[1] = 0XA5;
        back_data[0] = 0X5A;
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: tmp_data[0]=0x%22X, retry_cnt=%d \n", __func__, tmp_data[0],
                 retry_cnt);
        retry_cnt++;
    } while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
              || tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
             && retry_cnt < HIMAX_REG_RETRY_TIMES);

    TPD_INFO("%s:End", __func__);
    himax_sense_off_mptest();
    himax_switch_mode(HIMAX_INSPECTION_LPWUG_RAWDATA);
    return error_num;
}

static size_t hx83102d_proc_register_read(struct file *file, char *buf,
        size_t len, loff_t *pos)
{
    size_t ret = 0;
    uint16_t loop_i;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
    int max_bus_size = MAX_RECVS_SZ;
    uint8_t data[MAX_RECVS_SZ];
#else
    int max_bus_size = 128;
    uint8_t data[128];
#endif
    char *temp_buf;

    memset(data, 0x00, sizeof(data));

    if (!HX_PROC_SEND_FLAG) {
        temp_buf = kzalloc(len, GFP_KERNEL);

        TPD_INFO("himax_register_show: %02X,%02X,%02X,%02X\n", register_command[3],
                 register_command[2], register_command[1], register_command[0]);

        himax_register_read(register_command, max_bus_size, data, cfg_flag);

        ret += snprintf(temp_buf + ret, len - ret, "command:  %02X,%02X,%02X,%02X\n",
                        register_command[3], register_command[2], register_command[1],
                        register_command[0]);

        for (loop_i = 0; loop_i < max_bus_size; loop_i++) {
            ret += snprintf(temp_buf + ret, len - ret, "0x%2.2X ", data[loop_i]);
            if ((loop_i % 16) == 15) {
                ret += snprintf(temp_buf + ret, len - ret, "\n");
            }
        }
        ret += snprintf(temp_buf + ret, len - ret, "\n");
        if (copy_to_user(buf, temp_buf, len))
            TPD_INFO("%s,here:%d\n", __func__, __LINE__);
        kfree(temp_buf);
        HX_PROC_SEND_FLAG = 1;
    } else {
        HX_PROC_SEND_FLAG = 0;
    }
    return ret;
}

static size_t hx83102d_proc_register_write(struct file *file, const char *buff,
        size_t len, loff_t *pos)
{
    char buf[81] = {0};
    char buf_tmp[16];
    uint8_t length = 0;
    unsigned long result    = 0;
    uint8_t loop_i          = 0;
    uint16_t base           = 2;
    char *data_str = NULL;
    uint8_t w_data[20];
    uint8_t x_pos[20];
    uint8_t count = 0;

    if (len >= 80) {
        TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
        return -EFAULT;
    }

    if (copy_from_user(buf, buff, len)) {
        return -EFAULT;
    }
    buf[len] = '\0';

    memset(buf_tmp, 0x0, sizeof(buf_tmp));
    memset(w_data, 0x0, sizeof(w_data));
    memset(x_pos, 0x0, sizeof(x_pos));

    TPD_INFO("himax %s \n", buf);

    if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' && buf[2] == 'x') {
        length = strlen(buf);

        /*TPD_INFO("%s: length = %d.\n", __func__,length);*/
        for (loop_i = 0; loop_i < length; loop_i++) {/*find postion of 'x'*/
            if (buf[loop_i] == 'x') {
                x_pos[count] = loop_i;
                count++;
            }
        }

        data_str = strrchr(buf, 'x');
        TPD_INFO("%s: %s.\n", __func__, data_str);
        length = strlen(data_str + 1) - 1;

        if (buf[0] == 'r') {
            if (buf[3] == 'F' && buf[4] == 'E' && length == 4) {
                length = length - base;
                cfg_flag = true;
                memcpy(buf_tmp, data_str + base + 1, length);
            } else {
                cfg_flag = false;
                memcpy(buf_tmp, data_str + 1, length);
            }

            byte_length = length / 2;
            if (!kstrtoul(buf_tmp, 16, &result)) {
                for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
                    register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
            }
        } else if (buf[0] == 'w') {
            if (buf[3] == 'F' && buf[4] == 'E') {
                cfg_flag = true;
                memcpy(buf_tmp, buf + base + 3, length);
            } else {
                cfg_flag = false;
                memcpy(buf_tmp, buf + 3, length);
            }
            if (count < 3) {
                byte_length = length / 2;
                if (!kstrtoul(buf_tmp, 16, &result)) {
                    for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
                        register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
                }
                if (!kstrtoul(data_str + 1, 16, &result)) {
                    for (loop_i = 0 ; loop_i < byte_length ; loop_i++)
                        w_data[loop_i] = (uint8_t)(result >> loop_i * 8);
                }
                himax_register_write(register_command, byte_length, w_data, cfg_flag);
            } else {
                byte_length = x_pos[1] - x_pos[0] - 2;
                /*parsing addr after 'x'*/
                for (loop_i = 0; loop_i < count; loop_i++) {
                    memcpy(buf_tmp, buf + x_pos[loop_i] + 1, byte_length);
                    /*TPD_INFO("%s: buf_tmp = %s\n", __func__,buf_tmp);*/
                    if (!kstrtoul(buf_tmp, 16, &result)) {
                        if (loop_i == 0) {
                            register_command[loop_i] = (uint8_t)(result);
                            /*TPD_INFO("%s: register_command = %X\n", __func__,register_command[0]);*/
                        } else {
                            w_data[loop_i - 1] = (uint8_t)(result);
                            /*TPD_INFO("%s: w_data[%d] = %2X\n", __func__,loop_i - 1,w_data[loop_i - 1]);*/
                        }
                    }
                }

                byte_length = count - 1;
                himax_register_write(register_command, byte_length, &w_data[0], cfg_flag);
            }
        } else {
            return len;
        }

    }
    return len;
}

void himax_return_event_stack(void)
{
    int retry = 20;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    TPD_INFO("%s:entering\n", __func__);
    do {
        TPD_INFO("%s,now %d times\n!", __func__, retry);
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x00;
        himax_flash_write_burst(tmp_addr, tmp_data);

        himax_register_read(tmp_addr, 4, tmp_data, false);
        retry--;
        msleep(10);
    } while ((tmp_data[1] != 0x00 && tmp_data[0] != 0x00) && retry > 0);

    TPD_INFO("%s: End of setting!\n", __func__);

}
/*IC_BASED_END*/

int himax_write_read_reg(uint8_t *tmp_addr, uint8_t *tmp_data, uint8_t hb,
                         uint8_t lb)
{
    int cnt = 0;

    do {
        himax_flash_write_burst(tmp_addr, tmp_data);

        msleep(20);
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s:Now tmp_data[0]=0x%02X, [1]=0x%02X, [2]=0x%02X, [3]=0x%02X\n",
                 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
    } while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

    if (cnt >= 99)  {
        TPD_INFO("himax_write_read_reg ERR Now register 0x%08X : high byte=0x%02X, low byte=0x%02X\n",
                 tmp_addr[3], tmp_data[1], tmp_data[0]);
        return -1;
    }

    TPD_INFO("Now register 0x%08X : high byte=0x%02X, low byte=0x%02X\n",
             tmp_addr[3], tmp_data[1], tmp_data[0]);
    return NO_ERR;
}

void himax_get_DSRAM_data(uint8_t *info_data, uint8_t x_num, uint8_t y_num)
{
    int i = 0;
    unsigned char tmp_addr[4];
    unsigned char tmp_data[4];
    uint8_t max_i2c_size = MAX_RECVS_SZ;
    int m_key_num = 0;
    int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
    int total_size_temp;
    int mutual_data_size = x_num * y_num * 2;
    int total_read_times = 0;
    int address = 0;
    uint8_t *temp_info_data; /*max mkey size = 8*/
    uint32_t check_sum_cal = 0;
    int fw_run_flag = -1;

    temp_info_data = kzalloc(sizeof(uint8_t) * (total_size + 8), GFP_KERNEL);

    /*1. Read number of MKey R100070E8H to determin data size*/
    m_key_num = 0;
    /*TPD_INFO("%s,m_key_num=%d\n",__func__,m_key_num);*/
    total_size += m_key_num * 2;

    /* 2. Start DSRAM Rawdata and Wait Data Ready */
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x5A;
    tmp_data[0] = 0xA5;
    fw_run_flag = himax_write_read_reg(tmp_addr, tmp_data, 0xA5, 0x5A);
    if (fw_run_flag < 0) {
        TPD_INFO("%s Data NOT ready => bypass \n", __func__);
        kfree(temp_info_data);
        return;
    }

    /* 3. Read RawData */
    total_size_temp = total_size;
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;

    if (total_size % max_i2c_size == 0)
        total_read_times = total_size / max_i2c_size;
    else
        total_read_times = total_size / max_i2c_size + 1;

    for (i = 0; i < (total_read_times); i++) {
        if (total_size_temp >= max_i2c_size) {
            himax_register_read(tmp_addr, max_i2c_size, &temp_info_data[i * max_i2c_size],
                                false);
            total_size_temp = total_size_temp - max_i2c_size;
        } else {
            /*TPD_INFO("last total_size_temp=%d\n",total_size_temp);*/
            himax_register_read(tmp_addr, total_size_temp % max_i2c_size,
                                &temp_info_data[i * max_i2c_size], false);
        }

        address = ((i + 1) * max_i2c_size);
        tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
        tmp_addr[0] = (uint8_t)((address) & 0x00FF);
    }

    /* 4. FW stop outputing */
    /*TPD_INFO("DSRAM_Flag=%d\n",DSRAM_Flag);*/
    if (DSRAM_Flag == false) {
        /*TPD_INFO("Return to Event Stack!\n");*/
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x00;
        himax_flash_write_burst(tmp_addr, tmp_data);
    } else {
        /*TPD_INFO("Continue to SRAM!\n");*/
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x11;
        tmp_data[2] = 0x22;
        tmp_data[1] = 0x33;
        tmp_data[0] = 0x44;
        himax_flash_write_burst(tmp_addr, tmp_data);
    }

    /* 5. Data Checksum Check */
    for (i = 2; i < total_size; i = i + 2) {
        /* 2:PASSWORD NOT included */
        check_sum_cal += (temp_info_data[i + 1] * 256 + temp_info_data[i]);
    }

    if (check_sum_cal % 0x10000 != 0) {
        memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
        TPD_INFO("%s check_sum_cal fail=%2x \n", __func__, check_sum_cal);
        kfree(temp_info_data);
        return;
    } else {
        memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
        TPD_INFO("%s checksum PASS \n", __func__);
    }
    kfree(temp_info_data);
}

void himax_ts_diag_func(struct chip_data_hx83102d *chip_info,
                        int32_t *mutual_data)
{
    int i = 0;
    int j = 0;
    unsigned int index = 0;
    int total_size = chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM * 2;
    uint8_t info_data[total_size];

    int32_t new_data;
    /* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
    int dsram_type = 0;
    char write_buf[total_size * 3];

    memset(write_buf, '\0', sizeof(write_buf));

    dsram_type = g_diag_command / 10;

    TPD_INFO("%s:Entering g_diag_command=%d\n!", __func__, g_diag_command);

    if (dsram_type == 8) {
        dsram_type = 1;
        TPD_INFO("%s Sorting Mode run sram type1 ! \n", __func__);
    }

    himax_burst_enable(1);
    himax_get_DSRAM_data(info_data, chip_info->hw_res->RX_NUM,
                         chip_info->hw_res->TX_NUM);

    index = 0;
    for (i = 0; i < chip_info->hw_res->TX_NUM; i++) {
        for (j = 0; j < chip_info->hw_res->RX_NUM; j++) {
            new_data = ((int8_t)info_data[index + 1] << 8 | info_data[index]);
            mutual_data[i * chip_info->hw_res->RX_NUM + j] = new_data;
            index += 2;
        }
    }
}

void diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num,
                         int self_num, uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data)
{
    int RawDataLen_word;
    int index = 0;
    int temp1, temp2, i;

    if (hx_touch_data->hx_rawdata_buf[0] == 0x3A
        && hx_touch_data->hx_rawdata_buf[1] == 0xA3
        && hx_touch_data->hx_rawdata_buf[2] > 0
        && hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
        RawDataLen_word = hx_touch_data->rawdata_size / 2;
        index = (hx_touch_data->hx_rawdata_buf[2] - 1) * RawDataLen_word;
        /*TPD_INFO("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);*/
        /*TPD_INFO("RawDataLen=%d , RawDataLen_word=%d , hx_touch_info_size=%d\n", RawDataLen, RawDataLen_word, hx_touch_info_size);*/
        for (i = 0; i < RawDataLen_word; i++) {
            temp1 = index + i;

            if (temp1 < mul_num) {
                /*mutual 4: RawData Header, 1:HSB*/
                mutual_data[index + i] = ((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1])
                                         * 256 + hx_touch_data->hx_rawdata_buf[i * 2 + 4];
            } else {
                /*self*/
                temp1 = i + index;
                temp2 = self_num + mul_num;

                if (temp1 >= temp2)
                    break;
                /*4: RawData Header*/
                self_data[i + index - mul_num] = (((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 +
                                                   4 + 1]) << 8) | hx_touch_data->hx_rawdata_buf[i * 2 + 4];
            }
        }
    }

}
/*return checksum value*/
bool diag_check_sum(struct himax_report_data *hx_touch_data)
{
    uint16_t check_sum_cal = 0;
    int i;

    /*Check 128th byte CRC*/
    for (i = 0, check_sum_cal = 0;
         i < (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
         i = i + 2) {
        check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1] * 256 +
                          hx_touch_data->hx_rawdata_buf[i]);
    }
    if (check_sum_cal % 0x10000 != 0) {
        TPD_INFO("%s fail=%2X \n", __func__, check_sum_cal);
        return 0;
    }

    return 1;
}

static size_t hx83102d_proc_diag_write(struct file *file, const char *buff,
                                       size_t len, loff_t *pos)
{
    char messages[80] = {0};
    uint8_t command[2] = {0x00, 0x00};
    uint8_t receive[1];

    struct touchpanel_data *ts = PDE_DATA(file_inode(file));
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)
                                           ts->chip_data;

    /* 0: common , other: dsram*/
    int storage_type = 0;
    /* 1:IIR,2:DC,3:Bank,4:IIR2,5:IIR2_N,6:FIR2,7:Baseline,8:dump coord */
    int rawdata_type = 0;

    memset(receive, 0x00, sizeof(receive));

    if (len >= 80) {
        TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
        return -EFAULT;
    }
    if (copy_from_user(messages, buff, len)) {
        return -EFAULT;
    }

    if (messages[1] == 0x0A)
        g_diag_command = messages[0] - '0';
    else
        g_diag_command = (messages[0] - '0') * 10 + (messages[1] - '0');

    storage_type = g_diag_command / 10;
    rawdata_type = g_diag_command % 10;

    TPD_INFO(" messages       = %s\n"
             " g_diag_command = 0x%x\n"
             " storage_type   = 0x%x\n"
             " rawdata_type   = 0x%x\n",
             messages, g_diag_command, storage_type, rawdata_type);

    if (g_diag_command > 0 && rawdata_type == 0) {
        TPD_INFO("[Himax]g_diag_command=0x%x, storage_type=%d, rawdata_type=%d! Maybe no support!\n",
                 g_diag_command, storage_type, rawdata_type);
        g_diag_command = 0x00;
    } else {
        TPD_INFO("[Himax]g_diag_command=0x%x, storage_type=%d, rawdata_type=%d\n",
                 g_diag_command, storage_type, rawdata_type);
    }

    if (storage_type == 0 && rawdata_type > 0 && rawdata_type < 8) {
        TPD_INFO("%s,common\n", __func__);
        if (DSRAM_Flag) {
            /*(1) Clear DSRAM flag*/
            DSRAM_Flag = false;
            /*(2) Enable ISR*/
            enable_irq(chip_info->hx_irq);
            /*(3) FW leave sram and return to event stack*/
            himax_return_event_stack();
        }

        command[0] = g_diag_command;
        himax_diag_register_set(command[0]);
    } else if (storage_type > 0 && storage_type < 8 && rawdata_type > 0
               && rawdata_type < 8) {
        TPD_INFO("%s,dsram\n", __func__);

        /*0. set diag flag*/
        if (DSRAM_Flag) {
            /*(1) Clear DSRAM flag*/
            DSRAM_Flag = false;
            /*(2) Enable ISR*/
            enable_irq(chip_info->hx_irq);
            /*(3) FW leave sram and return to event stack*/
            himax_return_event_stack();
        }

        switch (rawdata_type) {
        /*IIR*/
        case 1:
            command[0] = 0x09;
            break;
        /*RAWDATA*/
        case 2:
            command[0] = 0x0A;
            break;
        /*Baseline*/
        case 3:
            command[0] = 0x08;
            break;

        default:
            command[0] = 0x00;
            TPD_INFO("%s: Sram no support this type !\n", __func__);
            break;
        }
        himax_diag_register_set(command[0]);
        TPD_INFO("%s: Start get raw data in DSRAM\n", __func__);
        /*1. Disable ISR*/
        disable_irq(chip_info->hx_irq);

        /*2. Set DSRAM flag*/
        DSRAM_Flag = true;
    } else {
        /*set diag flag*/
        if (DSRAM_Flag) {
            TPD_INFO("return and cancel sram thread!\n");
            /*(1) Clear DSRAM flag*/
            DSRAM_Flag = false;
            himax_return_event_stack();
        }
        command[0] = 0x00;
        g_diag_command = 0x00;
        himax_diag_register_set(command[0]);
        TPD_INFO("return to normal g_diag_command=0x%x\n", g_diag_command);
    }
    return len;
}

static size_t hx83102d_proc_diag_read(struct file *file, char *buff, size_t len,
                                      loff_t *pos)
{
    size_t ret = 0;
    char *temp_buf;
    uint16_t mutual_num;
    uint16_t self_num;
    uint16_t width;
    int dsram_type = 0;
    int data_type = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    struct touchpanel_data *ts = PDE_DATA(file_inode(file));
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)
                                           ts->chip_data;

    if (!HX_PROC_SEND_FLAG) {
        temp_buf = kzalloc(len, GFP_KERNEL);
        if (!temp_buf)
            goto RET_OUT;

        dsram_type = g_diag_command / 10;
        data_type = g_diag_command % 10;

        mutual_num = chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM;
        self_num = chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM;
        width = chip_info->hw_res->RX_NUM;
        ret += snprintf(temp_buf + ret, len - ret,
                        "ChannelStart (rx tx) : %4d, %4d\n\n", chip_info->hw_res->RX_NUM,
                        chip_info->hw_res->TX_NUM);

        /*start to show out the raw data in adb shell*/
        if ((data_type >= 1 && data_type <= 7)) {
            if (dsram_type > 0)
                himax_ts_diag_func(chip_info, hx_touch_data->diag_mutual);

            for (j = 0; j < chip_info->hw_res->RX_NUM ; j++) {
                for (i = 0; i < chip_info->hw_res->TX_NUM; i++) {
                    k = ((mutual_num - j) - chip_info->hw_res->RX_NUM * i) - 1;
                    ret += snprintf(temp_buf + ret, len - ret, "%6d",
                                    hx_touch_data->diag_mutual[k]);
                }
                ret += snprintf(temp_buf + ret, len - ret, " %6d\n", diag_self[j]);
            }

            ret += snprintf(temp_buf + ret, len - ret, "\n");
            for (i = 0; i < chip_info->hw_res->TX_NUM; i++) {
                ret += snprintf(temp_buf + ret, len - ret, "%6d", diag_self[i]);
            }
        }

        ret += snprintf(temp_buf + ret, len - ret, "\n");
        ret += snprintf(temp_buf + ret, len - ret, "ChannelEnd");
        ret += snprintf(temp_buf + ret, len - ret, "\n");

        /* print Mutual/Slef Maximum and Minimum */
        for (i = 0; i < (chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM); i++) {
            if (hx_touch_data->diag_mutual[i] > g_max_mutual)
                g_max_mutual = hx_touch_data->diag_mutual[i];
            if (hx_touch_data->diag_mutual[i] < g_min_mutual)
                g_min_mutual = hx_touch_data->diag_mutual[i];
        }

        /*himax_get_self_edge();*/
        for (i = 0; i < (chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM); i++) {
            if (diag_self[i] > g_max_self)
                g_max_self = diag_self[i];
            if (diag_self[i] < g_min_self)
                g_min_self = diag_self[i];
        }

        ret += snprintf(temp_buf + ret, len - ret, "Mutual Max:%3d, Min:%3d\n",
                        g_max_mutual, g_min_mutual);
        ret += snprintf(temp_buf + ret, len - ret, "Self Max:%3d, Min:%3d\n",
                        g_max_self, g_min_self);

        /* recovery status after print*/
        g_max_mutual = 0;
        g_min_mutual = 0xFFFF;
        g_max_self = 0;
        g_min_self = 0xFFFF;

        if (copy_to_user(buff, temp_buf, len))
            TPD_INFO("%s,here:%d\n", __func__, __LINE__);

        HX_PROC_SEND_FLAG = 1;
RET_OUT:
        if (temp_buf)
            kfree(temp_buf);
    } else {
        HX_PROC_SEND_FLAG = 0;
    }
    return ret;
}

uint8_t himax_read_DD_status(struct chip_data_hx83102d *chip_info,
                             uint8_t *cmd_set, uint8_t *tmp_data)
{
    int cnt = 0;
    uint8_t req_size = cmd_set[0];
    uint8_t cmd_addr[4] = {0xFC, 0x00, 0x00, 0x90};
    uint8_t tmp_addr[4] = {0x80, 0x7F, 0x00, 0x10};

    cmd_set[3] = 0xAA;
    himax_register_write(cmd_addr, 4, cmd_set, 0);

    TPD_INFO("%s: cmd_set[0] = 0x%02X, cmd_set[1] = 0x%02X, cmd_set[2] = 0x%02X, cmd_set[3] = 0x%02X\n",
             __func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

    for (cnt = 0; cnt < 100; cnt++) {
        himax_register_read(cmd_addr, 4, tmp_data, false);
        TPD_INFO("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X, tmp_data[3] = 0x%02X,cnt = % d\n", __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3], cnt);
                       msleep(10);
        if (tmp_data[3] == 0xBB) {
        TPD_INFO("%s Data ready goto moving data\n", __func__);
            break;
        } else if (cnt >= 99) {
        TPD_INFO("%s Data not ready in FW \n", __func__);
            return FW_NOT_READY;
        }
    }
    himax_register_read(tmp_addr, req_size, tmp_data, false);
    return NO_ERR;
}

static size_t hx83102d_proc_DD_debug_read(struct file *file, char *buf,
        size_t len, loff_t *pos)
{
    int ret = 0;
    uint8_t tmp_data[64];
    uint8_t loop_i = 0;
    char *temp_buf;

    struct touchpanel_data *ts = PDE_DATA(file_inode(file));
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)
                                           ts->chip_data;

    if (!HX_PROC_SEND_FLAG) {
        temp_buf = kzalloc(len, GFP_KERNEL);

        if (mutual_set_flag == 1) {
            if (himax_read_DD_status(chip_info, cmd_set, tmp_data) == NO_ERR) {
                for (loop_i = 0; loop_i < cmd_set[0]; loop_i++) {
                    if ((loop_i % 8) == 0)
                        ret += snprintf(temp_buf + ret, len - ret, "0x%02X : ", loop_i);
                    ret += snprintf(temp_buf + ret, len - ret, "0x%02X ", tmp_data[loop_i]);
                    if ((loop_i % 8) == 7)
                        ret += snprintf(temp_buf + ret, len - ret, "\n");
                }
            }
        }
        ret += snprintf(temp_buf + ret, len - ret, "\n");
        if (copy_to_user(buf, temp_buf, len))
            TPD_INFO("%s,here:%d\n", __func__, __LINE__);
        kfree(temp_buf);
        HX_PROC_SEND_FLAG = 1;
    } else
        HX_PROC_SEND_FLAG = 0;
    return ret;
}

static size_t hx83102d_proc_DD_debug_write(struct file *file, const char *buff,
        size_t len, loff_t *pos)
{
    uint8_t i = 0;
    uint8_t cnt = 2;
    unsigned long result = 0;
    char buf_tmp[20];
    char buf_tmp2[4];

    if (len >= 20) {
        TPD_INFO("%s: no command exceeds 20 chars.\n", __func__);
        return -EFAULT;
    }
    if (copy_from_user(buf_tmp, buff, len)) {
        return -EFAULT;
    }
    memset(buf_tmp2, 0x0, sizeof(buf_tmp2));

    if (buf_tmp[2] == 'x' && buf_tmp[6] == 'x' && buf_tmp[10] == 'x') {
        mutual_set_flag = 1;
        for (i = 3; i < 12; i = i + 4) {
            memcpy(buf_tmp2, buf_tmp + i, 2);
            if (!kstrtoul(buf_tmp2, 16, &result))
                cmd_set[cnt] = (uint8_t)result;
            else
                TPD_INFO("String to oul is fail in cnt = %d, buf_tmp2 = %s", cnt, buf_tmp2);
            cnt--;
        }
        TPD_INFO("cmd_set[2] = %02X, cmd_set[1] = %02X, cmd_set[0] = %02X\n",
                 cmd_set[2], cmd_set[1], cmd_set[0]);
    } else
        mutual_set_flag = 0;

    return len;
}

int himax_read_FW_status(struct chip_data_hx83102d *chip_info,
                         uint8_t *state_addr, uint8_t *tmp_addr)
{
    uint8_t req_size = 0;
    /*0x10007F44*/
    uint8_t status_addr[4] = {0x44, 0x7F, 0x00, 0x10};
    /*0x900000F8*/
    uint8_t cmd_addr[4] = {0xF8, 0x00, 0x00, 0x90};

    if (state_addr[0] == 0x01) {
        state_addr[1] = 0x04;
        state_addr[2] = status_addr[0];
        state_addr[3] = status_addr[1];
        state_addr[4] = status_addr[2];
        state_addr[5] = status_addr[3];
        req_size = 0x04;
        himax_sense_off();
        himax_register_read(status_addr, req_size, tmp_addr, false);
        himax_sense_on(1);
    } else if (state_addr[0] == 0x02) {
        state_addr[1] = 0x30;
        state_addr[2] = cmd_addr[0];
        state_addr[3] = cmd_addr[1];
        state_addr[4] = cmd_addr[2];
        state_addr[5] = cmd_addr[3];
        req_size = 0x30;
        himax_register_read(cmd_addr, req_size, tmp_addr, false);
    }

    return NO_ERR;
}

static size_t hx83102d_proc_FW_debug_read(struct file *file, char *buf,
        size_t len, loff_t *pos)
{
    int ret = 0;
    uint8_t loop_i = 0;
    uint8_t tmp_data[64];
    char *temp_buf;

    struct touchpanel_data *ts = PDE_DATA(file_inode(file));
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)
                                           ts->chip_data;

    if (!HX_PROC_SEND_FLAG) {
        temp_buf = kzalloc(len, GFP_KERNEL);

        cmd_set[0] = 0x01;
        if (himax_read_FW_status(chip_info, cmd_set, tmp_data) == NO_ERR) {
            ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t", cmd_set[5],
                            cmd_set[4], cmd_set[3], cmd_set[2]);
            for (loop_i = 0; loop_i < cmd_set[1]; loop_i++)
                ret += snprintf(temp_buf + ret, len - ret, "%5d\t", tmp_data[loop_i]);
            ret += snprintf(temp_buf + ret, len - ret, "\n");
        }
        cmd_set[0] = 0x02;
        if (himax_read_FW_status(chip_info, cmd_set, tmp_data) == NO_ERR) {
            for (loop_i = 0; loop_i < cmd_set[1]; loop_i = loop_i + 2) {
                if ((loop_i % 16) == 0)
                    ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t",
                                    cmd_set[5], cmd_set[4], cmd_set[3] + (((cmd_set[2] + loop_i) >> 8) & 0xFF),
                                    (cmd_set[2] + loop_i) & 0xFF);

                ret += snprintf(temp_buf + ret, len - ret, "%5d\t",
                                tmp_data[loop_i] + (tmp_data[loop_i + 1] << 8));
                if ((loop_i % 16) == 14)
                    ret += snprintf(temp_buf + ret, len - ret, "\n");
            }

        }
        ret += snprintf(temp_buf + ret, len - ret, "\n");
        if (copy_to_user(buf, temp_buf, len))
            TPD_INFO("%s,here:%d\n", __func__, __LINE__);
        kfree(temp_buf);
        HX_PROC_SEND_FLAG = 1;
    } else
        HX_PROC_SEND_FLAG = 0;

    return ret;
}

static int hx83102d_configuration_init(struct chip_data_hx83102d *chip_info,
                                       bool config)
{
    int ret = 0;
    TPD_INFO("%s, configuration init = %d\n", __func__, config);
    return ret;
}

int himax_ic_reset(struct chip_data_hx83102d *chip_info, uint8_t loadconfig,
                   uint8_t int_off)
{
    int ret = 0;
    HX_HW_RESET_ACTIVATE = 1;

    TPD_INFO("%s,status: loadconfig=%d,int_off=%d\n", __func__, loadconfig,
             int_off);

    if (chip_info->hw_res->reset_gpio) {
        if (int_off) {
            ret = hx83102d_enable_interrupt(chip_info, false);
            if (ret < 0) {
                TPD_INFO("%s: hx83102d enable interrupt failed.\n", __func__);
                return ret;
            }
        }

        hx83102d_resetgpio_set(chip_info->hw_res, false);

        hx83102d_resetgpio_set(chip_info->hw_res, true);

        if (loadconfig) {
            ret = hx83102d_configuration_init(chip_info, false);
            if (ret < 0) {
                TPD_INFO("%s: hx83102d configuration init failed.\n", __func__);
                return ret;
            }
            ret = hx83102d_configuration_init(chip_info, true);
            if (ret < 0) {
                TPD_INFO("%s: hx83102d configuration init failed.\n", __func__);
                return ret;
            }
        }
        if (int_off) {
            ret = hx83102d_enable_interrupt(chip_info, true);
            if (ret < 0) {
                TPD_INFO("%s: hx83102d enable interrupt failed.\n", __func__);
                return ret;
            }
        }
    }
    return 0;
}

static size_t hx83102d_proc_reset_write(struct file *file, const char *buff,
                                        size_t len, loff_t *pos)
{
    char buf_tmp[12];
    struct touchpanel_data *ts = PDE_DATA(file_inode(file));
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)
                                           ts->chip_data;

    if (len >= 12) {
        TPD_INFO("%s: no command exceeds 12 chars.\n", __func__);
        return -EFAULT;
    }
    if (copy_from_user(buf_tmp, buff, len)) {
        return -EFAULT;
    }
    if (buf_tmp[0] == '1')
        himax_ic_reset(chip_info, false, false);
    else if (buf_tmp[0] == '2')
        himax_ic_reset(chip_info, false, true);
    else if (buf_tmp[0] == '3')
        himax_ic_reset(chip_info, true, false);
    else if (buf_tmp[0] == '4')
        himax_ic_reset(chip_info, true, true);
    else if ((buf_tmp[0] == 'z') && (buf_tmp[1] == 'r')) {
        himax_mcu_0f_operation_check(0);
        himax_mcu_0f_operation_check(1);
    } else if ((buf_tmp[0] == 'z') && (buf_tmp[1] == 'w')) {
        himax_mcu_0f_operation_dirly();
    }
    return len;
}

static size_t hx83102d_proc_sense_on_off_write(struct file *file,
        const char *buff, size_t len, loff_t *pos)
{
    char buf[80] = {0};
    if (len >= 80) {
        TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
        return -EFAULT;
    }
    if (copy_from_user(buf, buff, len))
        return -EFAULT;

    if (buf[0] == '0') {
        himax_sense_off();
        TPD_INFO("Sense off \n");
    } else if (buf[0] == '1') {
        if (buf[1] == 's') {
            himax_sense_on(0x00);
            TPD_INFO("Sense on re-map on, run sram \n");
        } else {
            himax_sense_on(0x01);
            TPD_INFO("Sense on re-map off, run flash \n");
        }
    } else {
        TPD_INFO("Do nothing \n");
    }
    return len;
}

static int hx83102d_get_touch_points(void *chip_data, struct point_info *points,
                                     int max_num)
{
    int i, x, y, z = 1, obj_attention = 0;

    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    char buf[128];
    uint16_t mutual_num;
    uint16_t self_num;
    int ret = 0;
    int check_sum_cal;
    int ts_status = HX_REPORT_COORD;
    int hx_point_num;
    uint8_t hx_state_info_pos;

    if (!hx_touch_data)
        TPD_INFO("%s:%d hx_touch_data is NULL\n", __func__, __LINE__);

    if (!hx_touch_data->hx_coord_buf) {
        TPD_INFO("%s:%d hx_touch_data->hx_coord_buf is NULL\n", __func__, __LINE__);
        return 0;
    }

    himax_burst_enable(0);
    if (g_diag_command)
        ret = himax_read_event_stack(buf, 128);
    else
        ret = himax_read_event_stack(buf, hx_touch_data->touch_info_size);
    if (!ret) {
        TPD_INFO("%s: can't read data from chip in normal!\n", __func__);
        goto checksum_fail;
    }

    if (LEVEL_DEBUG == tp_debug)
        himax_log_touch_data(buf, hx_touch_data);

    check_sum_cal = himax_checksum_cal(chip_info, buf, ts_status);
    if (check_sum_cal == CHECKSUM_FAIL) {
        goto checksum_fail;
    } else if (check_sum_cal == ERR_WORK_OUT) {
        goto err_workqueue_out;
    } else if (check_sum_cal == WORK_OUT) {
        goto workqueue_out;
    }

    hx_state_info_pos = hx_touch_data->touch_info_size - 3;
    if (ts_status == HX_REPORT_COORD) {
        memcpy(hx_touch_data->hx_coord_buf, &buf[0], hx_touch_data->touch_info_size);
        if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
            memcpy(hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
        else
            memset(hx_touch_data->hx_state_info, 0x00,
                   sizeof(hx_touch_data->hx_state_info));
    }
    if (g_diag_command) {
        mutual_num = chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM;
        self_num = chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM;
        TPD_INFO("hx_touch_data->touch_all_size= %d hx_touch_data->touch_info_size = %d ,  %d\n",
                 hx_touch_data->touch_all_size, hx_touch_data->touch_info_size,
                 hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
        memcpy(hx_touch_data->hx_rawdata_buf, &buf[hx_touch_data->touch_info_size],
               hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
        if (!diag_check_sum(hx_touch_data))
            goto err_workqueue_out;
        diag_parse_raw_data(hx_touch_data, mutual_num, self_num, g_diag_command,
                            hx_touch_data->diag_mutual, diag_self);
    }

    if (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
        hx_point_num = 0;
    else
        hx_point_num = hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;
    for (i = 0; i < 10; i++) {
        x = hx_touch_data->hx_coord_buf[i * 4] << 8;
        x = x | hx_touch_data->hx_coord_buf[i * 4 + 1];
        y = hx_touch_data->hx_coord_buf[i * 4 + 2] << 8;
        y = y | hx_touch_data->hx_coord_buf[i * 4 + 3];
        z = hx_touch_data->hx_coord_buf[i + 40];
        if (x >= 0 && x <= private_ts->resolution_info.max_x && y >= 0
            && y <= private_ts->resolution_info.max_y) {
            points[i].x = x;
            points[i].y = y;
            points[i].width_major = z;
            points[i].touch_major = z;
            points[i].status = 1;
            obj_attention = obj_attention | (0x0001 << i);
        }
    }

    /*TPD_DEBUG("%s:%d  obj_attention = 0x%x\n", __func__, __LINE__, obj_attention);*/

checksum_fail:
    return obj_attention;
err_workqueue_out:
workqueue_out:
    /*himax_ic_reset(chip_info, false, true);*/
    return -EINVAL;

}

static int hx83102d_ftm_process(void *chip_data)
{
    hx83102d_resetgpio_set(g_chip_info->hw_res, false);
    return 0;
}

static int hx83102d_get_vendor(void *chip_data, struct panel_info *panel_data)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;

    chip_info->tp_type = panel_data->tp_type;
    chip_info->p_tp_fw = &panel_data->TP_FW;
    TPD_INFO("chip_info->tp_type = %d, panel_data->test_limit_name = %s, panel_data->fw_name = %s\n",
             chip_info->tp_type, panel_data->test_limit_name, panel_data->fw_name);
    return 0;
}


static int hx83102d_get_chip_info(void *chip_data)
{
    return 1;
}

/**
 * hx83102d_get_fw_id -   get device fw id.
 * @chip_info: struct include i2c resource.
 * Return fw version result.
 */
static uint32_t hx83102d_get_fw_id(struct chip_data_hx83102d *chip_info)
{
    uint32_t current_firmware = 0;
    uint8_t cmd[4];
    uint8_t data[64];
    /*oppo fw id bin address : 0xc014	 , 49172	Tp ic address : 0x 10007014*/

    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x14;
    himax_register_read(cmd, 4, data, false);

    TPD_DEBUG("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,data[3]=0x%2.2X\n",
              __func__, data[0], data[1], data[2], data[3]);
    current_firmware = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    TPD_INFO("CURRENT_FIRMWARE_ID = 0x%x\n", current_firmware);

    return current_firmware;

}

static void __init get_lcd_vendor(void)
{
    if (strstr(boot_command_line, "1080p_dsi_vdo-1-fps")) {
        g_lcd_vendor = 1;
    } else if (strstr(boot_command_line, "1080p_dsi_vdo-2-fps")) {
        g_lcd_vendor = 2;
    } else if (strstr(boot_command_line, "1080p_dsi_vdo-3-fps")) {
        g_lcd_vendor = 3;
    } else if (strstr(boot_command_line, "1080p_dsi_vdo-7-fps")) {
        g_lcd_vendor = 7;
    } else if (strstr(boot_command_line, "1080p_dsi_vdo-8-fps")) {
        g_lcd_vendor = 8;
    } else if (strstr(boot_command_line, "1080p_dsi_vdo-9-fps")) {
        g_lcd_vendor = 9;
    }
}

static fw_check_state hx83102d_fw_check(void *chip_data,
                                        struct resolution_info *resolution_info, struct panel_info *panel_data)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;

    panel_data->TP_FW = hx83102d_get_fw_id(chip_info);
    if (panel_data->manufacture_info.version)
        sprintf(panel_data->manufacture_info.version, "0x%x-%d", panel_data->TP_FW,
                g_lcd_vendor);

    return FW_NORMAL;
}

static u8 hx83102d_trigger_reason(void *chip_data, int gesture_enable,
                                  int is_suspended)
{
    if ((gesture_enable == 1) && is_suspended)
        return IRQ_GESTURE;
    else
        return IRQ_TOUCH;

}

static void hx83102d_exit_esd_mode(void *chip_data)
{
    TPD_INFO("exit esd mode ok\n");
    return;
}

/*
 * return success: 0 ; fail : negative
 */
static int hx83102d_reset(void *chip_data)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    int ret = 0;
    int load_fw_times = 10;

    TPD_INFO("%s.\n", __func__);

    if (!chip_info->first_download_finished) {
        TPD_INFO("%s:First download has not finished, don't do reset.\n", __func__);
        return 0;
    }

    g_zero_event_count = 0;
    /*clear touch download flag*/
    clear_view_touchdown_flag();
    /*esd hw reset*/
    HX_ESD_RESET_ACTIVATE = 0;
    disable_irq_nosync(chip_info->hx_irq);

    do {
        load_fw_times--;
        g_core_fp.fp_0f_operation_dirly();
        ret = g_core_fp.fp_reload_disable(0);
    } while (!ret && load_fw_times > 0);

    if (!load_fw_times)
        TPD_INFO("%s: load_fw_times over 10 times\n", __func__);

    himax_sense_on(0x00);

    /*Yulianghan@RM.PSW.BSP.TP, 2018/09/07, add for hx83112a_noflash lcd esd TP irq exception in realme 18612*/
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
    enable_irq(chip_info->hx_irq);
#endif
    return ret;
}

void himax_ultra_enter(void)
{
    uint8_t tmp_data[4];
    int rtimes = 0;
    TPD_INFO("%s:entering\n", __func__);
    /* 34 -> 11 */
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:1/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0x11;
        if (himax_bus_write(0x34, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x34, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x34, correct 0x11 = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0x11);

    /* 33 -> 33 */
    rtimes = 0;
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:2/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0x33;
        if (himax_bus_write(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0x33);

    /* 34 -> 22 */
    rtimes = 0;
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:3/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0x22;
        if (himax_bus_write(0x34, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x34, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x34, correct 0x22 = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0x22);

    /* 33 -> AA */
    rtimes = 0;
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:4/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0xAA;
        if (himax_bus_write(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0xAA);

    /* 33 -> 33 */
    rtimes = 0;
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:5/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0x33;
        if (himax_bus_write(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0x33);

    /* 33 -> AA */
    rtimes = 0;
    do {
        if (rtimes > 10) {
            TPD_INFO("%s:6/6 retry over 10 times!\n", __func__);
            return;
        }
        tmp_data[0] = 0xAA;
        if (himax_bus_write(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi write fail!\n", __func__);
            continue;
        }
        tmp_data[0] = 0x00;
        if (himax_bus_read(0x33, 1, tmp_data) < 0) {
            TPD_INFO("%s: spi read fail!\n", __func__);
            continue;
        }

        TPD_INFO("%s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
                 __func__, rtimes, tmp_data[0]);
        rtimes++;
    } while (tmp_data[0] != 0xAA);

    TPD_INFO("%s:END\n", __func__);
}

static int hx83102d_enable_black_gesture(struct chip_data_hx83102d *chip_info,
        bool enable)
{
    int ret = 0;
    struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);
    static char ovl_done;
    TPD_INFO("%s:enable=%d, ts->is_suspended=%d \n", __func__, enable,
             ts->is_suspended);
    if (ts->is_suspended) {
        if (ovl_done == 0)
            hx83102d_0f_overlay(2, 1);
        if (enable) {
            if (ovl_done == 1) {
                /*reset gpio*/
                hx83102d_resetgpio_set(chip_info->hw_res, true);
                hx83102d_resetgpio_set(chip_info->hw_res, false);
                hx83102d_resetgpio_set(chip_info->hw_res, true);
            }
        } else {

            himax_ultra_enter();
        }
        if (ovl_done == 0)
            ovl_done = 1;
    } else {
        hx83102d_0f_overlay(2, 0);
        ovl_done = 0;
        //himax_sense_on(0);
    }
    return ret;
}
static int hx83102d_enable_charge_mode(struct chip_data_hx83102d *chip_info,
                                       bool enable)
{
    int ret = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    TPD_INFO("%s, charge mode enable = %d\n", __func__, enable);

    /*Enable:0x10007F38 = 0xA55AA55A*/
    if (enable) {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x38;
        tmp_data[3] = 0xA5;
        tmp_data[2] = 0x5A;
        tmp_data[1] = 0xA5;
        tmp_data[0] = 0x5A;
        himax_flash_write_burst(tmp_addr, tmp_data);
    } else {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x7F;
        tmp_addr[0] = 0x38;
        tmp_data[3] = 0x77;
        tmp_data[2] = 0x88;
        tmp_data[1] = 0x77;
        tmp_data[0] = 0x88;
        himax_flash_write_burst(tmp_addr, tmp_data);
    }

    return ret;
}

/*on = 1:on   0:off*/
static int hx83102d_jitter_switch(struct chip_data_hx83102d *chip_info, bool on)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int rtimes = 0;
    int ret = 0;

    TPD_INFO("%s:entering\n", __func__);
    /*jitter off*/
    if (!on) {
        do {
            if (rtimes > 10) {
                TPD_INFO("%s:retry over 10, jitter off failed!\n", __func__);
                TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
                ret = -1;
                break;
            }

            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0xE0;
            tmp_data[3] = 0xA5;
            tmp_data[2] = 0x5A;
            tmp_data[1] = 0xA5;
            tmp_data[0] = 0x5A;
            himax_flash_write_burst(tmp_addr, tmp_data);

            himax_register_read(tmp_addr, 4, tmp_data, false);

            TPD_INFO("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                     __func__,
                     rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
            rtimes++;
        } while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A || tmp_data[1] != 0xA5
                 || tmp_data[0] != 0x5A);
        TPD_INFO("%s:jitter off success!\n", __func__);
    } else {
        /*jitter on*/
        do {
            if (rtimes > 10) {
                TPD_INFO("%s:retry over 10, jitter on failed!\n", __func__);
                TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x00, 0x00, 0x00, 0x00\n",
                         __func__);
                ret = -1;
                break;
            }

            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0xE0;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);

            himax_register_read(tmp_addr, 4, tmp_data, false);
            TPD_INFO("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                     __func__,
                     rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
            rtimes++;
        } while (tmp_data[3] == 0xA5 && tmp_data[2] == 0x5A && tmp_data[1] == 0xA5
                 && tmp_data[0] == 0x5A);
        TPD_INFO("%s:jitter on success!\n", __func__);
    }
    TPD_INFO("%s:END\n", __func__);
    return ret;
}

static int hx83102d_enable_headset_mode(struct chip_data_hx83102d *chip_info,
                                        bool enable)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int rtimes = 0;
    int ret = 0;
    struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);

    if (ts->headset_pump_support) {
        /*insert headset*/
        if (enable) {
            do {
                if (rtimes > 10) {
                    TPD_INFO("%s:insert headset failed!\n", __func__);
                    TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
                    ret = -1;
                    break;
                }

                tmp_addr[3] = 0x10;
                tmp_addr[2] = 0x00;
                tmp_addr[1] = 0x7F;
                tmp_addr[0] = 0xE8;
                tmp_data[3] = 0xA5;
                tmp_data[2] = 0x5A;
                tmp_data[1] = 0xA5;
                tmp_data[0] = 0x5A;
                himax_flash_write_burst(tmp_addr, tmp_data);
                himax_register_read(tmp_addr, 4, tmp_data, false);
                TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                           __func__,
                           rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                rtimes++;
            } while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A || tmp_data[1] != 0xA5
                     || tmp_data[0] != 0x5A);
            TPD_INFO("%s:insert headset success!\n", __func__);
        } else {
            do {
                if (rtimes > 10) {
                    TPD_INFO("%s:remove headset failed!\n", __func__);
                    TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
                    ret = -1;
                    break;
                }

                tmp_addr[3] = 0x10;
                tmp_addr[2] = 0x00;
                tmp_addr[1] = 0x7F;
                tmp_addr[0] = 0xE8;
                tmp_data[3] = 0x00;
                tmp_data[2] = 0x00;
                tmp_data[1] = 0x00;
                tmp_data[0] = 0x00;
                himax_flash_write_burst(tmp_addr, tmp_data);

                himax_register_read(tmp_addr, 4, tmp_data, false);
                TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                           __func__,
                           rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                rtimes++;
            } while (tmp_data[3] != 0x00 || tmp_data[2] != 0x00 || tmp_data[1] != 0x00
                     || tmp_data[0] != 0x00);

            TPD_INFO("%s:remove headset success!\n", __func__);
        }
    }
    return ret;
}

/*mode = 0:off   1:normal   2:turn right    3:turn left*/
static int hx83102d_rotative_switch(struct chip_data_hx83102d *chip_info,
                                    int mode)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int rtimes = 0;
    int ret = 0;
    struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);

    TPD_DETAIL("%s:entering\n", __func__);

    if (ts->fw_edge_limit_support) {
        if (mode == 1 || VERTICAL_SCREEN == chip_info->touch_direction) {
            do {
                if (rtimes > 10) {
                    TPD_INFO("%s:rotative normal failed!\n", __func__);
                    TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
                    ret = -1;
                    break;
                }

                tmp_addr[3] = 0x10;
                tmp_addr[2] = 0x00;
                tmp_addr[1] = 0x7F;
                tmp_addr[0] = 0x3C;
                tmp_data[3] = 0xA5;
                tmp_data[2] = 0x5A;
                tmp_data[1] = 0xA5;
                tmp_data[0] = 0x5A;
                himax_flash_write_burst(tmp_addr, tmp_data);

                himax_register_read(tmp_addr, 4, tmp_data, false);
                TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                           __func__,
                           rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                rtimes++;
            } while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A || tmp_data[1] != 0xA5
                     || tmp_data[0] != 0x5A);

            TPD_INFO("%s:rotative normal success!\n", __func__);

        } else {
            rtimes = 0;
            /*turn right*/
            if (LANDSCAPE_SCREEN_270 == chip_info->touch_direction) {
                do {
                    if (rtimes > 10) {
                        TPD_INFO("%s:rotative right failed!\n", __func__);
                        TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x3A,0xA3,0x3A,0xA3\n", __func__);
                        ret = -1;
                        break;
                    }

                    tmp_addr[3] = 0x10;
                    tmp_addr[2] = 0x00;
                    tmp_addr[1] = 0x7F;
                    tmp_addr[0] = 0x3C;
                    tmp_data[3] = 0xA3;
                    tmp_data[2] = 0x3A;
                    tmp_data[1] = 0xA3;
                    tmp_data[0] = 0x3A;
                    himax_flash_write_burst(tmp_addr, tmp_data);
                    himax_register_read(tmp_addr, 4, tmp_data, false);
                    TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
                               __func__,
                               rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                    rtimes++;
                } while (tmp_data[3] != 0xA3 || tmp_data[2] != 0x3A || tmp_data[1] != 0xA3
                         || tmp_data[0] != 0x3A);

                TPD_INFO("%s:rotative right success!\n", __func__);
            } else if (LANDSCAPE_SCREEN_90 == chip_info->touch_direction) {
                /*turn left*/
                do {
                    if (rtimes > 10) {
                        TPD_INFO("%s:rotative left failed!\n", __func__);
                        TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x1A,0xA1,0x1A,0xA1\n", __func__);
                        ret = -1;
                        break;
                    }

                    tmp_addr[3] = 0x10;
                    tmp_addr[2] = 0x00;
                    tmp_addr[1] = 0x7F;
                    tmp_addr[0] = 0x3C;
                    tmp_data[3] = 0xA1;
                    tmp_data[2] = 0x1A;
                    tmp_data[1] = 0xA1;
                    tmp_data[0] = 0x1A;
                    himax_flash_write_burst(tmp_addr, tmp_data);

                    himax_register_read(tmp_addr, 4, tmp_data, false);

                    TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
                               __func__,
                               rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                    rtimes++;
                } while (tmp_data[3] != 0xA1 || tmp_data[2] != 0x1A || tmp_data[1] != 0xA1
                         || tmp_data[0] != 0x1A);

                TPD_INFO("%s:rotative left success!\n", __func__);

            }
        }
    } else {
        /*open*/
        if (mode) {
            do {
                if (rtimes > 10) {
                    TPD_INFO("%s:open edge limit failed!\n", __func__);
                    TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
                    ret = -1;
                    break;
                }

                tmp_addr[3] = 0x10;
                tmp_addr[2] = 0x00;
                tmp_addr[1] = 0x7F;
                tmp_addr[0] = 0x3C;
                tmp_data[3] = 0xA5;
                tmp_data[2] = 0x5A;
                tmp_data[1] = 0xA5;
                tmp_data[0] = 0x5A;
                himax_flash_write_burst(tmp_addr, tmp_data);

                himax_register_read(tmp_addr, 4, tmp_data, false);
                TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
                           __func__,
                           rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                rtimes++;
            }  while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A || tmp_data[1] != 0xA5
                      || tmp_data[0] != 0x5A);

            TPD_INFO("%s:open edge limit success!\n", __func__);
        } else {
            do {
                if (rtimes > 10) {
                    TPD_INFO("%s:close edge limit failed!\n", __func__);
                    TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x9A,0xA9,0x9A,0xA9\n", __func__);
                    ret = -1;
                    break;
                }

                tmp_addr[3] = 0x10;
                tmp_addr[2] = 0x00;
                tmp_addr[1] = 0x7F;
                tmp_addr[0] = 0x3C;
                tmp_data[3] = 0xA9;
                tmp_data[2] = 0x9A;
                tmp_data[1] = 0xA9;
                tmp_data[0] = 0x9A;
                himax_flash_write_burst(tmp_addr, tmp_data);

                himax_register_read(tmp_addr, 4, tmp_data, false);
                TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
                           __func__,
                           rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
                rtimes++;
            } while (tmp_data[3] != 0xA9 || tmp_data[2] != 0x9A || tmp_data[1] != 0xA9
                     || tmp_data[0] != 0x9A);
            TPD_INFO("%s:close edge limit success!\n", __func__);
        }
    }
    TPD_DETAIL("%s:END\n", __func__);
    return ret;
}

static int hx83102d_mode_switch(void *chip_data, work_mode mode, bool flag)
{
    int ret = -1;
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    switch (mode) {
    case MODE_NORMAL:
        ret = hx83102d_configuration_init(chip_info, true);
        if (ret < 0) {
            TPD_INFO("%s: hx83102d configuration init failed.\n", __func__);
            return ret;
        }
        break;

    case MODE_SLEEP:
        /*device control: sleep mode*/

        ret = hx83102d_configuration_init(chip_info, false) ;
        if (ret < 0) {
            TPD_INFO("%s: hx83102d configuration init failed.\n", __func__);
            return ret;
        }

        break;

    case MODE_GESTURE:
        ret = hx83102d_enable_black_gesture(chip_info, flag);
        if (ret < 0) {
            TPD_INFO("%s: hx83102d enable gesture failed.\n", __func__);
            return ret;
        }

        break;

    case MODE_GLOVE:

        break;

    case MODE_EDGE:
        ret = hx83102d_rotative_switch(chip_info, flag);
        if (ret < 0) {
            TPD_INFO("%s: hx83102d enable edg & corner limit failed.\n", __func__);
            return ret;
        }

        break;
    case MODE_CHARGE:
        ret = hx83102d_enable_charge_mode(chip_info, flag);
        if (ret < 0)
            TPD_INFO("%s: enable charge mode : %d failed\n", __func__, flag);
        break;

    case MODE_HEADSET:
        ret = hx83102d_enable_headset_mode(chip_info, flag);
        if (ret < 0)
            TPD_INFO("%s: enable headset mode : %d failed\n", __func__, flag);
        break;

    case MODE_GAME:
        ret = hx83102d_jitter_switch(chip_info, !flag);
        if (ret < 0)
            TPD_INFO("%s: enable game mode : %d failed\n", __func__, !flag);
        break;

    default:
        TPD_INFO("%s: Wrong mode.\n", __func__);
    }

    return ret;
}
void himax_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t back_data[4];
    uint8_t retry_cnt = 0;

    himax_sense_off();

    /*Enable:0x10007F10 = 0xA55AA55A*/
    do {
        if (SMWP_enable) {
            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0x10;
            tmp_data[3] = 0xA5;
            tmp_data[2] = 0x5A;
            tmp_data[1] = 0xA5;
            tmp_data[0] = 0x5A;
            himax_flash_write_burst(tmp_addr, tmp_data);
            back_data[3] = 0XA5;
            back_data[2] = 0X5A;
            back_data[1] = 0XA5;
            back_data[0] = 0X5A;
        } else {
            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0x10;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(tmp_addr, tmp_data);
            back_data[3] = 0X00;
            back_data[2] = 0X00;
            back_data[1] = 0X00;
            back_data[0] = 0x00;
        }
        himax_register_read(tmp_addr, 4, tmp_data, false);
        TPD_INFO("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d \n", __func__,
                 tmp_data[0], SMWP_enable, retry_cnt);
        retry_cnt++;
    } while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
              || tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
             && retry_cnt < 10);

    himax_sense_on(0);

}
static int hx83102d_get_gesture_info(void *chip_data,
                                     struct gesture_info *gesture)
{
    int i = 0;
    int j = 0;
    int gesture_sign = 0;
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    uint8_t *buf;

    int check_sum_cal;
    int ts_status = HX_REPORT_SMWP_EVENT;

    /*TPD_DEBUG("%s:%d\n", __func__, __LINE__);*/

    buf = kzalloc(hx_touch_data->event_size * sizeof(uint8_t), GFP_KERNEL);
    if (!buf) {
        TPD_INFO("%s:%d kzalloc buf error\n", __func__, __LINE__);
        return -1;
    }

    himax_burst_enable(0);
    if (!himax_read_event_stack(buf, hx_touch_data->event_size)) {
        TPD_INFO("%s: can't read data from chip in gesture!\n", __func__);
        kfree(buf);
        return -1;
    }

    printk("%s: gesture buf data\n", __func__);
    for (i = 0; i < 12; i++)
        printk("0x%02X 0x%02X\n",  buf[i * 2], buf[i * 2 + 1]);

    check_sum_cal = himax_checksum_cal(chip_info, buf, ts_status);
    if (check_sum_cal == CHECKSUM_FAIL)
        return -1;
    else if (check_sum_cal == ERR_WORK_OUT)
        goto err_workqueue_out;

    for (j = 0; j < GEST_SUP_NUM; j++) {
        if (buf[0] == gest_event[j]) {
            gesture_sign = gest_key_def[j];
            break;
        }
    }
    if (j >= GEST_SUP_NUM)
        goto RET_OUT;

    TPD_DEBUG("Himax gesture_sign= %x\n", gesture_sign);
    if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1 ||
        buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2) {
        goto RET_OUT;
    }

    /*TPD_DEBUG("gest_len = %d ",gest_len);*/

    i = 0;

    /*TPD_DEBUG("gest doornidate start  %s\n",__func__);*/
    while (i < 6) {

        gest_pt_x[i] = buf[GEST_PTLG_ID_LEN + 4 + i * 2] *
                       private_ts->resolution_info.max_x / 255;

        gest_pt_y[i] =  buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1] *
                        private_ts->resolution_info.max_y / 255;
        i++;
        /*TPD_DEBUG("gest_pt_x[%d]=%d \n",gest_pt_cnt,gest_pt_x[gest_pt_cnt]);*/
        /*TPD_DEBUG("gest_pt_y[%d]=%d \n",gest_pt_cnt,gest_pt_y[gest_pt_cnt]);*/

    }

    gesture->gesture_type = gesture_sign;
    gesture->Point_start.x = gest_pt_x[0];
    gesture->Point_start.y = gest_pt_y[0];
    gesture->Point_end.x  = gest_pt_x[1];
    gesture->Point_end.y  = gest_pt_y[1];
    gesture->Point_1st.x  = gest_pt_x[2];
    gesture->Point_1st.y  = gest_pt_y[2];
    gesture->Point_2nd.x  = gest_pt_x[3];
    gesture->Point_2nd.y  = gest_pt_y[3];
    gesture->Point_3rd.x  = gest_pt_x[4];
    gesture->Point_3rd.y   = gest_pt_y[4];
    gesture->Point_4th.x   = gest_pt_x[5];
    gesture->Point_4th.y   = gest_pt_y[5];
    gesture->clockwise     = buf[GEST_PTLG_ID_LEN + 4 + 12];
    TPD_DEBUG("t=%d, S=%d,%d, E=%d,%d, p1=%d,%d, p2=%d,%d, p3=%d,%d, p4=%d,%d, c=%d\n",
              gesture->gesture_type,
              gesture->Point_start.x, gesture->Point_start.y,
              gesture->Point_end.x, gesture->Point_end.y,
              gesture->Point_1st.x, gesture->Point_1st.y,
              gesture->Point_2nd.x, gesture->Point_2nd.y,
              gesture->Point_3rd.x, gesture->Point_3rd.y,
              gesture->Point_4th.x, gesture->Point_4th.y,
              gesture->clockwise);

RET_OUT:
    if (buf) {
        kfree(buf);
    }
    return 0;

err_workqueue_out:
    /*himax_ic_reset(chip_info, false, true);*/
    return -1;
}

static int hx83102d_power_control(void *chip_data, bool enable)
{
    int ret = 0;
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    if (true == enable) {
        ret = tp_powercontrol_2v8(chip_info->hw_res, true);
        if (ret)
            return -1;
        ret = tp_powercontrol_1v8(chip_info->hw_res, true);
        if (ret)
            return -1;
        ret = hx83102d_resetgpio_set(chip_info->hw_res, true);
        if (ret)
            return -1;
    } else {
        ret = hx83102d_resetgpio_set(chip_info->hw_res, false);
        if (ret)
            return -1;
        ret = tp_powercontrol_1v8(chip_info->hw_res, false);
        if (ret)
            return -1;
        ret = tp_powercontrol_2v8(chip_info->hw_res, false);
        if (ret)
            return -1;
    }

    return ret;
}

static void store_to_file(int fd, char *format, ...)
{
    va_list args;
    char buf[64] = { 0 };
    va_start(args, format);
    vsnprintf(buf, 64, format, args);
    va_end(args);
    if (fd >= 0)
        sys_write(fd, buf, strlen(buf));
}

static int hx83102d_int_pin_test(struct seq_file *s, void *chip_data,
                                 struct syna_testdata *syna_testdata)
{
    int eint_status, eint_count = 0, read_gpio_num = 10;

    TPD_INFO("%s, step 0: begin to check INT-GND short item\n", __func__);
    while (read_gpio_num--) {
        msleep(5);
        eint_status = gpio_get_value(syna_testdata->irq_gpio);
        if (eint_status == 1)
            eint_count--;
        else
            eint_count++;
        TPD_INFO("%s eint_count = %d  eint_status = %d\n", __func__, eint_count,
                 eint_status);
    }
    TPD_INFO("TP EINT PIN direct short! eint_count = %d\n", eint_count);
    if (eint_count == 10) {
        TPD_INFO("error :  TP EINT PIN direct short!\n");
        seq_printf(s, "TP EINT direct stort\n");
        store_to_file(syna_testdata->fd,
                      "eint_status is low, TP EINT direct stort, \n");
        eint_count = 0;
        return TEST_FAIL;
    }
    return TEST_PASS;
}

static void hx83102d_auto_test(struct seq_file *s, void *chip_data,
                               struct syna_testdata *syna_testdata)
{
    int error_count = 0;
    int ret = 0;
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    char *p_node = NULL;
    char *fw_name_test = NULL;
    char *postfix = "_TEST.img";
    uint8_t copy_len = 0;
	/*
	uint8_t mpap_addr[4] = {0xEC, 0x73, 0x00, 0x10};
	uint8_t enter_data[4] = {0x80, 0x73, 0x10, 0x00};
	*/
    fw_name_test = kzalloc(MAX_FW_NAME_LENGTH, GFP_KERNEL);
    if (fw_name_test == NULL) {
        TPD_INFO("fw_name_test kzalloc error!\n");
        return;
    }
	/*himax_register_write(mpap_addr, 4, enter_data, 0);*/
	IN_SELF_TEST = 1;
	if(IN_SELF_TEST)
		IN_BASELINE_TEST = 1;

    p_node  = strstr(private_ts->panel_data.fw_name, ".");
    copy_len = p_node - private_ts->panel_data.fw_name;
    memcpy(fw_name_test, private_ts->panel_data.fw_name, copy_len);
    strlcat(fw_name_test, postfix, MAX_FW_NAME_LENGTH);
    TPD_INFO("%s : fw_name_test is %s\n", __func__, fw_name_test);

    himax_mcu_0f_operation_test_dirly(fw_name_test);
    msleep(5);
    g_core_fp.fp_reload_disable(0);
    msleep(5);
    himax_sense_on(0x00);
    himax_read_OPPO_FW_ver();

    ret = hx83102d_enable_interrupt(chip_info, false);

    error_count += hx83102d_int_pin_test(s, chip_info, syna_testdata);
    error_count += himax_chip_self_test(s, chip_info);

    seq_printf(s, "imageid = 0x%llx, deviceid = 0x%llx\n", syna_testdata->TP_FW,
               syna_testdata->TP_FW);
    seq_printf(s, "%d error(s). %s\n", error_count,
               error_count ? "" : "All test passed.");
    TPD_INFO(" TP auto test %d error(s). %s\n", error_count,
             error_count ? "" : "All test passed.");
}

static void hx83102d_read_debug_data(struct seq_file *s, void *chip_data,
                                     int debug_data_type)
{
    uint16_t mutual_num;
    uint16_t self_num;
    uint16_t width;
    int i = 0;
    int j = 0;
    int k = 0;
    int32_t *data_mutual_sram;

    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    if (!chip_info)
        return ;

    data_mutual_sram = kzalloc(chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM * sizeof(int32_t), GFP_KERNEL);
    if (!data_mutual_sram)
        goto RET_OUT;

    mutual_num = chip_info->hw_res->TX_NUM * chip_info->hw_res->RX_NUM;
    self_num = chip_info->hw_res->TX_NUM + chip_info->hw_res->RX_NUM;
    width = chip_info->hw_res->RX_NUM;
    seq_printf(s, "ChannelStart (rx tx) : %4d, %4d\n\n", chip_info->hw_res->RX_NUM,
               chip_info->hw_res->TX_NUM);

    /*start to show debug data*/
    switch (debug_data_type) {
    case DEBUG_DATA_BASELINE:
        seq_printf(s, "Baseline data: \n");
        TPD_INFO("Baseline data: \n");
        break;

    case DEBUG_DATA_RAW:
        seq_printf(s, "Raw data: \n");
        TPD_INFO("Raw data: \n");
        break;

    case DEBUG_DATA_DELTA:
        seq_printf(s, "Delta data: \n");
        TPD_INFO("Raw data: \n");
        break;

    default:
        seq_printf(s, "No this debug datatype \n");
        TPD_INFO("No this debug datatype \n");
        goto RET_OUT;
        break;
    }

    himax_diag_register_set(debug_data_type);
    TPD_INFO("%s: Start get baseline data in DSRAM\n", __func__);
    DSRAM_Flag = true;

    himax_ts_diag_func(chip_info, data_mutual_sram);

    for (j = 0; j < chip_info->hw_res->RX_NUM; j++) {
        for (i = 0; i < chip_info->hw_res->TX_NUM; i++) {
            k = ((mutual_num - j) - chip_info->hw_res->RX_NUM * i) - 1;
            seq_printf(s, "%6d", data_mutual_sram[k]);
        }
        seq_printf(s, " %6d\n", diag_self[j]);
    }
    seq_printf(s, "\n");
    for (i = 0; i < chip_info->hw_res->TX_NUM; i++) {
        seq_printf(s, "%6d", diag_self[i]);
    }
    /*Clear DSRAM flag*/
    himax_diag_register_set(0x00);
    DSRAM_Flag = false;
    himax_return_event_stack();

    seq_printf(s, "\n");
    seq_printf(s, "ChannelEnd");
    seq_printf(s, "\n");

    TPD_INFO("%s,here:%d\n", __func__, __LINE__);

RET_OUT:
    if (data_mutual_sram) {
        kfree(data_mutual_sram);
    }

    return;
}

static void hx83102d_baseline_read(struct seq_file *s, void *chip_data)
{
    hx83102d_read_debug_data(s, chip_data, DEBUG_DATA_BASELINE);
    hx83102d_read_debug_data(s, chip_data, DEBUG_DATA_RAW);
    return;
}

static void hx83102d_delta_read(struct seq_file *s, void *chip_data)
{
    hx83102d_read_debug_data(s, chip_data, DEBUG_DATA_DELTA);
    return;
}

static void hx83102d_main_register_read(struct seq_file *s, void *chip_data)
{
    return;
}

/*Reserved node*/
static void hx83102d_reserve_read(struct seq_file *s, void *chip_data)
{
    return;
}

static fw_update_state hx83102d_fw_update(void *chip_data,
        const struct firmware *fw, bool force)
{
    uint32_t CURRENT_FIRMWARE_ID = 0, FIRMWARE_ID = 0;
    int err = NO_ERR;
    uint8_t cmd[4];
    uint8_t data[64];
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    const uint8_t *p_fw_id = NULL;
    char *p_node = NULL;
    char *fw_name_fae = NULL;
    char *postfix = "_FAE";
    uint8_t copy_len = 0;

    if (fw == NULL) {
        TPD_INFO("fw is NULL\n");
        return FW_NO_NEED_UPDATE;
    }

    p_fw_id = fw->data  + 49172;

    if (!chip_info) {
        TPD_INFO("Chip info is NULL\n");
        return 0;
    }

    TPD_INFO("%s is called\n", __func__);

    /*step 1:fill Fw related header, get all data.*/


    /*step 2:Get FW version from IC && determine whether we need get into update flow.*/

    CURRENT_FIRMWARE_ID = (*p_fw_id << 24) | (*(p_fw_id + 1) << 16) | (*(p_fw_id + 2) << 8) | *(p_fw_id + 3);

    /*oppo fw id bin address : 0xc014	 , 49172	Tp ic address : 0x 10007014*/
    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x14;
    himax_register_read(cmd, 4, data, false);
    FIRMWARE_ID = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    TPD_INFO("CURRENT TP FIRMWARE ID is 0x%x, FIRMWARE IMAGE ID is 0x%x\n",
             CURRENT_FIRMWARE_ID, FIRMWARE_ID);

    if (private_ts->fw_update_app_support && force == 1) {
        fw_name_fae = kzalloc(MAX_FW_NAME_LENGTH, GFP_KERNEL);
        if (fw_name_fae == NULL) {
            TPD_INFO("fw_name_fae kzalloc error!\n");
        } else {
            p_node  = strstr(private_ts->panel_data.fw_name, ".");
            if (p_node != NULL) {
                copy_len = p_node - private_ts->panel_data.fw_name;
                memcpy(fw_name_fae, private_ts->panel_data.fw_name, copy_len);
                strlcat(fw_name_fae, postfix, MAX_FW_NAME_LENGTH);
                strlcat(fw_name_fae, p_node, MAX_FW_NAME_LENGTH);
                if (g_chip_info->g_fw_entry != NULL) {
                    release_firmware(g_chip_info->g_fw_entry);
                    g_chip_info->g_fw_entry = NULL;
                }
            }
        }
    }
    if (g_chip_info->g_fw_entry == NULL) {
        if (private_ts->fw_update_app_support) {
            if (fw_name_fae == NULL)
                err = request_firmware_select(&g_chip_info->g_fw_entry,
                                              private_ts->panel_data.fw_name, private_ts->dev);
            else
                err = request_firmware(&g_chip_info->g_fw_entry,  fw_name_fae, private_ts->dev);
        } else {
            err = request_firmware(&g_chip_info->g_fw_entry,
                                   private_ts->panel_data.fw_name, private_ts->dev);
        }
        if (err < 0) {
            TPD_INFO("%s, fail in line%d error code=%d\n", __func__, __LINE__, err);
            if (g_chip_info->g_fw_entry != NULL) {
                release_firmware(g_chip_info->g_fw_entry);
                g_chip_info->g_fw_entry = NULL;
            }
        }
    }

    TPD_INFO("update-----------------firmware ------------------update!\n");
    g_core_fp.fp_firmware_update_0f(fw);
    g_core_fp.fp_reload_disable(0);
    msleep (10);

    TPD_INFO("Firmware && configuration flash over\n");
    himax_read_OPPO_FW_ver();
    himax_sense_on(0x00);
    msleep (10);
    enable_irq(chip_info->hx_irq);
    if (fw_name_fae != NULL) {
        kfree(fw_name_fae);
        fw_name_fae = NULL;
    }
    chip_info->first_download_finished = true;
    return FW_UPDATE_SUCCESS;
}

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
extern unsigned int upmu_get_rgs_chrdet(void);
static int hx83102d_get_usb_state(void)
{
    return upmu_get_rgs_chrdet();
}
#else
static int hx83102d_get_usb_state(void)
{
    return 0;
}
#endif


static int hx83102d_reset_gpio_control(void *chip_data, bool enable)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;
    if (gpio_is_valid(chip_info->hw_res->reset_gpio)) {
        TPD_INFO("%s: set reset state %d\n", __func__, enable);
        hx83102d_resetgpio_set(g_chip_info->hw_res, enable);
        TPD_DETAIL("%s: set reset state END\n", __func__);
    }
    return 0;
}

static void hx83102d_set_touch_direction(void *chip_data, uint8_t dir)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;

    chip_info->touch_direction = dir;
}

static uint8_t hx83102d_get_touch_direction(void *chip_data)
{
    struct chip_data_hx83102d *chip_info = (struct chip_data_hx83102d *)chip_data;

    return chip_info->touch_direction;
}

static struct oppo_touchpanel_operations hx83102d_ops = {
    .ftm_process      = hx83102d_ftm_process,
    .get_vendor       = hx83102d_get_vendor,
    .get_chip_info    = hx83102d_get_chip_info,
    .reset            = hx83102d_reset,
    .power_control    = hx83102d_power_control,
    .fw_check         = hx83102d_fw_check,
    .fw_update        = hx83102d_fw_update,
    .trigger_reason   = hx83102d_trigger_reason,
    .get_touch_points = hx83102d_get_touch_points,
    .get_gesture_info = hx83102d_get_gesture_info,
    .mode_switch      = hx83102d_mode_switch,
    .exit_esd_mode    = hx83102d_exit_esd_mode,
    .get_usb_state    = hx83102d_get_usb_state,
    .black_screen_test = hx83102d_black_screen_test,
    .reset_gpio_control = hx83102d_reset_gpio_control,
    .set_touch_direction    = hx83102d_set_touch_direction,
    .get_touch_direction    = hx83102d_get_touch_direction,
};

static struct himax_proc_operations hx83102d_proc_ops = {
    .auto_test     = hx83102d_auto_test,
    .himax_proc_register_write =  hx83102d_proc_register_write,
    .himax_proc_register_read =  hx83102d_proc_register_read,
    .himax_proc_diag_write =  hx83102d_proc_diag_write,
    .himax_proc_diag_read =  hx83102d_proc_diag_read,
    .himax_proc_DD_debug_read =  hx83102d_proc_DD_debug_read,
    .himax_proc_DD_debug_write =  hx83102d_proc_DD_debug_write,
    .himax_proc_FW_debug_read =  hx83102d_proc_FW_debug_read,
    .himax_proc_reset_write =  hx83102d_proc_reset_write,
    .himax_proc_sense_on_off_write =  hx83102d_proc_sense_on_off_write,
};

static struct debug_info_proc_operations debug_info_proc_ops = {
    .limit_read    = himax_limit_read,
    .delta_read    = hx83102d_delta_read,
    .baseline_read = hx83102d_baseline_read,
    .main_register_read = hx83102d_main_register_read,
    .reserve_read = hx83102d_reserve_read,
};

static int hx83102d_tp_probe(struct spi_device *spi)
{
    struct chip_data_hx83102d *chip_info = NULL;
    struct touchpanel_data *ts = NULL;
    int ret = -1;

    TPD_INFO("%s  is called\n", __func__);

    /*step1:Alloc chip_info*/
    chip_info = kzalloc(sizeof(struct chip_data_hx83102d), GFP_KERNEL);
    if (chip_info == NULL) {
        TPD_INFO("chip info kzalloc error\n");
        ret = -ENOMEM;
        return ret;
    }
    memset(chip_info, 0, sizeof(*chip_info));
    g_chip_info = chip_info;

    /* allocate himax report data */
    hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
    if (hx_touch_data == NULL)
        goto err_register_driver;

    /*step2:Alloc common ts*/
    ts = common_touch_data_alloc();
    if (ts == NULL) {
        TPD_INFO("ts kzalloc error\n");
        goto err_register_driver;
    }
    memset(ts, 0, sizeof(*ts));

    /*step3:binding dev for easy operate*/
    chip_info->hx_spi = spi;
    chip_info->syna_ops = &hx83102d_proc_ops;
    ts->debug_info_ops = &debug_info_proc_ops;
    ts->s_client = spi;
    chip_info->hx_irq = spi->irq;
    ts->irq = spi->irq;
    spi_set_drvdata(spi, ts);
    ts->dev = &spi->dev;
    ts->chip_data = chip_info;
    chip_info->hw_res = &ts->hw_res;
    mutex_init(&(chip_info->spi_lock));
    chip_info->touch_direction = VERTICAL_SCREEN;
    chip_info->using_headfile = false;
    chip_info->first_download_finished = false;

    if (ts->s_client->master->flags & SPI_MASTER_HALF_DUPLEX) {
        TPD_INFO("Full duplex not supported by master\n");
        ret = -EIO;
        goto err_spi_setup;
    }
    ts->s_client->bits_per_word = 8;
    ts->s_client->mode = SPI_MODE_3;
    ts->s_client->chip_select = 0;

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
    /* new usage of MTK spi API */
    memcpy(&chip_info->hx_spi_mcc, &hx_spi_ctrdata, sizeof(struct mtk_chip_config));
    ts->s_client->controller_data = (void *)&chip_info->hx_spi_mcc;
#else
    /* old usage of MTK spi API */
    memcpy(&chip_info->hx_spi_mcc, &hx_spi_ctrdata, sizeof(struct mt_chip_conf));
    ts->s_client->controller_data = (void *)&chip_info->hx_spi_mcc;

    ret = spi_setup(ts->s_client);
    if (ret < 0) {
        TPD_INFO("Failed to perform SPI setup\n");
        goto err_spi_setup;
    }
#endif
    chip_info->p_spuri_fp_touch = &(ts->spuri_fp_touch);
    ts->ts_ops = &hx83102d_ops;
    private_ts = ts;

    /*step5:register common touch*/
    ret = register_common_touch_device(ts);
    if (ret < 0) {
        goto err_register_driver;
    }
    disable_irq_nosync(chip_info->hx_irq);
    if (himax_ic_package_check() == false) {
        TPD_INFO("Himax chip doesn NOT EXIST");
        goto err_register_driver;
    }

    chip_info->p_firmware_headfile = &ts->panel_data.firmware_headfile;
    g_auto_update_flag = true;
    chip_info->himax_0f_update_wq =
        create_singlethread_workqueue("HMX_0f_update_reuqest");
    INIT_DELAYED_WORK(&chip_info->work_0f_update, himax_mcu_0f_operation);
    /*queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update, msecs_to_jiffies(2000));*/


    himax_power_on_init();
    /*touch data init*/
    ret = himax_report_data_init(ts->max_num,  ts->hw_res.TX_NUM,
                                 ts->hw_res.RX_NUM);
    if (ret) {
        goto err_register_driver;
    }

    ts->tp_suspend_order = TP_LCD_SUSPEND;
    ts->tp_resume_order = LCD_TP_RESUME;
    ts->skip_suspend_operate = true;
    ts->skip_reset_in_resume = true;

    /*step7:create hx83102d related proc files*/
    himax_create_proc(ts, chip_info->syna_ops);
    irq_en_cnt = 1;
    TPD_INFO("%s, probe normal end\n", __func__);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
    if (ts->boot_mode == RECOVERY_BOOT)
#else
    if (ts->boot_mode == MSM_BOOT_MODE__RECOVERY)
#endif
    {
        enable_irq(chip_info->hx_irq);
        TPD_INFO("In Recovery mode, no-flash download fw by headfile\n");
        queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update,
                           msecs_to_jiffies(500));
    }
    if (is_oem_unlocked()) {
        TPD_INFO("Replace system image for cts, download fw by headfile\n");
        queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update,
                           msecs_to_jiffies(500));
    }

    return 0;
err_spi_setup:
err_register_driver:
    if (chip_info->hx_irq != 0)
        disable_irq_nosync(chip_info->hx_irq);

    common_touch_data_free(ts);
    ts = NULL;

    if (hx_touch_data)
        kfree(hx_touch_data);
    if (chip_info)
        kfree(chip_info);
    ret = -1;
    TPD_INFO("%s, probe error\n", __func__);

    return ret;
}

static int hx83102d_tp_remove(struct spi_device *spi)
{
    struct touchpanel_data *ts = spi_get_drvdata(spi);

    ts->s_client = NULL;
    /* spin_unlock_irq(&ts->spi_lock); */
    spi_set_drvdata(spi, NULL);

    TPD_INFO("%s is called\n", __func__);
    kfree(ts);

    return 0;
}

static int hx83102d_i2c_suspend(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);

    TPD_INFO("%s: is called gesture_enable =%d\n", __func__, ts->gesture_enable);
    tp_i2c_suspend(ts);

    return 0;
}

static int hx83102d_i2c_resume(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);

    TPD_INFO("%s is called\n", __func__);
    tp_i2c_resume(ts);

    /* if (ts->black_gesture_support) {
    	if (ts->gesture_enable == 1) {
    	    TPD_INFO("himax_set_SMWP_enable 1\n");
    	    himax_set_SMWP_enable(1, ts->is_suspended);
    	}
    }*/
    return 0;
}

static const struct spi_device_id tp_id[] = {
#ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
    { "oppo,tp_noflash", 0 },
#else
    { TPD_DEVICE, 0 },
#endif
    { }
};

static struct of_device_id tp_match_table[] = {
#ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
    { .compatible = "oppo,tp_noflash",},
#else
    { .compatible = TPD_DEVICE,},
#endif
    { },
};

static const struct dev_pm_ops tp_pm_ops = {
#ifdef CONFIG_FB
    .suspend = hx83102d_i2c_suspend,
    .resume = hx83102d_i2c_resume,
#endif
};


static struct spi_driver himax_common_driver = {
    .probe      = hx83102d_tp_probe,
    .remove     = hx83102d_tp_remove,
    .id_table   = tp_id,
    .driver = {
        .name = TPD_DEVICE,
        .owner = THIS_MODULE,
        .of_match_table = tp_match_table,
        .pm = &tp_pm_ops,
    },
};

static int __init tp_driver_init(void)
{
    int status = 0;

    TPD_INFO("%s is called\n", __func__);
    if (!tp_judge_ic_match(TPD_DEVICE)) {
        return -1;
    }

    get_lcd_vendor();
    get_oem_verified_boot_state();

    status = spi_register_driver(&himax_common_driver);
    if (status < 0) {
        TPD_INFO("%s, Failed to register SPI driver.\n", __func__);
        return -EINVAL;
    }

    return status;
}

/* should never be called */
static void __exit tp_driver_exit(void)
{
    spi_unregister_driver(&himax_common_driver);
    return;
}

module_init(tp_driver_init);
module_exit(tp_driver_exit);

MODULE_DESCRIPTION("Touchscreen Driver");
MODULE_LICENSE("GPL");
