/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ilitek.h"

#define DTS_OF_NAME    "tchip,ilitek"
#define DEVICE_ID        "ILITEK_TDDI"

#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
extern void disable_esd_thread(void);
#endif // end of CONFIG_FB_MSM
#endif

/* Debug level */
s32 ipio_debug_level = DEBUG_OUTPUT;
EXPORT_SYMBOL(ipio_debug_level);

struct ilitek_tddi_dev *idev;
extern int tp_register_times;


/*******Part1:spi  Function implement*******/
static int core_rx_lock_check(int *ret_size)
{
    int i, count = 1;
    u8 txbuf[5] = {SPI_WRITE, 0x25, 0x94, 0x0, 0x2};
    u8 rxbuf[4] = {0};
    u16 status = 0, lock = 0x5AA5;

    for (i = 0; i < count; i++) {
        txbuf[0] = SPI_WRITE;
        if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
            ipio_err("spi write (0x25,0x94,0x0,0x2) error\n");
            goto out;
        }

        txbuf[0] = SPI_READ;
        if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 4) < 0) {
            ipio_err("spi read error\n");
            goto out;
        }

        status = (rxbuf[2] << 8) + rxbuf[3];
        *ret_size = (rxbuf[0] << 8) + rxbuf[1];

        //TPD_DEBUG("Rx lock = 0x%x, size = %d\n", status, *ret_size);

        if (status == lock)
            return 0;

        mdelay(1);
    }

out:
    ipio_err("Rx check lock error, lock = 0x%x, size = %d\n", status, *ret_size);
    return -EIO;
}

static int core_tx_unlock_check(void)
{
    int i, count = 100;
    u8 txbuf[5] = {SPI_WRITE, 0x25, 0x0, 0x0, 0x2};
    u8 rxbuf[4] = {0};
    u16 status = 0, unlock = 0x9881;

    for (i = 0; i < count; i++) {
        txbuf[0] = SPI_WRITE;
        if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
            ipio_err("spi write (0x25,0x0,0x0,0x2) error\n");
            goto out;
        }

        txbuf[0] = SPI_READ;
        if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 4) < 0) {
            ipio_err("spi read error\n");
            goto out;
        }

        status = (rxbuf[2] << 8) + rxbuf[3];

        //TPD_DEBUG("Tx unlock = 0x%x\n", status);

        if (status == unlock)
            return 0;

        mdelay(1);
    }

out:
    ipio_err("Tx check unlock error, unlock = 0x%x\n", status);
    return -EIO;
}

static int core_spi_ice_mode_unlock_read(u8 *data, int size)
{
    int ret = 0;
    u8 txbuf[64] = { 0 };

    /* set read address */
    txbuf[0] = SPI_WRITE;
    txbuf[1] = 0x25;
    txbuf[2] = 0x98;
    txbuf[3] = 0x0;
    txbuf[4] = 0x2;
    if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
        TPD_INFO("spi write (0x25,0x98,0x00,0x2) error\n");
        ret = -EIO;
        return ret;
    }

    /* read data */
    txbuf[0] = SPI_READ;
    if (idev->spi_write_then_read(idev->spi, txbuf, 1, data, size) < 0) {
        ret = -EIO;
        return ret;
    }

    /* write data unlock */
    txbuf[0] = SPI_WRITE;
    txbuf[1] = 0x25;
    txbuf[2] = 0x94;
    txbuf[3] = 0x0;
    txbuf[4] = 0x2;
    txbuf[5] = (size & 0xFF00) >> 8;
    txbuf[6] = size & 0xFF;
    txbuf[7] = (char)0x98;
    txbuf[8] = (char)0x81;
    if (idev->spi_write_then_read(idev->spi, txbuf, 9, txbuf, 0) < 0) {
        ipio_err("spi write unlock (0x9881) error, ret = %d\n", ret);
        ret = -EIO;
    }
    return ret;
}

static int core_spi_ice_mode_lock_write(u8 *data, int size)
{
    int ret = 0;
    int safe_size = size;
    u8 check_sum = 0, wsize = 0;
    u8 *txbuf = NULL;

    txbuf = kcalloc(size + 9, sizeof(u8), GFP_KERNEL);
    if (ERR_ALLOC_MEM(txbuf)) {
        ipio_err("Failed to allocate txbuf\n");
        ret = -ENOMEM;
        goto out;
    }

    /* Write data */
    txbuf[0] = SPI_WRITE;
    txbuf[1] = 0x25;
    txbuf[2] = 0x4;
    txbuf[3] = 0x0;
    txbuf[4] = 0x2;

    /* Calcuate checsum and fill it in the last byte */
    check_sum = ilitek_calc_packet_checksum(data, size);
    ipio_memcpy(txbuf + 5, data, size, safe_size + 9);
    txbuf[5 + size] = check_sum;
    size++;
    wsize = size;
    if (wsize % 4 != 0)
        wsize += 4 - (wsize % 4);

    if (idev->spi_write_then_read(idev->spi, txbuf, wsize + 5, txbuf, 0) < 0) {
        TPD_INFO("spi write (0x25,0x4,0x00,0x2) error\n");
        ret = -EIO;
        goto out;
    }

    /* write data lock */
    txbuf[0] = SPI_WRITE;
    txbuf[1] = 0x25;
    txbuf[2] = 0x0;
    txbuf[3] = 0x0;
    txbuf[4] = 0x2;
    txbuf[5] = (size & 0xFF00) >> 8;
    txbuf[6] = size & 0xFF;
    txbuf[7] = (char)0x5A;
    txbuf[8] = (char)0xA5;
    if (idev->spi_write_then_read(idev->spi, txbuf, 9, txbuf, 0) < 0) {
        ipio_err("spi write lock (0x5AA5) error, ret = %d\n", ret);
        ret = -EIO;
    }

out:
    ipio_kfree((void **)&txbuf);
    return ret;
}

static int core_spi_ice_mode_disable(void)
{
    u8 txbuf[5] = {0x82, 0x1B, 0x62, 0x10, 0x18};

    if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
        ipio_err("spi write ice mode disable failed\n");
        return -EIO;
    }
    return 0;
}

static int core_spi_ice_mode_enable(void)
{
    u8 txbuf[5] = {0x82, 0x1F, 0x62, 0x10, 0x18};
    u8 rxbuf[2] = {0};

    if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 1) < 0) {
        ipio_err("spi write 0x82 error\n");
        return -EIO;
    }

    /* check recover data */
    if (rxbuf[0] != SPI_ACK) {
        ipio_err("Check SPI_ACK failed (0x%x)\n", rxbuf[0]);
        return DO_SPI_RECOVER;
    }

    if (idev->spi_write_then_read(idev->spi, txbuf, 5, rxbuf, 0) < 0) {
        ipio_err("spi write ice mode enable failed\n");
        return -EIO;
    }
    return 0;
}

static int core_spi_ice_mode_write(u8 *data, int len)
{
    int ret = 0;
    if (core_spi_ice_mode_enable() < 0)
        return -EIO;

    /* send data and change lock status to 0x5AA5. */
    ret = core_spi_ice_mode_lock_write(data, len);
    if (ret < 0)
        goto out;

    /*
     * Check FW if they already received the data we sent.
     * They change lock status from 0x5AA5 to 0x9881 if they did.
     */
    ret = core_tx_unlock_check();
    if (ret < 0)
        goto out;

out:
    if (core_spi_ice_mode_disable() < 0)
        return -EIO;

    return ret;
}

static int core_spi_ice_mode_read(u8 *data, int len)
{
    int size = 0, ret = 0;

    ret = core_spi_ice_mode_enable();
    if (ret < 0)
        return ret;

    /*
     * Check FW if they already send their data to rxbuf.
     * They change lock status from 0x9881 to 0x5AA5 if they did.
     */
    ret = core_rx_lock_check(&size);
    if (ret < 0)
        goto out;

    if (len < size) {
        TPD_INFO("WARRING! size(%d) > len(%d), use len to get data\n", size, len);
        size = len;
    }

    /* receive data from rxbuf and change lock status to 0x9881. */
    ret = core_spi_ice_mode_unlock_read(data, size);
    if (ret < 0)
        goto out;

out:
    if (core_spi_ice_mode_disable() < 0)
        return -EIO;

    if (ret >= 0)
        return size;

    return ret;
}

int core_spi_check_read_size(void)
{
    int size = 0, ret = 0;

    ret = core_spi_ice_mode_enable();
    if (ret < 0)
        return ret;

    /*
     * Check FW if they already send their data to rxbuf.
     * They change lock status from 0x9881 to 0x5AA5 if they did.
     */
    ret = core_rx_lock_check(&size);
    if (ret < 0)
        goto out;
    return size;
out:
    core_spi_ice_mode_disable();
    return -EIO;
}

int core_spi_read_data_after_checksize(uint8_t *data, int size)
{
    int ret = 0;
    ret = core_spi_ice_mode_unlock_read(data, size);
    if (ret < 0)
        goto out;

out:
    if (core_spi_ice_mode_disable() < 0)
        return -EIO;
    if (ret < 0) {
        return ret;
    }
    return size;
}

static int core_spi_write(u8 *data, int len)
{
    int ret = 0, count = 2;
    u8 *txbuf = NULL;
    int safe_size = len;

    if (atomic_read(&idev->ice_stat) == DISABLE) {
        do {
            ret = core_spi_ice_mode_write(data, len);
            if (ret >= 0)
                break;
        } while (--count > 0);
        goto out;
    }

    txbuf = kcalloc(len + 1, sizeof(u8), GFP_KERNEL);
    if (ERR_ALLOC_MEM(txbuf)) {
        ipio_err("Failed to allocate txbuf\n");
        return -ENOMEM;
    }

    txbuf[0] = SPI_WRITE;
    ipio_memcpy(txbuf + 1, data, len, safe_size + 1);

    if (idev->spi_write_then_read(idev->spi, txbuf, len + 1, txbuf, 0) < 0) {
        ipio_err("spi write data error in ice mode\n");
        ret = -EIO;
        goto out;
    }

out:
    ipio_kfree((void **)&txbuf);
    return ret;
}

static int core_spi_read(u8 *rxbuf, int len)
{
    int ret = 0, count = 2;
    u8 txbuf[1] = {0};

    txbuf[0] = SPI_READ;

    if (atomic_read(&idev->ice_stat) == DISABLE) {
        do {
            ret = core_spi_ice_mode_read(rxbuf, len);
            if (ret >= 0)
                break;
        } while (--count > 0);
        goto out;
    }

    if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, len) < 0) {
        ipio_err("spi read data error in ice mode\n");
        ret = -EIO;
        goto out;
    }

out:
    return ret;
}

static int ilitek_spi_write(void *buf, int len)
{
    int ret = 0;

    if (!len) {
        ipio_err("spi write len is invaild\n");
        return -EINVAL;
    }

    ret = core_spi_write(buf, len);
    if (ret < 0) {
        if (atomic_read(&idev->tp_reset) == START) {
            ret = 0;
            goto out;
        }
        ipio_err("spi write error, ret = %d\n", ret);
    }

out:
    return ret;
}

/* If ilitek_spi_read success ,this function will return read length */
static int ilitek_spi_read(void *buf, int len)
{
    int ret = 0;

    if (!len) {
        ipio_err("spi read len is invaild\n");
        return -EINVAL;
    }

    ret = core_spi_read(buf, len);
    if (ret < 0) {
        if (atomic_read(&idev->tp_reset) == START) {
            ret = 0;
            goto out;
        }
        ipio_err("spi read error, ret = %d\n", ret);
    }

out:
    return ret;
}

static int core_spi_setup(u32 freq)
{
    TPD_INFO("spi clock = %d\n", freq);

    idev->spi->mode = SPI_MODE_0;
    idev->spi->bits_per_word = 8;
    if (idev->spi->max_speed_hz == 0) {
        idev->spi->max_speed_hz = freq;
    }
    idev->spi->chip_select = 0; //modify reg=0 for more tp vendor share same spi interface

    if (spi_setup(idev->spi) < 0) {
        ipio_err("Failed to setup spi device\n");
        return -ENODEV;
    }

    TPD_INFO("name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n",
             idev->spi->modalias,
             idev->spi->master->bus_num,
             idev->spi->chip_select,
             idev->spi->mode,
             idev->spi->max_speed_hz);
    return 0;
}

/*******Part2:comom Function implement*******/

void ilitek_plat_tp_reset(void)
{
    TPD_INFO("edge delay = %d\n", idev->rst_edge_delay);
    gpio_direction_output(idev->hw_res->reset_gpio, 1);
    mdelay(1);
    gpio_set_value(idev->hw_res->reset_gpio, 0);
    mdelay(5);
    gpio_set_value(idev->hw_res->reset_gpio, 1);
    mdelay(idev->rst_edge_delay);
}

void ilitek_plat_irq_disable(void)
{
    unsigned long flag;

    spin_lock_irqsave(&idev->irq_spin, flag);

    if (atomic_read(&idev->irq_stat) == DISABLE)
        goto out;

    if (!idev->irq_num) {
        ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
        goto out;
    }

    disable_irq_nosync(idev->irq_num);
    atomic_set(&idev->irq_stat, DISABLE);
    TPD_DEBUG("Disable irq success\n");

out:
    spin_unlock_irqrestore(&idev->irq_spin, flag);
}

void ilitek_plat_irq_enable(void)
{
    unsigned long flag;

    spin_lock_irqsave(&idev->irq_spin, flag);

    if (atomic_read(&idev->irq_stat) == ENABLE)
        goto out;

    if (!idev->irq_num) {
        ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
        goto out;
    }

    enable_irq(idev->irq_num);
    atomic_set(&idev->irq_stat, ENABLE);
    TPD_DEBUG("Enable irq success\n");

out:
    spin_unlock_irqrestore(&idev->irq_spin, flag);
}

/*******Part3:main flow  Function implement*******/

int ilitek_tddi_mp_test_handler(char *apk, struct seq_file *s, char *message, bool lcm_on)
{
    int ret = 0;
    u8 tp_mode = P5_X_FW_TEST_MODE;

    if (atomic_read(&idev->fw_stat)) {
        ipio_err("fw upgrade processing, ignore\n");
        if (!ERR_ALLOC_MEM(message)) {
            sprintf(message, "fw upgrade processing, ignore\n");
        }
        if (!ERR_ALLOC_MEM(s)) {
            seq_printf(s, "fw upgrade processing, ignore\n");
        }
        return 0;
    }

    if (!idev->chip->open_c_formula ||
        !idev->chip->open_sp_formula) {
        ipio_err("formula is null\n");
        if (!ERR_ALLOC_MEM(message)) {
            sprintf(message, "formula is null\n");
        }
        if (!ERR_ALLOC_MEM(s)) {
            seq_printf(s, "formula is null\n");
        }
        return -1;
    }

    idev->esd_check_enabled = false;
    mutex_lock(&idev->touch_mutex);
    atomic_set(&idev->mp_stat, ENABLE);

    if (idev->actual_tp_mode != P5_X_FW_TEST_MODE) {
        if (ilitek_tddi_switch_mode(&tp_mode) < 0) {
            if (!ERR_ALLOC_MEM(message)) {
                sprintf(message, "switch test mode failed\n");
            }
            if (!ERR_ALLOC_MEM(s)) {
                seq_printf(s, "switch test mode failed\n");
            }
            goto out;
        }
    }

    ret = ilitek_tddi_mp_test_main(apk, s, message, lcm_on);

out:
    /* Set tp as demo mode and reload code if it's iram. */
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    if (lcm_on) {
        if (idev->fw_upgrade_mode == UPGRADE_IRAM)
            ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
        else
            ilitek_tddi_reset_ctrl(idev->reset);
    }

    atomic_set(&idev->mp_stat, DISABLE);
    mutex_unlock(&idev->touch_mutex);
    idev->esd_check_enabled = true;
    return ret;
}

int ilitek_tddi_switch_mode(u8 *data)
{
    int ret = 0, mode;
    u8 cmd[4] = {0};

    if (!data) {
        ipio_err("data is null\n");
        return -EINVAL;
    }

    atomic_set(&idev->tp_sw_mode, START);

    mode = data[0];
    idev->actual_tp_mode = mode;

    switch (idev->actual_tp_mode) {
    case P5_X_FW_I2CUART_MODE:
        TPD_INFO("Not implemented yet\n");
        break;
    case P5_X_FW_DEMO_MODE:
        TPD_INFO("Switch to Demo mode\n");
        cmd[0] = P5_X_MODE_CONTROL;
        cmd[1] = mode;
        ret = idev->write(cmd, 2);
        if (ret < 0) {
            msleep(100);
            ret = idev->write(cmd, 2);
            if (ret < 0) {
                ipio_err("Failed to switch demo mode, do reset/reload instead\n");
                if (idev->fw_upgrade_mode == UPGRADE_IRAM)
                    ret = ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
                else
                    ret = ilitek_tddi_reset_ctrl(idev->reset);
            }
        }
        break;
    case P5_X_FW_DEBUG_MODE:
        TPD_INFO("Switch to Debug mode\n");
        cmd[0] = P5_X_MODE_CONTROL;
        cmd[1] = mode;
        ret = idev->write(cmd, 2);
        if (ret < 0) {
            msleep(100);
            ret = idev->write(cmd, 2);
            if (ret < 0) {
                ipio_err("Failed to switch Debug mode\n");
            }
        }
        break;
    case P5_X_FW_GESTURE_MODE:
        TPD_INFO("Switch to Gesture mode, lpwg cmd = %d\n",  idev->gesture_mode);
        ret = ilitek_tddi_ic_func_ctrl("lpwg", idev->gesture_mode);
        break;
    case P5_X_FW_TEST_MODE:
        TPD_INFO("Switch to Test mode\n");
        ret = idev->mp_move_code();
        break;
    case P5_X_FW_DEMO_DEBUG_INFO_MODE:
        TPD_INFO("Switch to demo debug info mode\n");
        cmd[0] = P5_X_MODE_CONTROL;
        cmd[1] = mode;
        ret = idev->write(cmd, 2);
        if (ret < 0)
            ipio_err("Failed to switch debug info mode\n");
        break;
    case P5_X_FW_SOP_FLOW_MODE:
        TPD_INFO("Not implemented SOP flow mode yet\n");
        break;
    case P5_X_FW_ESD_MODE:
        TPD_INFO("Not implemented ESD mode yet\n");
        break;
    default:
        ipio_err("Unknown TP mode: %x\n", mode);
        ret = -1;
        break;
    }

    if (ret < 0)
        ipio_err("Switch mode failed\n");

    TPD_DEBUG("Actual TP mode = %d\n", idev->actual_tp_mode);
    atomic_set(&idev->tp_sw_mode, END);
    return ret;
}

void ilitek_tddi_wq_ges_recover(void)
{
    atomic_set(&idev->esd_stat, START);
    idev->ges_recover();
    atomic_set(&idev->esd_stat, END);
}

void ilitek_tddi_wq_spi_recover(void)
{
    idev->esd_check_enabled = false;
    atomic_set(&idev->esd_stat, START);
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
    atomic_set(&idev->esd_stat, END);
    idev->esd_check_enabled = true;
}

int ilitek_tddi_wq_esd_spi_check(void)
{
    int ret = 0;
    u8 tx = SPI_WRITE;
    u8 rx = 0;
    idev->spi_write_then_read(idev->spi, &tx, 1, &rx, 1);
    TPD_DEBUG("spi esd check = 0x%x\n", rx);
    if (rx != SPI_ACK) {
        ipio_err("rx = 0x%x\n", rx);
        ret = -1;
    }
    return ret;
}

void ilitek_tddi_wq_esd_check(void)
{
    if (idev->esd_check() < 0) {
        ipio_err("SPI ACK failed, doing spi recovery\n");
        tp_touch_btnkey_release();
        ilitek_tddi_wq_spi_recover();
        return;
    }
}

int ilitek_tddi_sleep_handler(int mode)
{
    int ret = 0;

    atomic_set(&idev->tp_sleep, START);

    if (atomic_read(&idev->fw_stat) ||
        atomic_read(&idev->mp_stat)) {
        TPD_INFO("fw upgrade or mp still running, ignore sleep requst\n");
        atomic_set(&idev->tp_sleep, END);
        return 0;
    }

    TPD_INFO("Sleep Mode = %d\n", mode);
    idev->esd_check_enabled = false;
    if (!idev->gesture) {
        ilitek_plat_irq_disable();
    }

    switch (mode) {
    case TP_SUSPEND:
        TPD_INFO("TP normal suspend start\n");
        ilitek_tddi_ic_func_ctrl("sense", DISABLE);
        ilitek_tddi_ic_check_busy(5, 35);

        if (idev->gesture) {
            ret = idev->gesture_move_code(idev->gesture_mode);
            if (ret < 0) {
                atomic_set(&idev->esd_stat, START);
                idev->ges_recover();
                atomic_set(&idev->esd_stat, END);
            }
            enable_irq_wake(idev->irq_num);
            //ilitek_plat_irq_enable();
        } else {
            ilitek_tddi_ic_func_ctrl("sleep", DEEP_SLEEP_IN);
        }
        msleep(35);
        TPD_INFO("TP normal suspend end\n");
        break;
    case TP_DEEP_SLEEP:
        TPD_INFO("TP deep suspend start\n");
        ilitek_tddi_ic_func_ctrl("sense", DISABLE);
        ilitek_tddi_ic_check_busy(5, 50);
        ilitek_tddi_ic_func_ctrl("sleep", DEEP_SLEEP_IN);
        msleep(35);
        TPD_INFO("TP deep suspend end\n");
        break;
    case TP_RESUME:
        TPD_INFO("TP resume start\n");
        if (idev->gesture)
            disable_irq_wake(idev->irq_num);

        /* Set tp as demo mode and reload code if it's iram. */
        idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
        if (idev->fw_upgrade_mode == UPGRADE_IRAM)
            ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
        else
            ilitek_tddi_reset_ctrl(idev->reset);
        ilitek_plat_irq_enable();
        TPD_INFO("TP resume end\n");
        idev->esd_check_enabled = true;
        break;
    default:
        ipio_err("Unknown sleep mode, %d\n", mode);
        ret = -EINVAL;
        break;
    }

    tp_touch_btnkey_release();
    atomic_set(&idev->tp_sleep, END);
    return ret;
}

int ilitek_tddi_get_tp_recore_data(u16 *out_buf, u32 out_len)
{
    u8 buf[8] = {0};
    u8 record_case = 0;
    s8 index;
    u16 *raw = NULL;
    u16 *raw_ptr = NULL;
    u16 frame_len;
    u32 base_addr = 0x20000;
    u32 addr;
    u32 len;
    u32 *ptr;
    u32 i;
    u32 j;
    u8 frame_cnt;
    int ret = 0;
    bool ice = atomic_read(&idev->ice_stat);
    struct record_state record_stat;

    if (idev->read(buf, sizeof(buf)) < 0) {
        ipio_err("Get info fail\n");
        return -1;
    }
    addr = ((buf[0] << 8) | buf[1]) + base_addr;
    len = ((buf[2] << 8) | buf[3]);
    index = buf[4];
    frame_cnt = buf[5];
    record_case = buf[6];
    ipio_memcpy(&record_stat, &buf[7], 1, 1);
    TPD_INFO("addr = 0x%x, len = %d, lndex = %d, frame_cnt = %d, record_case = 0x%x\n",
             addr, len, index, frame_cnt, record_case);
    ilitek_dump_data(buf, 8, sizeof(buf), 0, "all record bytes");

    raw = kcalloc(len, sizeof(u8), GFP_ATOMIC);

    if (ERR_ALLOC_MEM(raw)) {
        ipio_err("Failed to allocate packet memory, %ld\n", PTR_ERR(raw));
        return -1;
    }
    ptr = (u32 *)raw;

    if (!ice)
        ilitek_ice_mode_ctrl(ENABLE, ON);

    for (i = 0, j = 0; i < len; i += 4, j++) {
        ilitek_ice_mode_read((addr + i), &ptr[j], sizeof(u32));
    }
    if (frame_cnt > 0 && index >= 0 && index < 3) {
        frame_len = (len / (frame_cnt * 2));
        if (out_buf) {
            if (out_len > frame_len * 2) {
                TPD_INFO("out_len is %d, frame_len is %d\n", out_len, frame_len);
                out_len = frame_len * 2;
            }
            if (out_len > 0) {
                memcpy(out_buf, &raw[index * frame_len], out_len);
            }

        }
        for (i = 0; i < frame_cnt; i ++) {
            raw_ptr = raw + (index * frame_len);

            ilitek_dump_data(raw_ptr, 16, frame_len, idev->xch_num, "recore_data");
            index--;
            if(index < 0)
                index = frame_cnt - 1;
        }
    } else {
        ret = -1;
    }



    if (!ice)
        ilitek_ice_mode_ctrl(DISABLE, ON);

    if (record_case == 2) {
        TPD_INFO("tp_palm_stat = %d\n", record_stat.touch_palm_state_e);
        TPD_INFO("app_an_stat = %d\n", record_stat.app_an_statu_e);
        TPD_INFO("app_check_abnor = %d\n", record_stat.app_sys_check_bg_abnormal);
        TPD_INFO("wrong_bg = %d\n", record_stat.g_b_wrong_bg);
    }

    //ipio_kfree((void **)&raw);

    return ret;
}

int ilitek_tddi_get_tp_recore_ctrl(int data)
{
    int ret = 0;
    u16 *buf = NULL;

    switch((int)data) {
    case 0:
        TPD_INFO("recore disable");
        ret = ilitek_tddi_ic_func_ctrl("tp_recore", 0);
        break;
    case 1:
        TPD_INFO("recore enable");
        ret = ilitek_tddi_ic_func_ctrl("tp_recore", 1);
        mdelay(200);
        break;
    case 2:
        mdelay(50);
        TPD_INFO("Get data");
        ret = ilitek_tddi_ic_func_ctrl("tp_recore", 2);
        if (ret < 0) {
            ipio_err("cmd fail\n");
            goto out;
        }

        ret = ilitek_tddi_get_tp_recore_data(buf, 0);
        if (buf) {
            kfree(buf);
        }
        if (ret < 0)
            ipio_err("get data fail\n");

        TPD_INFO("recore reset");
        ret = ilitek_tddi_ic_func_ctrl("tp_recore", 3);
        if (ret < 0) {
            ipio_err("cmd fail\n");
            goto out;
        }
        break;
    default:
        ipio_err("Unknown get_tp_recore_ctrl case, %d\n", data);
    }
out:
    return ret;

}

int ilitek_tddi_report_handler(void)
{
    int ret = 0, pid = 0;
    u8 *buf = NULL, *data_buf = NULL, checksum = 0;
    int rlen = 0;
    int tmp = ipio_debug_level;

    /* Just in case these stats couldn't be blocked in top half context */
    if (!idev->report || atomic_read(&idev->tp_reset) ||
        atomic_read(&idev->fw_stat) || atomic_read(&idev->tp_sw_mode) ||
        atomic_read(&idev->mp_stat) || atomic_read(&idev->tp_sleep)) {
        TPD_INFO("ignore report request\n");
        return -1;
    }
    rlen = idev->spi_check_read_size();
    TPD_DEBUG("Packget length = %d\n", rlen);

    if (rlen < 0) {
        ipio_err("Length of packet is invaild\n");
        if (rlen == DO_SPI_RECOVER && idev->actual_tp_mode == P5_X_FW_GESTURE_MODE && idev->gesture) {
            ipio_err("Gesture failed, doing gesture recovery\n");
            tp_touch_btnkey_release();
            ilitek_tddi_wq_ges_recover();
            goto out;
        } else if (rlen == DO_SPI_RECOVER) {
            ipio_err("SPI ACK failed, doing spi recovery\n");
            tp_touch_btnkey_release();
            ilitek_tddi_wq_spi_recover();
            goto out;
        }
        ret = -1;
        goto out;
    }

    buf = kcalloc(rlen, sizeof(u8), GFP_ATOMIC);
    if (ERR_ALLOC_MEM(buf)) {
        ipio_err("Failed to allocate packet memory, %ld\n", PTR_ERR(buf));
        return -1;
    }
    ret = idev->spi_read_after_check_size(buf, rlen);
    if (ret < 0) {
        ipio_err("Read report packet failed ret = %d\n", ret);
        goto out;
    }

    TPD_DEBUG("Read length = %d\n", (ret));

    rlen = ret;

    ilitek_dump_data(buf, 8, rlen, 0, "finger report");

    checksum = ilitek_calc_packet_checksum(buf, rlen - 1);
    if (checksum != buf[rlen - 1]) {
        ret = -1;
        ipio_err("Wrong checksum, checksum = %x, buf = %x\n", checksum, buf[rlen - 1]);
        ipio_debug_level = 1;
        ilitek_dump_data(buf, 8, rlen, 0, "finger report");
        ipio_debug_level = tmp;
        if (P5_X_I2CUART_PACKET_ID != buf[0]) {
            goto out;
        }
    }

    pid = buf[0];
    data_buf = buf;
    if (pid == P5_X_INFO_HEADER_PACKET_ID) {
        TPD_DEBUG("Have header PID = %x\n", pid);
        data_buf = buf + P5_X_INFO_HEADER_LENGTH;
        pid = data_buf[0];
    }
    TPD_DEBUG("Packet ID = %x\n", pid);

    switch (pid) {
    case P5_X_DEMO_PACKET_ID:
        if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH) {
            ipio_err("read ap mode data len < 43 len = %d\n", rlen);
            ret = -1;
        } else {
            ilitek_tddi_report_ap_mode(buf, rlen);
        }
        break;
    case P5_X_DEBUG_PACKET_ID:
        if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH) {
            ipio_err("read debug mode data len < 43 len = %d\n", rlen);
            ret = -1;
        } else {
            ilitek_tddi_report_debug_mode(buf, rlen);
        }
        break;
    case P5_X_I2CUART_PACKET_ID:
        ret = -1;
        ilitek_tddi_report_i2cuart_mode(buf, rlen);
        break;
    case P5_X_GESTURE_PACKET_ID:
    case P5_X_GESTURE_FAIL_ID:
        ilitek_tddi_report_gesture_mode(buf, rlen);
        break;
    case P5_X_DEMO_DEBUG_INFO_PACKET_ID:
        if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH + 2) {
            ipio_err("read demo debug info mode data len < 45 len = %d\n", rlen);
            ret = -1;
        } else {
            ilitek_tddi_demo_debug_info_mode(data_buf, rlen);
        }
        break;
    default:
        ipio_err("Unknown packet id, %x\n", pid);
        break;
    }

out:
    ipio_kfree((void **)&buf);
    return ret;
}

int ilitek_tddi_reset_ctrl(int mode)
{
    int ret = 0;

    atomic_set(&idev->tp_reset, START);

    if (mode != TP_IC_CODE_RST)
        ilitek_tddi_ic_check_otp_prog_mode();

    switch (mode) {
    case TP_IC_CODE_RST:
        TPD_INFO("TP IC Code RST \n");
        ret = ilitek_tddi_ic_code_reset();
        break;
    case TP_IC_WHOLE_RST:
        TPD_INFO("TP IC whole RST\n");
        ret = ilitek_tddi_ic_whole_reset();
        break;
    case TP_HW_RST_ONLY:
        TPD_INFO("TP HW RST\n");
        ilitek_plat_tp_reset();
        break;
    default:
        ipio_err("Unknown reset mode, %d\n", mode);
        ret = -EINVAL;
        break;
    }

    /*
     * Since OTP must be folloing with reset, except for code rest,
     * the stat of ice mode should be set as 0.
     */
    if (mode != TP_IC_CODE_RST)
        atomic_set(&idev->ice_stat, DISABLE);

    atomic_set(&idev->tp_reset, END);
    return ret;
}


/*******Part3:irq hander Function implement*******/

void ilitek_dump_data(void *data, int type, int len, int row_len, const char *name)
{
    int i, row = 31;
    u8 *p8 = NULL;
    s32 *p32 = NULL;
    s16 *p16 = NULL;

    if (row_len > 0)
        row = row_len;

    if (ipio_debug_level) {
        if (data == NULL) {
            ipio_err("The data going to dump is NULL\n");
            return;
        }

        pr_cont("\n\n");
        pr_cont("ILITEK: Dump %s data\n", name);
        pr_cont("ILITEK: ");

        if (type == 8)
            p8 = (u8 *) data;
        if (type == 32 || type == 10)
            p32 = (s32 *) data;
        if (type == 16)
            p16 = (s16 *) data;

        for (i = 0; i < len; i++) {
            if (type == 8)
                pr_cont(" %4x ", p8[i]);
            else if (type == 32)
                pr_cont(" %4x ", p32[i]);
            else if (type == 10)
                pr_cont(" %4d ", p32[i]);
            else if (type == 16)
                pr_cont(" %5d ", p16[i]);
            if ((i % row) == row - 1) {
                pr_cont("\n");
                pr_cont("ILITEK: ");
            }
        }
        pr_cont("\n\n");
    }
}

int ilitek_tddi_move_mp_code_iram(void)
{
    TPD_INFO("Download MP code to iram\n");
    return ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
}

int ilitek_tddi_move_gesture_code_iram(int mode)
{
    int i;
    u8 tp_mode = P5_X_FW_GESTURE_MODE;
    u8 cmd[3] = {0};
    int retry = 10;
    TPD_INFO();

    if (ilitek_tddi_ic_func_ctrl("lpwg", 0x3) < 0)
        ipio_err("write gesture flag failed\n");

    ilitek_tddi_switch_mode(&tp_mode);

    for (i = 0; i < retry; i++) {
        /* Prepare Check Ready */
        cmd[0] = P5_X_READ_DATA_CTRL;
        cmd[1] = 0xA;
        cmd[2] = 0x5;
        idev->write(cmd, 2);

        mdelay(10);

        /* Check ready for load code */
        cmd[0] = 0x1;
        cmd[1] = 0xA;
        cmd[2] = 0x5;
        if ((idev->write(cmd, 3)) < 0)
            ipio_err("write 0x1,0xA,0x5 error");

        if ((idev->read(cmd, 1)) < 0)
            ipio_err("read gesture ready byte error\n");

        TPD_DEBUG("gesture ready byte = 0x%x\n", cmd[0]);
        if (cmd[0] == 0x91) {
            TPD_INFO("Gesture check fw ready\n");
            break;
        }
    }

    if (i >= retry) {
        ipio_err("Gesture is not ready, 0x%x running gesture esd flow\n", cmd[0]);
        return -1;
    }

    ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);

    /* FW star run gestrue code cmd */
    cmd[0] = 0x1;
    cmd[1] = 0xA;
    cmd[2] = 0x6;
    if ((idev->write(cmd, 3)) < 0)
        ipio_err("write 0x1,0xA,0x6 error");
    return 0;
}

void ilitek_set_gesture_fail_reason(bool enable)
{

    u8 cmd[24] = {0};

    /* set symbol */
    if (ilitek_tddi_ic_func_ctrl("knock_en", 0x8) < 0)
        ipio_err("set symbol failed");

    /* enable gesture fail reason */
    cmd[0] = 0x01;
    cmd[1] = 0x0A;
    cmd[2] = 0x10;
    if (enable)
        cmd[3] = 0x01;
    else
        cmd[3] = 0x00;
    cmd[4] = 0xFF;
    cmd[5] = 0xFF;
    if ((idev->write(cmd, 6)) < 0)
        ipio_err("enable gesture fail reason failed");

    /* set gesture parameters */
    cmd[0] = 0x01;
    cmd[1] = 0x0A;
    cmd[2] = 0x12;
    cmd[3] = 0x01;
    memset(cmd + 4, 0xFF, 20);
    if ((idev->write(cmd, 24)) < 0)
        ipio_err("set gesture parameters failed");

    /* get gesture parameters */
    cmd[0] = 0x01;
    cmd[1] = 0x0A;
    cmd[2] = 0x11;
    cmd[3] = 0x01;
    if ((idev->write(cmd, 4)) < 0)
        ipio_err("get gesture parameters failed");

}

u8 ilitek_calc_packet_checksum(u8 *packet, int len)
{
    int i;
    s32 sum = 0;

    for (i = 0; i < len; i++)
        sum += packet[i];

    return (u8) ((-sum) & 0xFF);
}

void ilitek_tddi_touch_esd_gesture_iram(void)
{
    int retry = 50;
    u32 answer = 0;

    /* start to download AP code with host download */
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);

    ilitek_ice_mode_ctrl(ENABLE, OFF);

    TPD_INFO("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
             SPI_ESD_GESTURE_PWD_ADDR, SPI_ESD_GESTURE_RUN);

    /* write a special password to inform FW go back into gesture mode */
    if (ilitek_ice_mode_write(SPI_ESD_GESTURE_PWD_ADDR, ESD_GESTURE_PWD, 4) < 0)
        ipio_err("write password failed\n");

    /* Host download gives effect to FW receives password successed */
    ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
    /* waiting for FW reloading code */
    msleep(100);

    ilitek_ice_mode_ctrl(ENABLE, ON);

    /* polling another specific register to see if gesutre is enabled properly */
    do {
        ilitek_ice_mode_read(SPI_ESD_GESTURE_PWD_ADDR, &answer, sizeof(u32));
        if (answer != SPI_ESD_GESTURE_RUN)
            TPD_INFO("answer = 0x%x != (0x%x)\n", answer, SPI_ESD_GESTURE_RUN);
        msleep(10);
    } while (answer != SPI_ESD_GESTURE_RUN && --retry > 0);

    if (retry <= 0)
        ipio_err("Enter gesture failed\n");
    else
        TPD_INFO("Enter gesture successfully\n");

    ilitek_ice_mode_ctrl(DISABLE, ON);

    idev->gesture_move_code(idev->gesture_mode);
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
    if (!idev->netlink && !idev->debug_node_open)
        return;

    mutex_lock(&idev->debug_mutex);

    /* Send data to netlink */
    if (idev->netlink) {
        netlink_reply_msg(buf, len);
        goto out;
    }

    /* Sending data to apk via the node of debug_message node */
    if (idev->debug_node_open && !(ERR_ALLOC_MEM(idev->debug_buf))
        && !(ERR_ALLOC_MEM(idev->debug_buf[idev->debug_data_frame]))) {
        memset(idev->debug_buf[idev->debug_data_frame], 0x00, (u8)sizeof(u8) * DEBUG_DATA_MAX_LENGTH);
        ipio_memcpy(idev->debug_buf[idev->debug_data_frame], buf, len, DEBUG_DATA_MAX_LENGTH);
        idev->debug_data_frame++;
        if (idev->debug_data_frame > 1)
            TPD_DEBUG("idev->debug_data_frame = %d\n", idev->debug_data_frame);
        if (idev->debug_data_frame > (DEBUG_DATA_SAVE_MAX_FRAME - 1)) {
            ipio_err("idev->debug_data_frame = %d > 1023\n",
                     idev->debug_data_frame);
            idev->debug_data_frame = (DEBUG_DATA_SAVE_MAX_FRAME - 1);
        }
        wake_up(&(idev->inq));
        goto out;
    }

out:
    mutex_unlock(&idev->debug_mutex);
}


void ilitek_tddi_report_ap_mode(u8 *buf, int len)
{
    int i = 0;
    u32 xop = 0, yop = 0;

    for (i = 0; i < MAX_TOUCH_NUM; i++) {
        if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF)
            && (buf[(4 * i) + 3] == 0xFF)) {
            continue;
        }

        xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
        yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));
        TPD_DEBUG("original x = %d, y = %d\n", xop, yop);
        idev->pointid_info = idev->pointid_info | (1 << i);
        idev->points[i].x = xop * idev->panel_wid / TPD_WIDTH;
        idev->points[i].y = yop * idev->panel_hei / TPD_HEIGHT;
        idev->points[i].z = buf[(4 * i) + 4];
        idev->points[i].width_major = buf[(4 * i) + 4];
        idev->points[i].touch_major = buf[(4 * i) + 4];
        idev->points[i].status = 1;
        TPD_DEBUG("scale x = %d, y = %d, p = %d\n",
                  idev->points[i].x, idev->points[i].y, idev->points[i].z);
    }

    ilitek_tddi_touch_send_debug_data(buf, len);
}

void ilitek_tddi_report_debug_mode(u8 *buf, int len)
{
    int i = 0;
    u32 xop = 0, yop = 0;
    for (i = 0; i < MAX_TOUCH_NUM; i++) {
        if ((buf[(3 * i) + 5] == 0xFF) && (buf[(3 * i) + 6] == 0xFF)
            && (buf[(3 * i) + 7] == 0xFF)) {
            continue;
        }

        xop = (((buf[(3 * i) + 5] & 0xF0) << 4) | (buf[(3 * i) + 6]));
        yop = (((buf[(3 * i) + 5] & 0x0F) << 8) | (buf[(3 * i) + 7]));
        TPD_DEBUG("original x = %d, y = %d\n", xop, yop);
        idev->pointid_info = idev->pointid_info | (1 << i);
        idev->points[i].x = xop * idev->panel_wid / TPD_WIDTH;
        idev->points[i].y = yop * idev->panel_hei / TPD_HEIGHT;
        idev->points[i].z = buf[(4 * i) + 4];
        idev->points[i].width_major = buf[(4 * i) + 4];
        idev->points[i].touch_major = buf[(4 * i) + 4];
        idev->points[i].status = 1;
        TPD_DEBUG("scale x = %d, y = %d, p = %d\n",
                  idev->points[i].x, idev->points[i].y, idev->points[i].z);
    }

    ilitek_tddi_touch_send_debug_data(buf, len);
}

#ifdef CONFIG_OPPO_TP_APK
static void ili_write_log_buf(u8 main_id, u8 sec_id)
{
    log_buf_write(idev->ts, main_id);
    sec_id = sec_id | 0x80;
    log_buf_write(idev->ts, sec_id);
}


void ilitek_tddi_demo_debug_info_id0(u8 *buf, int len)
{
    static struct demo_debug_info_id0 id_last;
    struct demo_debug_info_id0 id0;
    TPD_INFO("id0 len = %d,strucy len = %ld", (int)len, sizeof(id0));

    ipio_memcpy(&id0, buf, sizeof(id0), len);

    if (id_last.app_sys_powr_state_e != id0.app_sys_powr_state_e) {
        ili_write_log_buf(1, id0.app_sys_powr_state_e);

    }
    if (id_last.app_sys_state_e != id0.app_sys_state_e) {
        ili_write_log_buf(2, id0.app_sys_state_e);

    }
    if (id_last.tp_state_e != id0.tp_state_e) {
        ili_write_log_buf(3, id0.tp_state_e);
    }
    if (id_last.touch_palm_state_e != id0.touch_palm_state_e) {
        ili_write_log_buf(4, id0.touch_palm_state_e);
    }
    if (id_last.app_an_statu_e != id0.app_an_statu_e) {
        ili_write_log_buf(5, id0.app_an_statu_e);
    }
    if (id_last.app_sys_check_bg_abnormal != id0.app_sys_check_bg_abnormal) {
        ili_write_log_buf(6, id0.app_sys_check_bg_abnormal);
    }
    if (id_last.g_b_wrong_bg != id0.g_b_wrong_bg) {
        ili_write_log_buf(7, id0.g_b_wrong_bg);
    }
    if (id_last.reserved0 != id0.reserved0) {
        ili_write_log_buf(8, id0.reserved0);
    }
    if (id_last.status_of_dynamic_th_e != id0.status_of_dynamic_th_e) {
        ili_write_log_buf(9, id0.status_of_dynamic_th_e);
    }
    if (id_last.reserved1 != id0.reserved1) {
        ili_write_log_buf(10, id0.reserved1);
    }
    if (id_last.algo_pt_status0 != id0.algo_pt_status0) {
        ili_write_log_buf(11, id0.algo_pt_status0);
    }
    if (id_last.algo_pt_status1 != id0.algo_pt_status1) {
        ili_write_log_buf(12, id0.algo_pt_status1);
    }
    if (id_last.algo_pt_status2 != id0.algo_pt_status2) {
        ili_write_log_buf(13, id0.algo_pt_status2);
    }
    if (id_last.algo_pt_status3 != id0.algo_pt_status3) {
        ili_write_log_buf(14, id0.algo_pt_status3);
    }
    if (id_last.algo_pt_status4 != id0.algo_pt_status4) {
        ili_write_log_buf(15, id0.algo_pt_status4);
    }
    if (id_last.algo_pt_status5 != id0.algo_pt_status5) {
        ili_write_log_buf(16, id0.algo_pt_status5);
    }
    if (id_last.algo_pt_status6 != id0.algo_pt_status6) {
        ili_write_log_buf(17, id0.algo_pt_status6);
    }
    if (id_last.algo_pt_status7 != id0.algo_pt_status7) {
        ili_write_log_buf(18, id0.algo_pt_status7);
    }
    if (id_last.algo_pt_status8 != id0.algo_pt_status8) {
        ili_write_log_buf(19, id0.algo_pt_status8);
    }
    if (id_last.algo_pt_status9 != id0.algo_pt_status9) {
        ili_write_log_buf(20, id0.algo_pt_status9);
    }
    if (id_last.reserved2 != id0.reserved2) {
        ili_write_log_buf(21, id0.reserved2);
    }
    if (id0.hopping_flg) {
        if (id_last.hopping_index != id0.hopping_index) {
            ili_write_log_buf(22, id0.hopping_index);
        }
    }


    if (id_last.reserved3 != id0.reserved3) {
        ili_write_log_buf(23, id0.reserved3);
    }
    if (id_last.reserved4 != id0.reserved4) {
        ili_write_log_buf(24, id0.reserved4);
    }
    ipio_memcpy(&id_last, &id0, sizeof(id_last), sizeof(id0));

    //msleep(2000);
    if (tp_debug > 0 || ipio_debug_level) {
        TPD_INFO("id = %d\n", id0.id);
        TPD_INFO("app_sys_powr_state_e = %d\n", id0.app_sys_powr_state_e);
        TPD_INFO("app_sys_state_e = %d\n", id0.app_sys_state_e);
        TPD_INFO("tp_state_e = %d\n", id0.tp_state_e);
        TPD_INFO("touch_palm_state_e = %d\n", id0.touch_palm_state_e);
        TPD_INFO("app_an_statu_e = %d\n", id0.app_an_statu_e);
        TPD_INFO("app_sys_check_bg_abnormal = %d\n", id0.app_sys_check_bg_abnormal);
        TPD_INFO("g_b_wrong_bg = %d\n", id0.g_b_wrong_bg);
        TPD_INFO("status_of_dynamic_th_e = %d\n", id0.status_of_dynamic_th_e);
        TPD_INFO("algo_pt_status0 = %d\n", id0.algo_pt_status0);
        TPD_INFO("algo_pt_status1 = %d\n", id0.algo_pt_status1);
        TPD_INFO("algo_pt_status2 = %d\n", id0.algo_pt_status2);
        TPD_INFO("algo_pt_status3 = %d\n", id0.algo_pt_status3);
        TPD_INFO("algo_pt_status4 = %d\n", id0.algo_pt_status4);
        TPD_INFO("algo_pt_status5 = %d\n", id0.algo_pt_status5);
        TPD_INFO("algo_pt_status6 = %d\n", id0.algo_pt_status6);
        TPD_INFO("algo_pt_status7 = %d\n", id0.algo_pt_status7);
        TPD_INFO("algo_pt_status8 = %d\n", id0.algo_pt_status8);
        TPD_INFO("algo_pt_status9 = %d\n", id0.algo_pt_status9);
        TPD_INFO("hopping_index = %d\n", id0.hopping_index);
        TPD_INFO("hopping_flg = %d\n", id0.hopping_flg);
        TPD_INFO("frequency = %dK\n", (id0.frequency_h << 8) + id0.frequency_l);
    }
}

#endif // end of CONFIG_OPPO_TP_APK


void ilitek_tddi_demo_debug_info_mode(u8 *buf, int len)
{
    u8 *info_ptr;
    u8 info_id, info_len;
    ilitek_tddi_report_ap_mode(buf, P5_X_DEMO_MODE_PACKET_LENGTH);
    info_ptr = buf + P5_X_DEMO_MODE_PACKET_LENGTH;
    info_len = info_ptr[0];
    info_id = info_ptr[1];

    TPD_INFO("info len = %d ,id = %d\n", info_len, info_id);
    if (info_id == 0) {
#ifdef CONFIG_OPPO_TP_APK
        idev->demo_debug_info[info_id](&info_ptr[1], info_len);
#endif // end of CONFIG_OPPO_TP_APK
    } else {
        TPD_INFO("not support this id %d\n", info_id);
    }
}

void ilitek_tddi_report_gesture_mode(u8 *buf, int len)
{
    TPD_INFO("gesture code = 0x%x\n", buf[1]);
    ipio_memcpy(idev->gesture_data, buf, len, P5_X_GESTURE_INFO_LENGTH);

#ifdef CONFIG_OPPO_TP_APK
    if (idev->debug_gesture_sta) {
        if (idev->ts->gesture_buf) {
            int tmp_len = len ;
            if (tmp_len > P5_X_GESTURE_INFO_LENGTH) {
                tmp_len = P5_X_GESTURE_INFO_LENGTH;
            }
            memcpy(idev->ts->gesture_buf, buf, tmp_len);

        }
    }
#endif // end of CONFIG_OPPO_TP_APK
    ilitek_tddi_touch_send_debug_data(buf, len);
}

void ilitek_tddi_report_i2cuart_mode(u8 *buf, int len)
{
    ilitek_tddi_touch_send_debug_data(buf, len);
}

/*******Part4:Call Back Function implement*******/
static int ilitek_ftm_process(void *chip_data)
{
    int ret = -1;
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("\n");
    chip_info->hw_res->reset_gpio = chip_info->ts->hw_res.reset_gpio;
    mutex_lock(&idev->touch_mutex);
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    ret = ilitek_tddi_fw_upgrade(chip_info->fw_upgrade_mode, ILI_FILE, REQUEST_FIRMWARE);
    if (ret < 0) {
        TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
    }

    TPD_INFO("FTM tp enter sleep\n");
    /*ftm sleep in */
    ilitek_tddi_ic_func_ctrl("sleep", SLEEP_IN_FTM_BEGIN);
    mutex_unlock(&idev->touch_mutex);
    return ret;
}

void tp_goto_sleep_ftm(void)
{
    int ret = 0;

    if(idev != NULL && idev->ts != NULL) {
        TPD_INFO("idev->ts->boot_mode = %d\n", idev->ts->boot_mode);

        if ((idev->ts->boot_mode == MSM_BOOT_MODE__FACTORY
             || idev->ts->boot_mode == MSM_BOOT_MODE__RF
             || idev->ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
            mutex_lock(&idev->touch_mutex);
            idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
            ret = ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
            mutex_unlock(&idev->touch_mutex);
            if (ret < 0) {
                TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
            }

            //lcd will goto sleep when tp suspend, close lcd esd check
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
            TPD_INFO("disable_esd_thread by tp driver\n");
            disable_esd_thread();
#endif // end of CONFIG_FB_MSM
#endif

            mutex_lock(&idev->touch_mutex);
            ilitek_tddi_ic_func_ctrl("sense", DISABLE);
            ilitek_tddi_ic_check_busy(5, 35);
            ilitek_tddi_ic_func_ctrl("sleep", SLEEP_IN_FTM_END);
            mutex_unlock(&idev->touch_mutex);
            msleep(60);
            TPD_INFO("mdelay 60 ms test for ftm wait sleep\n");
        }
    }

}

static void ilitek_reset_queue_work_prepare(void)
{
    mutex_lock(&idev->touch_mutex);
    atomic_set(&idev->fw_stat, ENABLE);
    ilitek_tddi_reset_ctrl(idev->reset);
    idev->ignore_first_irq = true;
    ilitek_ice_mode_ctrl(ENABLE, OFF);
    idev->already_reset = true;
    mdelay(5);
    mutex_unlock(&idev->touch_mutex);
}

static int ilitek_reset(void *chip_data)
{
    int ret = -1;
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    if (chip_info->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
        chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
    }
    mutex_lock(&idev->touch_mutex);
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    ret = ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
    if (ret < 0) {
        TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
    }
    mutex_unlock(&idev->touch_mutex);
    return 0;
}

static int ilitek_power_control(void *chip_data, bool enable)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("set reset pin low\n");
    if (gpio_is_valid(chip_info->hw_res->reset_gpio)) {
        gpio_direction_output(chip_info->hw_res->reset_gpio, 0);
    }
    return 0;
}

static int ilitek_get_chip_info(void *chip_data)
{

    TPD_INFO("\n");
    return 0;
}

static u8 ilitek_trigger_reason(void *chip_data, int gesture_enable, int is_suspended)
{
    int ret = 0;
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    chip_info->pointid_info = 0;
    chip_info->irq_timer = jiffies;    //reset esd check trigger base time

    memset(chip_info->points, 0, sizeof(struct point_info) * 10);
    memset(chip_info->gesture_data, 0, P5_X_GESTURE_INFO_LENGTH);
    if (atomic_read(&idev->mp_int_check) == ENABLE) {
        atomic_set(&idev->mp_int_check, DISABLE);
        TPD_INFO("Get an INT for mp, ignore\n");
        return IRQ_IGNORE;
    }
    /*ignore first irq after hw rst pin reset*/
    if (idev->ignore_first_irq) {
        TPD_INFO("ignore_first_irq\n");
        idev->ignore_first_irq = false;
        return IRQ_IGNORE;
    }

    if (!idev->report || atomic_read(&idev->tp_reset) ||
        atomic_read(&idev->fw_stat) || atomic_read(&idev->tp_sw_mode) ||
        atomic_read(&idev->mp_stat) || atomic_read(&idev->tp_sleep) ||
        atomic_read(&idev->esd_stat)) {
        TPD_INFO("ignore interrupt !\n");
        return IRQ_IGNORE;
    }

    if ((gesture_enable == 1) && is_suspended) {
        if (idev->gesture_process_ws) {
            TPD_INFO("black gesture process wake lock\n");
            __pm_stay_awake(idev->gesture_process_ws);
        }
        mdelay(40);

        mutex_lock(&idev->touch_mutex);
        ret = ilitek_tddi_report_handler();
        mutex_unlock(&idev->touch_mutex);

        if (idev->gesture_process_ws) {
            TPD_INFO("black gesture process wake unlock\n");
            __pm_relax(idev->gesture_process_ws);
        }
        if (ret < 0) {
            return IRQ_IGNORE;
        }
        return IRQ_GESTURE;
    } else if (is_suspended) {
        return IRQ_IGNORE;
    }
    mutex_lock(&idev->touch_mutex);
    ret = ilitek_tddi_report_handler();
    mutex_unlock(&idev->touch_mutex);
    if (ret < 0) {
        TPD_INFO("get point info error ignore\n");
        return IRQ_IGNORE;
    }
    return IRQ_TOUCH;
}

static int ilitek_get_touch_points(void *chip_data, struct point_info *points, int max_num)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    memcpy(points, chip_info->points, sizeof(struct point_info) * MAX_TOUCH_NUM);
    return chip_info->pointid_info;
}

static int ilitek_get_gesture_info(void *chip_data, struct gesture_info *gesture)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
    uint8_t gesture_id = 0;
    uint8_t score = 0;
    int lu_x = 0;
    int lu_y = 0;
    int rd_x = 0;
    int rd_y = 0;
    uint8_t point_data[P5_X_GESTURE_INFO_LENGTH] = {0};

    memset(point_data, 0, sizeof(point_data));
    memcpy(point_data, chip_info->gesture_data, P5_X_GESTURE_INFO_LENGTH);
    if (point_data[0] != P5_X_GESTURE_PACKET_ID) {
        TPD_INFO("read gesture data failed point_data[0] = 0x%X\n", point_data[0]);
        return -1;
    }

    gesture_id = (uint8_t)(point_data[1]);
    score = point_data[36];

    gesture->Point_start.x = (((point_data[4] & 0xF0) << 4) | (point_data[5]));
    gesture->Point_start.y = (((point_data[4] & 0x0F) << 8) | (point_data[6]));
    gesture->Point_end.x   = (((point_data[7] & 0xF0) << 4) | (point_data[8]));
    gesture->Point_end.y   = (((point_data[7] & 0x0F) << 8) | (point_data[9]));

    gesture->Point_1st.x   = (((point_data[16] & 0xF0) << 4) | (point_data[17]));
    gesture->Point_1st.y   = (((point_data[16] & 0x0F) << 8) | (point_data[18]));
    gesture->Point_2nd.x   = (((point_data[19] & 0xF0) << 4) | (point_data[20]));
    gesture->Point_2nd.y   = (((point_data[19] & 0x0F) << 8) | (point_data[21]));
    gesture->Point_3rd.x   = (((point_data[22] & 0xF0) << 4) | (point_data[23]));
    gesture->Point_3rd.y   = (((point_data[22] & 0x0F) << 8) | (point_data[24]));

    switch (gesture_id) {   //judge gesture type
    case GESTURE_RIGHT :
        gesture->gesture_type  = Left2RightSwip;
        break;

    case GESTURE_LEFT :
        gesture->gesture_type  = Right2LeftSwip;
        break;

    case GESTURE_DOWN  :
        gesture->gesture_type  = Up2DownSwip;
        break;

    case GESTURE_UP :
        gesture->gesture_type  = Down2UpSwip;
        break;

    case GESTURE_DOUBLECLICK:
        gesture->gesture_type  = DouTap;
        gesture->Point_end     = gesture->Point_start;
        break;

    case GESTURE_V :
        gesture->gesture_type  = UpVee;
        break;

    case GESTURE_V_DOWN :
        gesture->gesture_type  = DownVee;
        break;

    case GESTURE_V_LEFT :
        gesture->gesture_type  = LeftVee;
        break;

    case GESTURE_V_RIGHT :
        gesture->gesture_type  = RightVee;
        break;

    case GESTURE_O  :
        gesture->gesture_type = Circle;
        gesture->clockwise = (point_data[34] > 1) ? 0 : point_data[34];

        lu_x = (((point_data[28] & 0xF0) << 4) | (point_data[29]));
        lu_y = (((point_data[28] & 0x0F) << 8) | (point_data[30]));
        rd_x = (((point_data[31] & 0xF0) << 4) | (point_data[32]));
        rd_y = (((point_data[31] & 0x0F) << 8) | (point_data[33]));

        gesture->Point_1st.x   = ((rd_x + lu_x) / 2);  //ymain
        gesture->Point_1st.y   = lu_y;
        gesture->Point_2nd.x   = lu_x;  //xmin
        gesture->Point_2nd.y   = ((rd_y + lu_y) / 2);
        gesture->Point_3rd.x   = ((rd_x + lu_x) / 2);  //ymax
        gesture->Point_3rd.y   = rd_y;
        gesture->Point_4th.x   = rd_x;  //xmax
        gesture->Point_4th.y   = ((rd_y + lu_y) / 2);
        break;

    case GESTURE_M  :
        gesture->gesture_type  = Mgestrue;
        break;

    case GESTURE_W :
        gesture->gesture_type  = Wgestrue;
        break;

    case GESTURE_TWOLINE_DOWN :
        gesture->gesture_type  = DouSwip;
        gesture->Point_1st.x   = (((point_data[10] & 0xF0) << 4) | (point_data[11]));
        gesture->Point_1st.y   = (((point_data[10] & 0x0F) << 8) | (point_data[12]));
        gesture->Point_2nd.x   = (((point_data[13] & 0xF0) << 4) | (point_data[14]));
        gesture->Point_2nd.y   = (((point_data[13] & 0x0F) << 8) | (point_data[15]));
        break;

    default:
        gesture->gesture_type = UnkownGesture;
        break;
    }
    TPD_DEBUG("gesture data 0-17 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
              "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", \
              point_data[0], point_data[1], point_data[2], point_data[3], point_data[4], point_data[5], \
              point_data[6], point_data[7], point_data[8], point_data[9], point_data[10], point_data[11], \
              point_data[12], point_data[13], point_data[14], point_data[15], point_data[16], point_data[17]);

    TPD_DEBUG("gesture data 18-35 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
              "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", \
              point_data[18], point_data[19], point_data[20], point_data[21], point_data[22], point_data[23], \
              point_data[24], point_data[25], point_data[26], point_data[27], point_data[28], point_data[29], \
              point_data[30], point_data[31], point_data[32], point_data[33], point_data[34], point_data[35]);

    TPD_INFO("gesture debug data 160-168 0x%02X 0x%02X "
             "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", \
             point_data[160], point_data[161], point_data[162], point_data[163], \
             point_data[164], point_data[165], point_data[166], point_data[167], point_data[168]);

    TPD_DEBUG("before scale gesture_id: 0x%x, score: %d, gesture_type: %d, clockwise: %d,"
              "points: (%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n", \
              gesture_id, score, gesture->gesture_type, gesture->clockwise, \
              gesture->Point_start.x, gesture->Point_start.y, \
              gesture->Point_end.x, gesture->Point_end.y, \
              gesture->Point_1st.x, gesture->Point_1st.y, \
              gesture->Point_2nd.x, gesture->Point_2nd.y, \
              gesture->Point_3rd.x, gesture->Point_3rd.y, \
              gesture->Point_4th.x, gesture->Point_4th.y);

    gesture->Point_start.x = gesture->Point_start.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_start.y = gesture->Point_start.y * (idev->panel_hei) / TPD_HEIGHT;
    gesture->Point_end.x = gesture->Point_end.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_end.y = gesture->Point_end.y * (idev->panel_hei) / TPD_HEIGHT;
    gesture->Point_1st.x = gesture->Point_1st.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_1st.y = gesture->Point_1st.y * (idev->panel_hei) / TPD_HEIGHT;

    gesture->Point_2nd.x = gesture->Point_2nd.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_2nd.y = gesture->Point_2nd.y * (idev->panel_hei) / TPD_HEIGHT;

    gesture->Point_3rd.x = gesture->Point_3rd.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_3rd.y = gesture->Point_3rd.y * (idev->panel_hei) / TPD_HEIGHT;

    gesture->Point_4th.x = gesture->Point_4th.x * (idev->panel_wid) / TPD_WIDTH;
    gesture->Point_4th.y = gesture->Point_4th.y * (idev->panel_hei) / TPD_HEIGHT;

    TPD_INFO("gesture_id: 0x%x, score: %d, gesture_type: %d, clockwise: %d, points:"
             "(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n", \
             gesture_id, score, gesture->gesture_type, gesture->clockwise, \
             gesture->Point_start.x, gesture->Point_start.y, \
             gesture->Point_end.x, gesture->Point_end.y, \
             gesture->Point_1st.x, gesture->Point_1st.y, \
             gesture->Point_2nd.x, gesture->Point_2nd.y, \
             gesture->Point_3rd.x, gesture->Point_3rd.y, \
             gesture->Point_4th.x, gesture->Point_4th.y);

    return 0;
}

static int ilitek_mode_switch(void *chip_data, work_mode mode, bool flag)
{
    int ret = 0;
    uint8_t temp[64] = {0};
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    if (!chip_info->already_upgrade || atomic_read(&chip_info->tp_reset) ||
        atomic_read(&chip_info->fw_stat) || atomic_read(&chip_info->tp_sw_mode) ||
        atomic_read(&chip_info->mp_stat) || atomic_read(&chip_info->tp_sleep) ||
        atomic_read(&chip_info->esd_stat)) {
        TPD_INFO("doing other process!\n");
        return ret;
    }
    mutex_lock(&idev->touch_mutex);
    switch(mode) {
    case MODE_NORMAL:
        TPD_DEBUG("MODE_NORMAL flag = %d\n", flag);
        ret = 0;
        break;

    case MODE_SLEEP:
        TPD_INFO("MODE_SLEEP flag = %d\n", flag);

        //lcd will goto sleep when tp suspend, close lcd esd check
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
        TPD_INFO("disable_esd_thread by tp driver\n");
        disable_esd_thread();
#endif // end of CONFIG_FB_MSM
#endif
        if (chip_info->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
            ilitek_tddi_sleep_handler(TP_DEEP_SLEEP);
        } else {
            ilitek_tddi_ic_func_ctrl("sleep", SLEEP_IN);
        }
        //ilitek_platform_tp_hw_reset(false);
        break;

    case MODE_GESTURE:
        TPD_INFO("MODE_GESTURE flag = %d\n", flag);
        if (chip_info->sleep_type == DEEP_SLEEP_IN) {
            TPD_INFO("TP in deep sleep mode is not support gesture mode flag = %d\n", flag);
            break;
        }
        chip_info->gesture = flag;
        if (flag) {
            //lcd will goto sleep when tp suspend, close lcd esd check
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
            TPD_INFO("disable_esd_thread by tp driver\n");
            disable_esd_thread();
#endif // end of CONFIG_FB_MSM
#endif
            if (chip_info->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
                ilitek_tddi_sleep_handler(TP_SUSPEND);
            } else {
                temp[0] = 0xF6;
                temp[1] = 0x0A;
                TPD_INFO("write prepare gesture command 0xF6 0x0A \n");
                if ((chip_info->write(temp, 2)) < 0) {
                    TPD_INFO("write prepare gesture command error\n");
                }
                if (ilitek_tddi_ic_func_ctrl("lpwg", 0x2) < 0)
                    ipio_err("write gesture flag failed\n");
            }
#ifdef CONFIG_OPPO_TP_APK
            if (chip_info->debug_gesture_sta) {
                ilitek_set_gesture_fail_reason(ENABLE);
            }
#endif // end of CONFIG_OPPO_TP_APK
        }
        break;

    case MODE_EDGE:
        TPD_INFO("MODE_EDGE flag = %d\n", flag);
        if (flag || (VERTICAL_SCREEN == chip_info->touch_direction)) {
            temp[0] = 0x01;
        } else if (LANDSCAPE_SCREEN_270 == chip_info->touch_direction) {
            temp[0] = 0x00;
        } else if (LANDSCAPE_SCREEN_90 == chip_info->touch_direction) {
            temp[0] = 0x02;
        }
        if (ilitek_tddi_ic_func_ctrl("edge_palm", temp[0]) < 0)
            ipio_err("write edge_palm flag failed\n");
        break;

    case MODE_HEADSET:
        TPD_INFO("MODE_HEADSET flag = %d\n", flag);
        if (ilitek_tddi_ic_func_ctrl("ear_phone", flag) < 0)
            ipio_err("write ear_phone flag failed\n");
        break;

    case MODE_CHARGE:
        TPD_INFO("MODE_CHARGE flag = %d\n", flag);
        if (ilitek_tddi_ic_func_ctrl("plug", !flag) < 0)
            ipio_err("write plug flag failed\n");
        break;

    case MODE_GAME:
        TPD_INFO("MODE_GAME flag = %d\n", flag);
        if (ilitek_tddi_ic_func_ctrl("lock_point", !flag) < 0)
            ipio_err("write lock_point flag failed\n");
        break;

    default:
        TPD_INFO("%s: Wrong mode.\n", __func__);
    }
    mutex_unlock(&idev->touch_mutex);
    return ret;
}

static fw_check_state ilitek_fw_check(void *chip_data, struct resolution_info *resolution_info, struct panel_info *panel_data)
{
    //uint8_t ver_len = 0;
    int ret = 0;
    //char dev_version[MAX_DEVICE_VERSION_LENGTH] = {0};
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("%s: call\n", __func__);
    mutex_lock(&idev->touch_mutex);
    ret = ilitek_tddi_ic_get_fw_ver();
    mutex_unlock(&idev->touch_mutex);
    if (ret < 0) {
        TPD_INFO("%s: get fw info failed\n", __func__);
    } else {
        panel_data->TP_FW = (chip_info->chip->fw_ver >> 8) & 0xFF;
        //sprintf(dev_version, "%02X", panel_data->TP_FW);
        TPD_INFO("firmware_ver = %02X\n", panel_data->TP_FW);

    }
    //chip_info->fw_version = panel_data->manufacture_info.version;
    return FW_NORMAL;
}

static void copy_fw_to_buffer(struct ilitek_tddi_dev *chip_info, const struct firmware *fw)
{
    if (fw) {
        //free already exist fw data buffer
        ipio_vfree((void **) & (chip_info->tp_firmware.data));
        //new fw data buffer
        chip_info->tp_firmware.data = vmalloc(fw->size);
        if (chip_info->tp_firmware.data == NULL) {
            TPD_INFO("kmalloc tp firmware data error\n");

            chip_info->tp_firmware.data = vmalloc(fw->size);
            if (chip_info->tp_firmware.data == NULL) {
                TPD_INFO("retry kmalloc tp firmware data error\n");
                return;
            }
        }

        //copy bin fw to data buffer
        memcpy((u8 *)chip_info->tp_firmware.data, (u8 *)(fw->data), fw->size);
        if (0 == memcmp((u8 *)chip_info->tp_firmware.data, (u8 *)(fw->data), fw->size)) {
            TPD_INFO("copy_fw_to_buffer fw->size=%zu\n", fw->size);
            chip_info->tp_firmware.size = fw->size;
        } else {
            TPD_INFO("copy_fw_to_buffer fw error\n");
            chip_info->tp_firmware.size = 0;
        }
    }

    return;
}

static fw_update_state ilitek_fw_update_common(void *chip_data, const struct firmware *fw, bool force)
{
    int ret = 0;
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("%s start\n", __func__);

    //request firmware failed, get from headfile
    if(fw == NULL) {
        TPD_INFO("request firmware failed\n");
    }
    copy_fw_to_buffer(chip_info, fw);

    mutex_lock(&idev->touch_mutex);
    if (!idev->get_ic_info_flag) {
        /* Must do hw reset once in first time for work normally if tp reset is avaliable */
        if (!TDDI_RST_BIND)
            ilitek_tddi_reset_ctrl(idev->reset);

        idev->do_otp_check = ENABLE;

        ilitek_ice_mode_ctrl(ENABLE, OFF);

        if (ilitek_tddi_ic_get_info() < 0) {
            ipio_err("Not found ilitek chips\n");
        }
        idev->get_ic_info_flag = true;
    }
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    ret = ilitek_tddi_fw_upgrade(chip_info->fw_upgrade_mode, HEX_FILE, REQUEST_FIRMWARE);
    mutex_unlock(&idev->touch_mutex);
    if (ret < 0) {
        TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
        return ret;
    }
    return FW_UPDATE_SUCCESS;
}

static int ilitek_get_vendor(void *chip_data, struct panel_info *panel_data)
{
    int len = 0;
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    len = strlen(panel_data->fw_name);
    if (len > 3) {
        panel_data->fw_name[len - 3] = 'b';
        panel_data->fw_name[len - 2] = 'i';
        panel_data->fw_name[len - 1] = 'n';
    }
    len = strlen(panel_data->test_limit_name);
    if ((len > 3)) {
        panel_data->test_limit_name[len - 3] = 'i';
        panel_data->test_limit_name[len - 2] = 'n';
        panel_data->test_limit_name[len - 1] = 'i';
    }
    chip_info->tp_type = panel_data->tp_type;
    /*get ftm firmware ini from touch.h*/
    chip_info->p_firmware_headfile = &panel_data->firmware_headfile;

    TPD_INFO("chip_info->tp_type = %d, panel_data->test_limit_name = %s, panel_data->fw_name = %s\n",
             chip_info->tp_type, panel_data->test_limit_name, panel_data->fw_name);

    return 0;
}

static void ilitek_black_screen_test(void *chip_data, char *message)
{
    char apk_ret[100] = {0};

    TPD_INFO("enter %s\n", __func__);
    mutex_unlock(&idev->ts->mutex);//for common mutex
    ilitek_tddi_mp_test_handler(apk_ret, NULL, message, OFF);
    mutex_lock(&idev->ts->mutex);//for common mutex
}

static int ilitek_esd_handle(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
    unsigned int timer = jiffies_to_msecs(jiffies - chip_info->irq_timer);

    chip_info->irq_timer = jiffies;
    mutex_lock(&idev->touch_mutex);
    if ((chip_info->esd_check_enabled) && (timer >= WQ_ESD_DELAY) && chip_info->already_upgrade) {
        ilitek_tddi_wq_esd_check();
    } else {
        TPD_DEBUG("Undo esd_check_enabled =  %d\n", chip_info->esd_check_enabled);
    }
    mutex_unlock(&idev->touch_mutex);
    return 0;
}

static void ilitek_set_touch_direction(void *chip_data, uint8_t dir)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    chip_info->touch_direction = dir;
}

static uint8_t ilitek_get_touch_direction(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    return chip_info->touch_direction;
}

static struct oppo_touchpanel_operations ilitek_ops = {
    .ftm_process                = ilitek_ftm_process,
    .ftm_process_extra          = tp_goto_sleep_ftm,
    .reset                      = ilitek_reset,
    .power_control              = ilitek_power_control,
    .get_chip_info              = ilitek_get_chip_info,
    .trigger_reason             = ilitek_trigger_reason,
    .get_touch_points           = ilitek_get_touch_points,
    .get_gesture_info           = ilitek_get_gesture_info,
    .mode_switch                = ilitek_mode_switch,
    .fw_check                   = ilitek_fw_check,
    .fw_update                  = ilitek_fw_update_common,
    .get_vendor                 = ilitek_get_vendor,
    .black_screen_test          = ilitek_black_screen_test,
    .esd_handle                 = ilitek_esd_handle,
    .set_touch_direction        = ilitek_set_touch_direction,
    .get_touch_direction        = ilitek_get_touch_direction,
    .tp_queue_work_prepare      = ilitek_reset_queue_work_prepare,
};

static int ilitek_read_debug_data(struct seq_file *s, struct ilitek_tddi_dev *chip_info, u8 read_type)
{
    int ret;
    u8 tp_mode;
    uint8_t test_cmd[4] = { 0 };
    int i = 0;
    int j = 0;
    int xch = idev->xch_num;
    int ych = idev->ych_num;

    mutex_lock(&idev->touch_mutex);
    tp_mode = P5_X_FW_DEBUG_MODE;
    ret = ilitek_tddi_switch_mode(&tp_mode);
    if (ret < 0) {
        TPD_INFO("Failed to switch debug mode\n");
        seq_printf(s, "get data failed\n");
        mutex_unlock(&idev->touch_mutex);
        return -1;
    }
    test_cmd[0] = 0xFA;
    test_cmd[1] = read_type;
    TPD_INFO("debug cmd 0x%X, 0x%X", test_cmd[0], test_cmd[1]);

    idev->debug_node_open = false;
    idev->debug_data_frame = 0;
    if (ERR_ALLOC_MEM(idev->debug_buf)) {
        idev->debug_buf = (unsigned char **)kmalloc(DEBUG_DATA_SAVE_MAX_FRAME  * sizeof(unsigned char *), GFP_KERNEL);
        if (!ERR_ALLOC_MEM(idev->debug_buf)) {
            for (i = 0; i < DEBUG_DATA_SAVE_MAX_FRAME; i++) {
                idev->debug_buf[i] = (char *)kmalloc(DEBUG_DATA_MAX_LENGTH * sizeof(unsigned char), GFP_KERNEL);
                if (ERR_ALLOC_MEM(idev->debug_buf[i])) {
                    TPD_INFO("Failed to malloc debug_buf[%d]\n", i);
                }
            }
        } else {
            TPD_INFO("Failed to malloc debug_buf\n");
        }
    } else {
        TPD_INFO("Already malloc debug_buf\n");
    }
    ret = idev->write(test_cmd, 2);
    idev->debug_node_open = true;
    mutex_unlock(&idev->touch_mutex);

    mutex_unlock(&idev->ts->mutex);//for common driver lock ts->mutex
    enable_irq(idev->irq_num);//because oppo disable
    for (i = 0; i < 400; i++) {
        msleep(5);
        //mutex_lock(&idev->touch_mutex);
        if (idev->debug_data_frame > 0 && idev->debug_buf[idev->debug_data_frame - 1][0] == P5_X_DEBUG_PACKET_ID) {
            TPD_INFO("already read debug data\n");
            idev->debug_node_open = false;
            //mutex_unlock(&idev->touch_mutex);
            break;
        }
        //mutex_unlock(&idev->touch_mutex);
    }
    idev->debug_node_open = false;
    disable_irq_nosync(idev->irq_num);
    msleep(15);
    switch (read_type) {
    case P5_X_FW_RAW_DATA_MODE:
        seq_printf(s, "raw_data:\n");
        break;
    case P5_X_FW_DELTA_DATA_MODE:
        seq_printf(s, "diff_data:\n");
        break;
    default:
        seq_printf(s, "read type not support\n");
        break;
    }
    if (idev->debug_data_frame > 0) {
        for (i = 0; i < ych; i++) {
            seq_printf(s, "[%2d]", i);
            for (j = 0; j < xch; j++) {
                seq_printf(s, "%5d,", (s16)((idev->debug_buf[idev->debug_data_frame - 1][(i * xch + j) * 2 + 35] << 8)
                                            + idev->debug_buf[idev->debug_data_frame - 1][(i * xch + j) * 2 + 35 + 1]));
            }
            seq_printf(s, "\n");
        }
    } else {
        seq_printf(s, "get data failed\n");
    }
    idev->debug_data_frame = 0;
    if (!ERR_ALLOC_MEM(idev->debug_buf)) {
        for (i = 0; i < DEBUG_DATA_SAVE_MAX_FRAME; i++) {
            ipio_kfree((void **)&idev->debug_buf[i]);
        }
        ipio_kfree((void **)&idev->debug_buf);
    } else {
        TPD_INFO("Already free debug_buf\n");
    }

    mutex_lock(&idev->ts->mutex);//for common driver  ts->mutex
    /* change to demo mode */
    tp_mode = P5_X_FW_DEMO_MODE;
    mutex_lock(&idev->touch_mutex);
    ilitek_tddi_switch_mode(&tp_mode);
    mutex_unlock(&idev->touch_mutex);
    return 0;
}

#ifdef CONFIG_OPPO_TP_APK
static int ilitek_read_debug_diff(struct seq_file *s, struct ilitek_tddi_dev *chip_info)
{
    int ret;
    int i = 0;
    int j = 0;
    int xch = idev->xch_num;
    int ych = idev->ych_num;
    u16 *debug_buf = NULL;
    mutex_lock(&idev->touch_mutex);
    mdelay(50);
    TPD_INFO("Get data");
    ret = ilitek_tddi_ic_func_ctrl("tp_recore", 2);
    if (ret < 0) {
        ipio_err("cmd fail\n");
        goto out;
    }

    debug_buf = kzalloc(xch * ych * 2, GFP_KERNEL);

    if(debug_buf == NULL) {
        goto out;
    }

    ret = ilitek_tddi_get_tp_recore_data(debug_buf, xch * ych * 2);

    if (ret < 0)
        ipio_err("get data fail\n");

    TPD_INFO("recore reset");
    ret = ilitek_tddi_ic_func_ctrl("tp_recore", 3);
    if (ret < 0) {
        ipio_err("cmd fail\n");
        goto out;
    }

    seq_printf(s, "debug finger down diff data:\n");

    if (debug_buf) {
        for (i = 0; i < ych; i++) {
            seq_printf(s, "[%2d]", i);
            for (j = 0; j < xch; j++) {
                seq_printf(s, "%5d,", (s16)(debug_buf[(i * xch + j) ]));
            }
            seq_printf(s, "\n");
        }
    } else {
        seq_printf(s, "get data failed\n");
    }

out:
    ipio_kfree((void **)&debug_buf);
    mutex_unlock(&idev->touch_mutex);
    return 0;
}
#endif // end of CONFIG_OPPO_TP_APK

static void ilitek_delta_read(struct seq_file *s, void *chip_data)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
    ilitek_read_debug_data(s, chip_info, P5_X_FW_DELTA_DATA_MODE);
#ifdef CONFIG_OPPO_TP_APK
    if (chip_info->debug_mode_sta) {
        ilitek_read_debug_diff(s, chip_info);
    }
#endif // end of CONFIG_OPPO_TP_APK
}

static void ilitek_baseline_read(struct seq_file *s, void *chip_data)
{
    struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

    TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
    ilitek_read_debug_data(s, chip_info, P5_X_FW_RAW_DATA_MODE);
}

static void ilitek_main_register_read(struct seq_file *s, void *chip_data)
{
    TPD_INFO("\n");
}

static struct debug_info_proc_operations ilitek_debug_info_proc_ops = {
    .baseline_read = ilitek_baseline_read,
    .delta_read = ilitek_delta_read,
    .main_register_read = ilitek_main_register_read,
};

#ifdef CONFIG_OPPO_TP_APK

static void ili_apk_game_set(void *chip_data, bool on_off)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    ilitek_mode_switch(chip_data, MODE_GAME, on_off);
    chip_info->lock_point_status = on_off;
}

static bool ili_apk_game_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    return chip_info->lock_point_status;
}

static void ili_apk_debug_set(void *chip_data, bool on_off)
{
    u8 cmd[1];
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;


    if (on_off) {
        cmd[0] = P5_X_FW_DEMO_DEBUG_INFO_MODE;
        ilitek_tddi_switch_mode(cmd);
        ilitek_tddi_get_tp_recore_ctrl(1);

    } else {
        cmd[0] = P5_X_FW_DEMO_MODE;
        ilitek_tddi_switch_mode(cmd);
        ilitek_tddi_get_tp_recore_ctrl(0);
    }

    chip_info->debug_mode_sta = on_off;
}

static bool ili_apk_debug_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;

    return chip_info->debug_mode_sta;
}

static void ili_apk_gesture_debug(void *chip_data, bool on_off)
{

    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    //get_gesture_fail_reason(on_off);
    chip_info->debug_gesture_sta = on_off;
}

static bool  ili_apk_gesture_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    return chip_info->debug_gesture_sta;
}

static int  ili_apk_gesture_info(void *chip_data, char *buf, int len)
{
    int ret = 0;
    int i;
    int num;

    if(len < 2) {
        return 0;
    }
    buf[0] = 255;
    if (idev->ts->gesture_buf[0] == 0xAA) {

        switch (idev->ts->gesture_buf[1]) {   //judge gesture type
        case GESTURE_RIGHT :
            buf[0]  = Left2RightSwip;
            break;

        case GESTURE_LEFT :
            buf[0]  = Right2LeftSwip;
            break;

        case GESTURE_DOWN  :
            buf[0]  = Up2DownSwip;
            break;

        case GESTURE_UP :
            buf[0]  = Down2UpSwip;
            break;

        case GESTURE_DOUBLECLICK:
            buf[0]  = DouTap;
            break;

        case GESTURE_V :
            buf[0]  = UpVee;
            break;

        case GESTURE_V_DOWN :
            buf[0]  = DownVee;
            break;

        case GESTURE_V_LEFT :
            buf[0]  = LeftVee;
            break;

        case GESTURE_V_RIGHT :
            buf[0]  = RightVee;
            break;

        case GESTURE_O  :
            buf[0] = Circle;

            break;

        case GESTURE_M  :
            buf[0]  = Mgestrue;
            break;

        case GESTURE_W :
            buf[0]  = Wgestrue;
            break;

        case GESTURE_TWOLINE_DOWN :
            buf[0]  = DouSwip;
            break;

        default:
            buf[0] = UnkownGesture;
            break;
        }
    } else if (idev->ts->gesture_buf[0] == 0xAE) {
        buf[0] = idev->ts->gesture_buf[1] + 128;
    }
    //buf[0] = gesture_buf[0];
    num = idev->ts->gesture_buf[35];

    if(num > 40) {
        num = 40;
    }
    ret = 2;
    buf[1] = num;
    for (i = 0; i < num; i++) {
        int x;
        int y;
        x = (idev->ts->gesture_buf[40 + i * 3] & 0xF0) << 4;
        x = x | (idev->ts->gesture_buf[40 + i * 3 + 1]);
        x = x * (idev->resolution_x) / TPD_WIDTH;

        y = (idev->ts->gesture_buf[40 + i * 3] & 0x0F) << 8;
        y = y | idev->ts->gesture_buf[40 + i * 3 + 2];
        y = y * (idev->resolution_y) / TPD_HEIGHT;

        if (len < i * 4 + 2) {
            break;
        }
        buf[i * 4 + 2] = x & 0xFF;
        buf[i * 4 + 3] = (x >> 8) & 0xFF;
        buf[i * 4 + 4] = y & 0xFF;
        buf[i * 4 + 5] = (y >> 8) & 0xFF;
        ret += 4;

    }

    return ret;
}


static void ili_apk_earphone_set(void *chip_data, bool on_off)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    ilitek_mode_switch(chip_data, MODE_HEADSET, on_off);
    chip_info->earphone_sta = on_off;
}

static bool ili_apk_earphone_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    return chip_info->earphone_sta;
}

static void ili_apk_charger_set(void *chip_data, bool on_off)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    ilitek_mode_switch(chip_data, MODE_CHARGE, on_off);
    chip_info->plug_status = on_off;

}

static bool ili_apk_charger_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;

    return chip_info->plug_status;

}

static void ili_apk_noise_set(void *chip_data, bool on_off)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;
    //ilitek_mode_switch(chip_data, MODE_CHARGE, on_off);
    ilitek_tddi_ic_func_ctrl("freq_scan", on_off);

    chip_info->noise_sta = on_off;

}

static bool ili_apk_noise_get(void *chip_data)
{
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;

    return chip_info->noise_sta;

}


static int  ili_apk_tp_info_get(void *chip_data, char *buf, int len)
{
    int ret;
    struct ilitek_tddi_dev *chip_info;
    chip_info = (struct ilitek_tddi_dev *)chip_data;

    ret = snprintf(buf, len, "IC:ILI%04X\nFW_VER:0x%02X\nCH:%dX%d\n",
                   chip_info->chip->id,
                   chip_info->chip->fw_ver >> 8 & 0xFF,
                   chip_info->hw_res->TX_NUM,
                   chip_info->hw_res->RX_NUM);
    if (ret > len) {
        ret = len;
    }

    return ret;
}

static void ili_init_oppo_apk_op(struct touchpanel_data *ts)
{
    ts->apk_op = kzalloc(sizeof(APK_OPERATION), GFP_KERNEL);
    if(ts->apk_op) {
        ts->apk_op->apk_game_set = ili_apk_game_set;
        ts->apk_op->apk_game_get = ili_apk_game_get;
        ts->apk_op->apk_debug_set = ili_apk_debug_set;
        ts->apk_op->apk_debug_get = ili_apk_debug_get;
        //apk_op->apk_proximity_set = ili_apk_proximity_set;
        //apk_op->apk_proximity_dis = ili_apk_proximity_dis;
        ts->apk_op->apk_noise_set = ili_apk_noise_set;
        ts->apk_op->apk_noise_get = ili_apk_noise_get;
        ts->apk_op->apk_gesture_debug = ili_apk_gesture_debug;
        ts->apk_op->apk_gesture_get = ili_apk_gesture_get;
        ts->apk_op->apk_gesture_info = ili_apk_gesture_info;
        ts->apk_op->apk_earphone_set = ili_apk_earphone_set;
        ts->apk_op->apk_earphone_get = ili_apk_earphone_get;
        ts->apk_op->apk_charger_set = ili_apk_charger_set;
        ts->apk_op->apk_charger_get = ili_apk_charger_get;
        ts->apk_op->apk_tp_info_get = ili_apk_tp_info_get;
        //apk_op->apk_data_type_set = ili_apk_data_type_set;
        //apk_op->apk_rawdata_get = ili_apk_rawdata_get;
        //apk_op->apk_diffdata_get = ili_apk_diffdata_get;
        //apk_op->apk_basedata_get = ili_apk_basedata_get;
        //ts->apk_op->apk_backdata_get = ili_apk_backdata_get;
        //apk_op->apk_debug_info = ili_apk_debug_info;

    } else {
        TPD_INFO("Can not kzalloc apk op.\n");
    }
}
#endif // end of CONFIG_OPPO_TP_APK


static int ilitek_platform_probe(struct spi_device *spi)
{
    int ret = 0;
    struct touchpanel_data *ts = NULL;

    if (!spi) {
        ipio_err("spi device is NULL\n");
        return -ENODEV;
    }

    TPD_INFO("platform probe\n");
    if (tp_register_times > 0) {
        TPD_INFO("TP driver have success loaded %d times, exit\n", tp_register_times);
        return -1;
    }

    /*step1:Alloc chip_info*/
    idev = kzalloc(sizeof(*idev), GFP_KERNEL);
    if (ERR_ALLOC_MEM(idev)) {
        TPD_INFO("Failed to allocate idev memory, %ld\n", PTR_ERR(idev));
        return -ENOMEM;
    }

    /*step2:Alloc common ts*/
    ts = common_touch_data_alloc();
    if (ts == NULL) {
        TPD_INFO("ts kzalloc error\n");
        goto err_ts_malloc;
    }
    memset(ts, 0, sizeof(*ts));

    idev->fw_buf_dma = kzalloc(128 * 1024, GFP_KERNEL | GFP_DMA);
    if (idev->fw_buf_dma == NULL) {
        TPD_INFO("fw kzalloc error\n");
        //ret = -ENOMEM;
        goto err_fw_dma;
    }

    idev->ts = ts;
    /*step3:spi function init*/
    idev->spi = spi;
    idev->dev = &spi->dev;
    idev->write = ilitek_spi_write;
    idev->read = ilitek_spi_read;
    idev->spi_check_read_size = core_spi_check_read_size;
    idev->spi_read_after_check_size = core_spi_read_data_after_checksize;
    idev->spi_speed = ilitek_tddi_ic_spi_speed_ctrl;
    idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
    idev->spi_write_then_read = spi_write_then_read;

    if (TDDI_RST_BIND)
        idev->reset = TP_IC_WHOLE_RST;
    else
        idev->reset = TP_HW_RST_ONLY;

    idev->rst_edge_delay = 1;
    idev->fw_open = FILP_OPEN;
    idev->fw_upgrade_mode = UPGRADE_IRAM;
    idev->mp_move_code = ilitek_tddi_move_mp_code_iram;
    idev->gesture_move_code = ilitek_tddi_move_gesture_code_iram;
    idev->esd_check = ilitek_tddi_wq_esd_spi_check;
    idev->ges_recover = ilitek_tddi_touch_esd_gesture_iram;
    idev->gesture_mode = P5_X_FW_GESTURE_INFO_MODE;
    idev->wtd_ctrl = ON;
    idev->report = ENABLE;
    idev->netlink = DISABLE;
    idev->debug_node_open = DISABLE;
    idev->already_upgrade = false;

    core_spi_setup(SPI_CLK);

    /*step4:ts function init*/
    ts->debug_info_ops = &ilitek_debug_info_proc_ops;
    ts->s_client = spi;
    ts->irq = spi->irq;

    spi_set_drvdata(spi, ts);

    ts->dev = &spi->dev;
    ts->chip_data = idev;

    idev->hw_res = &ts->hw_res;
    //idev->tp_type = TP_AUO;

    //---prepare for spi parameter---
    if (ts->s_client->master->flags & SPI_MASTER_HALF_DUPLEX) {
        printk("Full duplex not supported by master\n");
        ret = -EIO;
        goto err_spi_setup;
    }
    ts->ts_ops = &ilitek_ops;

    ////need before ftm mode
    mutex_init(&idev->touch_mutex);
    mutex_init(&idev->debug_mutex);/*for ili apk debug*/
    mutex_init(&idev->debug_read_mutex);/*for ili apk debug*/
    init_waitqueue_head(&(idev->inq));/*for ili apk debug*/
    spin_lock_init(&idev->irq_spin);

    atomic_set(&idev->irq_stat, DISABLE);
    atomic_set(&idev->ice_stat, DISABLE);
    atomic_set(&idev->tp_reset, END);
    atomic_set(&idev->fw_stat, END);
    atomic_set(&idev->mp_stat, DISABLE);
    atomic_set(&idev->tp_sleep, END);
    atomic_set(&idev->mp_int_check, DISABLE);
    atomic_set(&idev->esd_stat, END);
    atomic_set(&idev->irq_stat, ENABLE);

    /*set ic info flag*/
    idev->get_ic_info_flag = false;

    /*9881h ic init*/
    ilitek_tddi_ic_init();

    /*register common touch device*/
    ret = register_common_touch_device(ts);
    if (ret < 0) {
        TPD_INFO("\n");
        goto err_register_driver;
    }
    /*get ftm firmware ini from touch.h*/
    idev->p_firmware_headfile = &ts->panel_data.firmware_headfile;
    // update fw in probe
    if (ts->fw_update_in_probe_with_headfile) {
        ilitek_fw_update_common((void *)idev, NULL, false);
    }

    ts->tp_suspend_order = TP_LCD_SUSPEND;
    ts->tp_resume_order = LCD_TP_RESUME;

    /*get default info from dts*/
    idev->hw_res->reset_gpio = ts->hw_res.reset_gpio;
    idev->hw_res->irq_gpio = ts->hw_res.irq_gpio;
    idev->irq_num = ts->irq;
    idev->fw_name = ts->panel_data.fw_name;
    idev->test_limit_name = ts->panel_data.test_limit_name;
    idev->resolution_x = ts->resolution_info.max_x;
    idev->resolution_y = ts->resolution_info.max_y;

    /*for ili apk debug node*/
    ilitek_tddi_node_init();
#ifdef CONFIG_OPPO_TP_APK
    idev->demo_debug_info[0] = ilitek_tddi_demo_debug_info_id0;
#endif // end of CONFIG_OPPO_TP_APK

    ilitek_create_proc_for_oppo(ts);
#ifdef CONFIG_OPPO_TP_APK
    ili_init_oppo_apk_op(ts);
#endif // end of CONFIG_OPPO_TP_APK

    if (ts->esd_handle_support) {
        ts->esd_info.esd_work_time = msecs_to_jiffies(WQ_ESD_DELAY);
        TPD_INFO("%s:change esd handle time to %d ms\n", __func__, ts->esd_info.esd_work_time / HZ);
    }
    /*for gesture ws*/
    idev->gesture_process_ws = wakeup_source_register("gesture_wake_lock");
    if (!idev->gesture_process_ws) {
        TPD_INFO("gesture_process_ws request failed\n");
        goto err_spi_setup;
    }
    ipio_debug_level = 0;
    return 0;

err_register_driver:
    if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
         || ts->boot_mode == MSM_BOOT_MODE__RF
         || ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
        idev->gesture_process_ws = wakeup_source_register("gesture_wake_lock");
        TPD_INFO("ftm mode probe end ok\n");
        return 0;
    }

err_spi_setup:
    if (idev->fw_buf_dma) {
        kfree(idev->fw_buf_dma);
    }
err_fw_dma:
    spi_set_drvdata(spi, NULL);
    TPD_INFO("err_spi_setup end\n");

    common_touch_data_free(ts);
    ts = NULL;

err_ts_malloc:
    if(idev) {
        kfree(idev);
        idev = NULL;
    }
    return -1;
}

static int ilitek_platform_remove(struct spi_device *spi)
{
    struct touchpanel_data *ts = spi_get_drvdata(spi);

    TPD_INFO();

    wakeup_source_unregister(idev->gesture_process_ws);
    ipio_kfree((void **)&idev);
    spi_set_drvdata(spi, NULL);
    if (ts) {
        kfree(ts);
        ts = NULL;
    }

    return 0;
}


static int ilitek_spi_resume(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);

    TPD_INFO("%s is called\n", __func__);

    if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
         || ts->boot_mode == MSM_BOOT_MODE__RF
         || ts->boot_mode == MSM_BOOT_MODE__WLAN)) {

        TPD_INFO("ilitek_spi_resume do nothing in ftm\n");
        return 0;
    }
    tp_i2c_resume(ts);

    return 0;
}

static int ilitek_spi_suspend(struct device *dev)
{
    struct touchpanel_data *ts = dev_get_drvdata(dev);

    TPD_INFO("%s: is called\n", __func__);

    if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
         || ts->boot_mode == MSM_BOOT_MODE__RF
         || ts->boot_mode == MSM_BOOT_MODE__WLAN)) {

        TPD_INFO("ilitek_spi_suspend do nothing in ftm\n");
        return 0;
    }

    tp_i2c_suspend(ts);

    return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
#ifdef CONFIG_FB
    .suspend = ilitek_spi_suspend,
    .resume = ilitek_spi_resume,
#endif
};

/*
 * The name in the table must match the definiation
 * in a dts file.
 *
 */
static struct of_device_id tp_match_table[] = {
    {.compatible = DTS_OF_NAME},
    {},
};

static struct spi_driver tp_spi_driver = {
    .driver = {
        .name    = DEVICE_ID,
        .owner = THIS_MODULE,
        .of_match_table = tp_match_table,
        .pm = &tp_pm_ops,
    },
    .probe = ilitek_platform_probe,
    .remove = ilitek_platform_remove,
};

static int __init ilitek_platform_init(void)
{
    int res = 0;

    TPD_INFO("%s is called\n", __func__);
    if (!tp_judge_ic_match(TPD_DEVICE)) {
        TPD_INFO("TP driver is already register\n");
        return -1;
    }
    res = spi_register_driver(&tp_spi_driver);
    if (res < 0) {
        TPD_INFO("Failed to add spi driver\n");
        return -ENODEV;
    }

    TPD_INFO("Succeed to add driver\n");
    return res;
}

static void __exit ilitek_platform_exit(void)
{
    spi_unregister_driver(&tp_spi_driver);
}

module_init(ilitek_platform_init);
module_exit(ilitek_platform_exit);
MODULE_AUTHOR("ILITEK");
