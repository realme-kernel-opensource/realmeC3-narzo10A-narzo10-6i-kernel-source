#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <soc/oppo/oppo_project.h>

#include <linux/io.h>
#include <linux/of.h>
#ifdef CONFIG_MTK_SECURITY_SW_SUPPORT
#include <sec_boot_lib.h>
#endif
//#ifdef ODM_WT_EDIT
//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2019/11/30,add for NFC co-binary
#include <linux/syscalls.h>
//#endif /*ODM_WT_EDIT*/


/////////////////////////////////////////////////////////////
static struct proc_dir_entry *oppoVersion = NULL;

static ProjectInfoCDTType *format = NULL;
static ProjectInfoCDTType projectInfo = {
	.nProject		= 0,
	.nModem			= 0,
	.nOperator		= 0,
	.nPCBVersion	= 0,
	.engVersion		= 0,
	.isConfidential	= 1,
};

//#ifdef ODM_WT_EDIT
//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2019/11/30,add for NFC co-binary
static const char* nfc_feature = "nfc_feature";
static const char* feature_src = "/vendor/etc/nfc/com.oppo.nfc_feature.xml";
//#endif /* ODM_WT_EDIT */

static unsigned int init_project_version(void)
{
	struct device_node *np = NULL;
	int ret = 0;
    printk("init_project_version start\n");
	if(format == NULL)
		format = &projectInfo;

	np = of_find_node_by_name(NULL, "oppo_project");
	if(!np){
		printk("init_project_version error1");
		return 0;
	}

	ret = of_property_read_u32(np,"nProject",&(format->nProject));
	if(ret)
	{
		printk("init_project_version error2");
		return 0;
	}

	ret = of_property_read_u32(np,"nModem",&(format->nModem));
	if(ret)
	{
		printk("init_project_version error3");
		return 0;
	}

	ret = of_property_read_u32(np,"nOperator",&(format->nOperator));
	if(ret)
	{
		printk("init_project_version error4");
		return 0;
	}

	ret = of_property_read_u32(np,"nPCBVersion",&(format->nPCBVersion));
	if(ret)
	{
		printk("init_project_version error5");
		return 0;
	}

	ret = of_property_read_u32(np,"engVersion",&(format->engVersion));
	if(ret)
	{
		printk("init_project_version error6");
		return 0;
	}

	ret = of_property_read_u32(np,"isConfidential",&(format->isConfidential));
	if(ret)
	{
		printk("init_project_version error7");
		return 0;
	}

	printk("KE Version Info :Project(%d) Modem(%d) Operator(%d) PCB(%d) engVersion(%d) Confidential(%d)\n",
		format->nProject,format->nModem,format->nOperator,format->nPCBVersion, format->engVersion, format->isConfidential);

	return format->nProject;
}


unsigned int get_project(void)
{
	if(format)
		return format->nProject;
	else
		return init_project_version();
	return 0;
}

/* xiang.fei@PSW.MM.AudioDriver.Machine, 2018/05/28, Add for kernel driver */
EXPORT_SYMBOL(get_project);

//#ifdef VENDOR_EDIT
//Anhui.Sun@PSW.NW.RF, 2019/10/12, add for mtk version
#ifdef CONFIG_TARGET_BUILD_MTK_VERSION
EXPORT_SYMBOL(get_PCB_Version);
#endif
//#endif  /*VENDOR_EDIT*/

unsigned int is_project(OPPO_PROJECT project )
{
	return (get_project() == project?1:0);
}

#ifdef ODM_WT_EDIT
//Mingyao.Xie@ODM_WT.BSP.Storage.Usb, 2019/11/05, Modify for USB
EXPORT_SYMBOL(is_project);
#endif /*ODM_WT_EDIT*/

unsigned int get_PCB_Version(void)
{
	if(format)
		return format->nPCBVersion;
	return 0;
}

unsigned int get_Modem_Version(void)
{
	if(format)
		return format->nModem;
	return 0;
}

unsigned int get_Operator_Version(void)
{
	if (format == NULL) {
		init_project_version();
	}
	if(format)
		return format->nOperator;
	return 0;
}

unsigned int get_eng_version(void)
{
	if(format)
		return format->engVersion;
	return 0;
}

unsigned int is_confidential(void)
{
	if(format)
		return format->isConfidential;
	return 1;
}


//#ifdef ODM_WT_EDIT
//Yuexiu.Wu@ODM_WT.WCN.NFC.Basic.1941873,2019/11/30,add for NFC co-binary
static int __init update_feature(void)
{
        mm_segment_t fs;
        fs = get_fs();
        pr_err("update_feature, Operator Version [%d]", get_Operator_Version());
        set_fs(KERNEL_DS);
        if (oppoVersion) {
            if (get_Operator_Version() == OPERATOR_19747_RUSSIA || get_Operator_Version() == OPERATOR_19741_RUSSIA) {
                proc_symlink(nfc_feature, oppoVersion, feature_src);
            }
        }
        set_fs(fs);
        return 0;
}
late_initcall(update_feature);
//#endif /* ODM_WT_EDIT */

//this module just init for creat files to show which version
static ssize_t prjVersion_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;
	len = sprintf(page,"%d",get_project());

	if(len > *off)
		len -= *off;
	else
		len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
		return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);
}

static struct file_operations prjVersion_proc_fops = {
	.read = prjVersion_read_proc,
	.write = NULL,
};


static ssize_t pcbVersion_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;

	len = sprintf(page,"%d",get_PCB_Version());

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}

static struct file_operations pcbVersion_proc_fops = {
	.read = pcbVersion_read_proc,
};

static ssize_t engVersion_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;

	len = sprintf(page,"%d",get_eng_version());

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}

static struct file_operations engVersion_proc_fops = {
	.read = engVersion_read_proc,
};

static ssize_t is_confidential_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;

	len = sprintf(page,"%d",is_confidential());

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}

static struct file_operations isConfidential_proc_fops = {
	.read = is_confidential_read_proc,
};

static ssize_t operatorName_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;

	len = sprintf(page,"%d",get_Operator_Version());

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}

static struct file_operations operatorName_proc_fops = {
	.read = operatorName_read_proc,
};


static ssize_t modemType_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;

	len = sprintf(page,"%d",get_Modem_Version());

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}

static struct file_operations modemType_proc_fops = {
	.read = modemType_read_proc,
};

static ssize_t secureType_read_proc(struct file *file, char __user *buf,
		size_t count,loff_t *off)
{
	char page[256] = {0};
	int len = 0;
        #ifndef VENDOR_EDIT
	//LiBin@Prd6.BaseDrv, 2016/11/03,, Add for MTK secureType node
	//void __iomem *oem_config_base;
	uint32_t secure_oem_config = 0;

	//oem_config_base = ioremap(0x58034 , 10);
	//secure_oem_config = __raw_readl(oem_config_base);
	//iounmap(oem_config_base);
        printk(KERN_EMERG "lycan test secure_oem_config 0x%x\n", secure_oem_config);
        #else
#ifdef CONFIG_MTK_SECURITY_SW_SUPPORT
	int secure_oem_config = 0;
	secure_oem_config = sec_schip_enabled();
	printk(KERN_EMERG "lycan test secure_oem_config %d\n", secure_oem_config);
#else
	uint32_t secure_oem_config = 0;
#endif
        #endif /* VENDOR_EDIT */

	len = sprintf(page,"%d", secure_oem_config);

	if(len > *off)
	   len -= *off;
	else
	   len = 0;

	if(copy_to_user(buf,page,(len < count ? len : count))){
	   return -EFAULT;
	}
	*off += len < count ? len : count;
	return (len < count ? len : count);

}


static struct file_operations secureType_proc_fops = {
	.read = secureType_read_proc,
};

/*Yang.Tan@BSP.Fingerprint.Secure 2018/12/17 Add serialID for fastboot unlock*/
#define SERIALNO_LEN 16
extern char *saved_command_line;
static ssize_t serialID_read_proc(struct file *file, char __user *buf,
                size_t count, loff_t *off)
{
        char page[256] = {0};
        int len = 0;
        char * ptr;
        char serialno[SERIALNO_LEN+1] = {0};

        ptr = strstr(saved_command_line, "androidboot.serialno=");
        ptr += strlen("androidboot.serialno=");
        strncpy(serialno, ptr, SERIALNO_LEN);
        serialno[SERIALNO_LEN] = '\0';

        len = sprintf(page, "0x%s", serialno);
        if (len > *off) {
                len -= *off;
        }
        else{
                len = 0;
        }

        if (copy_to_user(buf, page, (len < count ? len : count))) {
                return -EFAULT;
        }

        *off += len < count ? len : count;
        return (len < count ? len : count);
}

struct file_operations serialID_proc_fops = {
        .read = serialID_read_proc,
};

static int __init oppo_project_init(void)
{
	int ret = 0;
	struct proc_dir_entry *pentry;

	if(oppoVersion) //if oppoVersion is not null means this proc dir has inited
	{
		return ret;
	}

	oppoVersion =  proc_mkdir("oppoVersion", NULL);
	if(!oppoVersion) {
		pr_err("can't create oppoVersion proc\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("prjVersion", S_IRUGO, oppoVersion, &prjVersion_proc_fops);
	if(!pentry) {
		pr_err("create prjVersion proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("pcbVersion", S_IRUGO, oppoVersion, &pcbVersion_proc_fops);
	if(!pentry) {
		pr_err("create pcbVersion proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("engVersion", S_IRUGO, oppoVersion, &engVersion_proc_fops);
	if(!pentry) {
		pr_err("create engVersion proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("isConfidential", S_IRUGO, oppoVersion, &isConfidential_proc_fops);
	if(!pentry) {
		pr_err("create is_confidential proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("operatorName", S_IRUGO, oppoVersion, &operatorName_proc_fops);
	if(!pentry) {
		pr_err("create operatorName proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("modemType", S_IRUGO, oppoVersion, &modemType_proc_fops);
	if(!pentry) {
		pr_err("create modemType proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	pentry = proc_create("secureType", S_IRUGO, oppoVersion, &secureType_proc_fops);
	if(!pentry) {
		pr_err("create secureType proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
/*Yang.Tan@BSP.Fingerprint.Secure 2018/12/17 Add serialID for fastboot unlock*/
	pentry = proc_create("serialID", S_IRUGO, oppoVersion, &serialID_proc_fops);
	if(!pentry) {
		pr_err("create serialID proc failed.\n");
		goto ERROR_INIT_VERSION;
	}
	return ret;
ERROR_INIT_VERSION:
	//remove_proc_entry("oppoVersion", NULL);
	return -ENOENT;
}


static int __init boot_oppo_project_core(void)
{
	init_project_version();
	oppo_project_init();
	return 0;
}

core_initcall(boot_oppo_project_core);

MODULE_DESCRIPTION("OPPO project version");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Joshua <gyx@oppo.com>");
