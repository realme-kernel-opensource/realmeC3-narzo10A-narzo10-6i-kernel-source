#include <linux/module.h>
#include <linux/slab.h>
#include <soc/oppo/mmkey_log.h>

//#ifdef VENDOR_EDIT
//Xiaoshan.Huang@PSW.NW.EM.2415682,2019/10/13
//modify for modem crash log
//#include <linux/switch.h>
#include <criticallog_class.h>
//#endif /* VENDOR_EDIT */

#include <linux/kthread.h>
#include <linux/wait.h>
//John.Xu@PhoneSw.AudioDriver.OppoDebug.CriticalLog, 2016/02/22, Add for critical_log

static int keylog_registered = 0;
//#ifdef VENDOR_EDIT
//Xiaoshan.Huang@PSW.NW.EM.2415682,2019/10/13
//modify for modem crash log
//static struct switch_dev keylog_swthdev;
static struct criticallog_dev keylog_swthdev;
//#endif /* VENDOR_EDIT */

static wait_queue_head_t keylog_thread_wq;
static struct task_struct *keylog_thread_task = NULL;

#define KEYLOG_BUF_MAX 5
//Wentian.Mai@PSW.NW.EM.1448074, 2018/06/26
//Add for monitor modem crash
#define LOGNAME_BUF_LEN 2148

struct keylog_data{
    char buf[LOGNAME_BUF_LEN];
    int id;
};
struct keylog_buf{
    spinlock_t          keylog_lock;
    unsigned int        index_write;
    unsigned int        index_read;
    struct keylog_data  data[KEYLOG_BUF_MAX];
};

static struct keylog_buf gKeylog_buf;


void mm_keylog_write(const char *logmessage, const char *cause, int id) {
   if(keylog_thread_task && keylog_registered){
        struct keylog_data * keylog_data;
        spin_lock(&gKeylog_buf.keylog_lock);
        keylog_data = &gKeylog_buf.data[gKeylog_buf.index_write%KEYLOG_BUF_MAX];
        snprintf(keylog_data->buf, LOGNAME_BUF_LEN,"log:%s cause:%s\n", logmessage,cause);
        keylog_data->id = id;
        gKeylog_buf.index_write++;
        spin_unlock(&gKeylog_buf.keylog_lock);
        wake_up_interruptible(&keylog_thread_wq);
   }
}

void mm_keylog_write_modemdump(unsigned int hashId, const char *cause, int id) {
   if(keylog_thread_task && keylog_registered){
        struct keylog_data * keylog_data;
        spin_lock(&gKeylog_buf.keylog_lock);
        keylog_data = &gKeylog_buf.data[gKeylog_buf.index_write%KEYLOG_BUF_MAX];
        snprintf(keylog_data->buf, LOGNAME_BUF_LEN,"fid:%u;cause:%s\n", hashId, cause);
        keylog_data->id = id;
        gKeylog_buf.index_write++;
        spin_unlock(&gKeylog_buf.keylog_lock);
        wake_up_interruptible(&keylog_thread_wq);
   }
}

EXPORT_SYMBOL_GPL(mm_keylog_write);
EXPORT_SYMBOL_GPL(mm_keylog_write_modemdump);

//#ifdef VENDOR_EDIT
//Xiaoshan.Huang@PSW.NW.EM.2415682,2019/10/13
//modify for modem crash log
static ssize_t mm_keylog_printname(struct criticallog_dev *sdev, char *buf) {
    struct keylog_data *keylog_data = &gKeylog_buf.data[gKeylog_buf.index_read%KEYLOG_BUF_MAX];
    pr_err("name = %s\n", keylog_data->buf);
    return snprintf(buf, LOGNAME_BUF_LEN,"%s", keylog_data->buf);
}
//else
//static ssize_t mm_keylog_printname(struct switch_dev *sdev, char *buf) {
//    struct keylog_data	*keylog_data = &gKeylog_buf.data[gKeylog_buf.index_read%KEYLOG_BUF_MAX];
//    return snprintf(buf, LOGNAME_BUF_LEN,"%s", keylog_data->buf);
//}
//#endif /* VENDOR_EDIT */


static int keylog_thread(void *data){
    while(1){
        wait_event_interruptible(keylog_thread_wq, (gKeylog_buf.index_write - gKeylog_buf.index_read));
        while(gKeylog_buf.index_write > gKeylog_buf.index_read){
            struct keylog_data *keylog_data = &gKeylog_buf.data[gKeylog_buf.index_read%KEYLOG_BUF_MAX];
            pr_err("logname lenth = %d\n", (int)strlen(keylog_data->buf));
            //#ifdef VENDOR_EDIT
            //Xiaoshan.Huang@PSW.NW.EM.2415682,2019/10/13
            //modify for modem crash log
            criticallog_set_state(&keylog_swthdev, keylog_data->id);
            criticallog_set_state(&keylog_swthdev, -keylog_data->id);
            //else
            //switch_set_state(&keylog_swthdev, keylog_data->id);
            //switch_set_state(&keylog_swthdev, -keylog_data->id);
            //#endif /* VENDOR_EDIT */
            gKeylog_buf.index_read++;
        }
    }
    keylog_thread_task = NULL;
    return 0;
}


static int __init oppo_criticallog_init(void)
{
    int ret = 0;
    init_waitqueue_head(&keylog_thread_wq);
    keylog_swthdev.name = "oppo_critical_log";
    keylog_swthdev.print_name = mm_keylog_printname;
    //#ifdef VENDOR_EDIT
    //Xiaoshan.Huang@PSW.NW.EM.2415682,2019/10/13
    //modify for modem crash log
    ret = criticallog_dev_register(&keylog_swthdev);
    //else
    //ret = switch_dev_register(&keylog_swthdev);
    //#endif /* VENDOR_EDIT */
    if(ret){
        goto keylog_err;
    }
    spin_lock_init(&gKeylog_buf.keylog_lock);
    gKeylog_buf.index_write = 0;
    gKeylog_buf.index_read = 0;
    keylog_thread_task = kthread_create(keylog_thread, (void *)&gKeylog_buf,"keylog");
    if(!keylog_thread_task){
        ret = -1;
        goto keylog_err;
    }
    keylog_registered = 1;
    wake_up_process(keylog_thread_task);
    return 0;
keylog_err:
    criticallog_dev_unregister(&keylog_swthdev);
    keylog_registered = 0;
    return ret;
}
arch_initcall(oppo_criticallog_init);

MODULE_DESCRIPTION("OPPO critical log");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("John.Xu <xuzhaoan@oppo.com>");
