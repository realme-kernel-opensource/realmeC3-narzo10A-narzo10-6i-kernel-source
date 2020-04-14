/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define DEBUG    1

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#ifndef CONFIG_MTK_FPGA
//#include <mtk_clkbuf_ctl.h>
#endif

#include "../../pinctrl/core.h"

#include "../../misc/mediatek/include/mt-plat/mtk_boot_common.h"


#define PN544_DRVNAME           "pn553"
#define PN553_DRVNAME           "pn553"

#define MAX_BUFFER_SIZE         512
#define I2C_ID_NAME             "pn553"

#define PN553_MAGIC             0xE9
#define PN553_SET_PWR           _IOW(PN553_MAGIC, 0x01, unsigned int)




//#undef CONFIG_MTK_I2C_EXTENSION // baker add ,without DMA Mode
/******************************************************************************
 * extern functions
 *******************************************************************************/
//extern void mt_eint_mask(unsigned int eint_num);
//extern void mt_eint_unmask(unsigned int eint_num);
//extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
//extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

struct pn553_dev
{
        wait_queue_head_t       read_wq;
        struct mutex            read_mutex;
        struct i2c_client       *client;
        struct miscdevice       pn553_device;
        bool                    irq_enabled;
        spinlock_t              irq_enabled_lock;

        unsigned int            irq_gpio;
        struct regulator        *reg;
};

//struct pn553_i2c_platform_data pn553_platform_data;

struct pinctrl *gpctrl = NULL;
struct pinctrl_state *st_ven_h = NULL;
struct pinctrl_state *st_ven_l = NULL;
struct pinctrl_state *st_dwn_h = NULL;
struct pinctrl_state *st_dwn_l = NULL;

struct pinctrl_state *st_eint_int = NULL;

/* For DMA */


static unsigned char I2CDMAWriteBuf[MAX_BUFFER_SIZE];
static unsigned char I2CDMAReadBuf[MAX_BUFFER_SIZE];

//#ifdef ODM_WT_EDIT
//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2019/10/09,add for load nfc driver according to boardid
char nfc_hardware_info[20];
//#endif /* ODM_WT_EDIT */


/*****************************************************************************
 * Function
 *****************************************************************************/

static void pn553_enable_irq(struct pn553_dev *pn553_dev)
{
        unsigned long flags;
        printk("%s\n", __func__);

        spin_lock_irqsave(&pn553_dev->irq_enabled_lock, flags);
        if (!pn553_dev->irq_enabled)
        {
                enable_irq(pn553_dev->client->irq);
                pn553_dev->irq_enabled = true;
        }
        spin_unlock_irqrestore(&pn553_dev->irq_enabled_lock, flags);
}

static void pn553_disable_irq(struct pn553_dev *pn553_dev)
{
        unsigned long flags;

        printk("%s\n", __func__);


        spin_lock_irqsave(&pn553_dev->irq_enabled_lock, flags);
        if (pn553_dev->irq_enabled)
        {
                //mt_eint_mask(EINT_NUM);
                disable_irq_nosync(pn553_dev->client->irq);
                pn553_dev->irq_enabled = false;
        }
        spin_unlock_irqrestore(&pn553_dev->irq_enabled_lock, flags);
}


//static int pn553_flag = 0;

static irqreturn_t pn553_dev_irq_handler(int irq, void *dev)
{
        struct pn553_dev *pn553_dev = dev;

        printk("pn553_dev_irq_handler()\n");

        pn553_disable_irq(pn553_dev);

        //pn553_flag = 1;
        /* Wake up waiting readers */
        wake_up(&pn553_dev->read_wq);

        return IRQ_HANDLED;
}

static int pn553_platform_pinctrl_select(struct pinctrl *p, struct pinctrl_state *s)
{
        int ret = 0;

        if (p != NULL && s != NULL) {
                ret = pinctrl_select_state(p, s);
               if (ret) {
			printk("pn553 pinctrl select state:%s failed\n", s->name);
		} else {
			printk("pn553 pinctrl select state %s ok\n", s->name);
		}
	} else {
		printk("%s: pinctrl is NULL\n", __func__);
		ret = -1;
	}

	return ret;
}

static ssize_t pn553_dev_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *offset)
{
	struct pn553_dev *pn553_dev = filp->private_data;
	int ret=0,i;//baker_mod;
	//printk("%s\n", __func__);

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	printk("pn553 %s : reading %zu bytes.\n", __func__, count);
	mutex_lock(&pn553_dev->read_mutex);
	printk("pn553 pn553_dev->irq_gpio ==%d\n",gpio_get_value(pn553_dev->irq_gpio));
	if (!gpio_get_value(pn553_dev->irq_gpio)) {
		pn553_enable_irq(pn553_dev);
		//set_current_state(TASK_INTERRUPTIBLE);
		printk("pn553 read1  pn553_dev->irq_enabled=%d\n", pn553_dev->irq_enabled);
		printk("pn553 pn553_dev->irq_gpio ==%d\n",gpio_get_value(pn553_dev->irq_gpio));
		ret = wait_event_interruptible(pn553_dev->read_wq, !pn553_dev->irq_enabled);
		printk("pn553 read2  pn553_dev->irq_enabled=%d\n", pn553_dev->irq_enabled);
		//pn553_flag = 0;
		pn553_disable_irq(pn553_dev);
		printk("pn553 %s :wait_event_interruptible  ret == %d.\n", __func__, ret);
		if (ret)
		{
			printk("pn553 read wait event error\n");
			goto fail;
		}
		//pn553_flag = 0;
		
		//set_current_state(TASK_RUNNING);

	}

	ret = i2c_master_recv(pn553_dev->client, I2CDMAReadBuf, count);
	//#ifdef ODM_WT_EDIT
	//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2020/1/7,delete redundant  logs
        //for(i = 0;i < count;i++){
        //    printk("%02x ",I2CDMAReadBuf[i]);
	//}
	//printk("\n");
	//#endif #ODM_WT_EDIT#

	if (ret < 0){
		goto fail;
	}else if( ret > count){
		ret = -EIO;
		goto fail;
	}

	if (copy_to_user(buf, I2CDMAReadBuf, ret)) {
		printk("pn553 copy_to_user error\n");
		ret = -EFAULT;
		goto fail;
	}

	mutex_unlock(&pn553_dev->read_mutex);
        //#ifdef ODM_WT_EDIT
        //Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2020/1/7,delete redundant  logs
	//printk("pn553 IFD->PC:");
	//for(i = 0; i < count; i++) {
	//	printk(" %02x", I2CDMAReadBuf[i]);
	//}
	//printk("\n");
	//#endif #ODM_WT_EDIT#

	return ret;

fail:
	mutex_unlock(&pn553_dev->read_mutex);
	printk("pn553 %s: received data err = (%d)\n", __func__, ret);
	return ret;
}


static ssize_t pn553_dev_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *offset)
{
	struct pn553_dev *pn553_dev;
	int ret=0, idx = 0; //baker_mod_w=0;
	
        printk("%s\n", __func__);

	pn553_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(I2CDMAWriteBuf, &buf[(idx*255)], count)) {
		printk("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}
	msleep(15);
	
	ret = i2c_master_send(pn553_dev->client, I2CDMAWriteBuf, count);
	if (ret != count) {
		printk("pn553 %s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	udelay(1000);

	return ret;
}

static int pn553_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct pn553_dev *pn553_dev = container_of(filp->private_data, struct pn553_dev, pn553_device);

	//printk("%s:pn553_dev=%p\n", __func__, pn553_dev);

	filp->private_data = pn553_dev;

	printk("pn553 %s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return ret;
}

static long pn553_dev_unlocked_ioctl(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	int ret;
	struct pn553_dev *pn553_dev = filp->private_data;

	printk("%s:cmd=%d, arg=%ld, pn553_dev=%p\n", __func__, cmd, arg, pn553_dev);

	switch (cmd) 
	{
		case PN553_SET_PWR:
			if (arg == 2) {
				/* power on with firmware download (requires hw reset) */
				printk("pn553 %s power on with firmware\n", __func__);
				ret = pn553_platform_pinctrl_select(gpctrl, st_ven_h);
				ret = pn553_platform_pinctrl_select(gpctrl, st_dwn_h);
				msleep(10);
				ret = pn553_platform_pinctrl_select(gpctrl, st_ven_l);
				msleep(50);
				ret = pn553_platform_pinctrl_select(gpctrl, st_ven_h);
				msleep(10);
			} else if (arg == 1) {
				/* power on */
				printk("pn553 %s power on\n", __func__);
				ret = pn553_platform_pinctrl_select(gpctrl, st_dwn_l);
				ret = pn553_platform_pinctrl_select(gpctrl, st_ven_h); 
				msleep(10);
			} else  if (arg == 0) {
				/* power off */
				printk("pn553 %s power off\n", __func__);
				ret = pn553_platform_pinctrl_select(gpctrl, st_dwn_l);
				ret = pn553_platform_pinctrl_select(gpctrl, st_ven_l);
				msleep(50);
			} else {
				printk("pn553 %s bad arg %lu\n", __func__, arg);
				return -EINVAL;
			}
			break;
		default:
			printk("pn553 %s bad ioctl %u\n", __func__, cmd);
			return -EINVAL;
	}

	return ret;
}


static const struct file_operations pn553_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = pn553_dev_read,
	.write = pn553_dev_write,
	.open = pn553_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = pn553_dev_unlocked_ioctl,
#endif
	.unlocked_ioctl = pn553_dev_unlocked_ioctl,
};

static int pn553_remove(struct i2c_client *client)
{
	struct pn553_dev *pn553_dev;



	pn553_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn553_dev);
	misc_deregister(&pn553_dev->pn553_device);
	mutex_destroy(&pn553_dev->read_mutex);
	regulator_put(pn553_dev->reg);
	kfree(pn553_dev);
	return 0;
}

/* Check for availability of NQ_ NFC controller hardware */
static int pn553_hw_check(struct i2c_client *client, struct pn553_dev *pn553_dev)
{
	int ret = 0;
	unsigned char raw_nci_reset_cmd[] =  {0x20, 0x00, 0x01, 0x01};
	unsigned char nci_reset_rsp[6];

	pn553_platform_pinctrl_select(gpctrl, st_ven_l);/* ULPM: Disable */
	/* hardware dependent delay */
	msleep(20);
	pn553_platform_pinctrl_select(gpctrl, st_ven_h);/* HPD : Enable*/
	/* hardware dependent delay */
	msleep(20);

	/* send NCI CORE RESET CMD with Keep Config parameters */
	//pn553_dev->client->addr = ((pn553_dev->client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG));
	//pn553_dev->client->timing = 400;
	ret = i2c_master_send(client, raw_nci_reset_cmd,
						sizeof(raw_nci_reset_cmd));
	if (ret < 0) {
		printk("%s: - i2c_master_send Error\n", __func__);
		goto err_nfcc_hw_check;
	}
	/* hardware dependent delay */
	msleep(30);

	/* Read Response of RESET command */
	ret = i2c_master_recv(client, nci_reset_rsp,
		sizeof(nci_reset_rsp));
	if (ret < 0) {
		printk("%s: - i2c_master_recv Error\n", __func__);
		goto err_nfcc_hw_check;
	}
	printk("%s: - success - reset cmd answer : NfcNciRx %02x %02x %02x %02x %02x %02x\n",
	__func__, nci_reset_rsp[0], nci_reset_rsp[1],
	nci_reset_rsp[2], nci_reset_rsp[3],
	nci_reset_rsp[4], nci_reset_rsp[5]);

	ret = 0;
	goto done;

err_nfcc_hw_check:
	ret = -ENXIO;
	printk("%s: - failed - HW not available\n", __func__);
done:
	/*Disable NFC by default to save power on boot*/
	pn553_platform_pinctrl_select(gpctrl, st_ven_l);/* ULPM: Disable */
	return ret;
}

static int pn553_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int ret=0;
	struct pn553_dev *pn553_dev;
	struct device_node *node;

	printk("%s: start...\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	printk("%s: step02 is ok\n", __func__);

	pn553_dev = kzalloc(sizeof(*pn553_dev), GFP_KERNEL);
	//printk("pn553_dev=%p\n", pn553_dev);

	if (pn553_dev == NULL)
	{
		dev_err(&client->dev, "pn553 failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	memset(pn553_dev, 0, sizeof(struct pn553_dev));

	printk("%s: step03 is ok\n", __func__);

	pn553_dev->client = client;

	/* init mutex and queues */
	init_waitqueue_head(&pn553_dev->read_wq);
	mutex_init(&pn553_dev->read_mutex);
	spin_lock_init(&pn553_dev->irq_enabled_lock);
#if 1
	pn553_dev->pn553_device.minor = MISC_DYNAMIC_MINOR;
	if (get_boot_mode() == FACTORY_BOOT) {
		pn553_dev->pn553_device.name = PN544_DRVNAME;
	} else {
		pn553_dev->pn553_device.name = PN553_DRVNAME;
	}
	pn553_dev->pn553_device.fops = &pn553_dev_fops;

	ret = misc_register(&pn553_dev->pn553_device);
	if (ret) {
		pr_err("%s: misc_register failed\n", __func__);
		goto err_misc_register;
	}
#endif
	printk("%s: step04 is ok\n", __func__);

	/* request irq.  the irq is set whenever the chip has data available
	* for reading.  it is cleared when all data has been read.
	*/

	memset(I2CDMAWriteBuf, 0x00, sizeof(I2CDMAWriteBuf));
	memset(I2CDMAReadBuf, 0x00, sizeof(I2CDMAReadBuf));

	printk("%s: step05 is ok\n", __func__);

	/*  NFC IRQ settings     */
	node = of_find_compatible_node(NULL, NULL, "mediatek,nfc-gpios");

	if (node) {
		of_property_read_u32_array(node, "gpio-irq",
					   &(pn553_dev->irq_gpio), 1);
		printk("pn553_dev->irq_gpio = %d\n", pn553_dev->irq_gpio);
	} else {
		pr_debug("%s : get gpio num err.\n", __func__);
		goto err_of_find_node;
	}
	node = of_find_compatible_node(NULL, NULL, "mediatek,irq_nfc-eint");
	//node = of_find_compatible_node(NULL, NULL, "mediatek, hall_4-eint");
	if (node) {
		client->irq = irq_of_parse_and_map(node, 0);
		printk("pn553 client->irq = %d\n", client->irq);

		ret = request_irq(client->irq, pn553_dev_irq_handler,
				IRQF_TRIGGER_HIGH, "nfc_eint_as_int", pn553_dev);

		if (ret) {
			pr_err("%s: EINT IRQ LINE NOT AVAILABLE, ret = %d\n", __func__, ret);
		} else {
			printk("%s: set EINT finished, client->irq=%d\n", __func__, client->irq);
			pn553_dev->irq_enabled = true;
			pn553_disable_irq(pn553_dev);
		}
	} else {
		pr_err("%s: can not find NFC eint compatible node\n", __func__);
	}

    i2c_set_clientdata(client, pn553_dev);

	printk("%s: successfully\n", __func__);

	return 0;

//err_request_hw_check_failed:
//	free_irq(client->irq, pn553_dev);
err_of_find_node:
	misc_deregister(&pn553_dev->pn553_device);
err_misc_register:
	mutex_destroy(&pn553_dev->read_mutex);
	kfree(pn553_dev);
	return ret;
}

static const struct i2c_device_id pn553_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static const struct of_device_id nfc_i2c_of_match[] = {
	{.compatible = "mediatek,pn553nfc"},
	{},
};

static struct i2c_driver pn553_i2c_driver = 
{
	.id_table	= pn553_id,
	.probe		= pn553_probe,
	.remove		= pn553_remove,
	/* .detect	= pn553_detect, */
	.driver		= {
		.name = "pn553",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nfc_i2c_of_match,
#endif
	},
};


static int pn553_platform_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;

	printk("%s\n", __func__);

	gpctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gpctrl)) {
		dev_err(&pdev->dev, "Cannot find pinctrl!");
		ret = PTR_ERR(gpctrl);
		goto end;
	}

	st_ven_h = pinctrl_lookup_state(gpctrl, "ven_high");
	if (IS_ERR(st_ven_h)) {
		ret = PTR_ERR(st_ven_h);
		printk("%s: pinctrl err, ven_high\n", __func__);
	}

	st_ven_l = pinctrl_lookup_state(gpctrl, "ven_low");
	if (IS_ERR(st_ven_l)) {
		ret = PTR_ERR(st_ven_l);
		printk("%s: pinctrl err, ven_low\n", __func__);
	}
//modify by tzf@meitu.begin
	st_dwn_h = pinctrl_lookup_state(gpctrl, "dwn_high");
	if (IS_ERR(st_dwn_h)) {
		ret = PTR_ERR(st_dwn_h);
		printk("%s: pinctrl err, dwn_high\n", __func__);
	}

	st_dwn_l = pinctrl_lookup_state(gpctrl, "dwn_low");
	if (IS_ERR(st_dwn_l)) {
		ret = PTR_ERR(st_dwn_l);
		printk("%s: pinctrl err, dwn_low\n", __func__);
	}

	st_eint_int = pinctrl_lookup_state(gpctrl, "eint_as_int");
	if (IS_ERR(st_eint_int)) {
		ret = PTR_ERR(st_eint_int);
		printk("%s: pinctrl err, st_eint_int\n", __func__);
	}
	/* select state */
	pn553_platform_pinctrl_select(gpctrl, st_ven_l);
	usleep_range(900, 1000);

	pn553_platform_pinctrl_select(gpctrl, st_dwn_l);
	usleep_range(900, 1000);

	pn553_platform_pinctrl_select(gpctrl, st_eint_int);
	usleep_range(900, 1000);

end:
	return ret;
}

static int pn553_platform_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk("%s: &pdev=%p\n", __func__, pdev);

	/* pinctrl init */
	ret = pn553_platform_pinctrl_init(pdev);

	return ret;
}

static int pn553_platform_remove(struct platform_device *pdev)
{
	printk("%s: &pdev=%p\n", __func__, pdev);

	return 0;
}

/*  platform driver */
static const struct of_device_id pn553_platform_of_match[] = {
	{.compatible = "mediatek,nfc-gpios",},
	{},
};

static struct platform_driver pn553_platform_driver = {
	.probe		= pn553_platform_probe,
	.remove		= pn553_platform_remove,
	.driver		= {
		.name = PN553_DRVNAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = pn553_platform_of_match,
#endif
	},
};

//#ifdef ODM_WT_EDIT
//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2019/10/09,add for load nfc driver according to boardid
static int __init nfc_board_id_get(char *str)
{

    strncpy(nfc_hardware_info, str,strlen(str)+1);
    pr_info("%s BoardID= %s\n ",__func__,nfc_hardware_info);
    return 0;
}
__setup("board_id=", nfc_board_id_get);
//#endif /* ODM_WT_EDIT */


/*
 * module load/unload record keeping
 */
static int __init pn553_dev_init(void)
{
	int ret;
	printk("pn553_dev_init\n");

	if((strncmp(nfc_hardware_info,"S98670GA1 ",strlen("S98670GA1")) == 0)||
       (strncmp(nfc_hardware_info,"S98670HA1",strlen("S98670HA1")) == 0)||
       (strncmp(nfc_hardware_info,"S98670JA1",strlen("S98670JA1")) == 0)||
       (strncmp(nfc_hardware_info,"S98675JA1",strlen("S98675JA1")) == 0)||
       (strncmp(nfc_hardware_info,"S98675KA1",strlen("S98675KA1")) == 0)||
       (strncmp(nfc_hardware_info,"S98675LA1",strlen("S98675LA1")) == 0)){
          ret = platform_driver_register(&pn553_platform_driver);
	      printk("[pn553] platform_driver_register  ret = %d \n", ret);

	      ret = i2c_add_driver(&pn553_i2c_driver);
	      printk("[pn553] i2c_add_driver  ret = %d \n", ret);

		  printk("pn553_dev_init success\n");

		  return ret;
    }else{
          pr_info(" %s This Board doesn't support NFC,BoardID:%s Exit! \n",__func__,nfc_hardware_info);
          return -1;
    }
    
}
module_init(pn553_dev_init);

static void __exit pn553_dev_exit(void)
{
	printk("pn553_dev_exit\n");

	i2c_del_driver(&pn553_i2c_driver);
}
module_exit(pn553_dev_exit);

MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("NFC PN553 driver");
MODULE_LICENSE("GPL");

