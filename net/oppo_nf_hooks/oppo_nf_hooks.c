/***********************************************************
** Copyright (C), 2008-2018, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: oppo_nf_hooks.c
** Description: Add for WeChat lucky money recognition
**
** Version: 1.0
** Date : 2018/06/18
** Author: Yuan.Huang@PSW.CN.WiFi.Network.internet.1461349
**
** ------------------ Revision History:------------------------
** <author> <data> <version > <desc>
** HuangYuan 2018/06/18 1.0 build this module
****************************************************************/

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/version.h>
#include <net/tcp.h>
#include <linux/random.h>
#include <net/sock.h>
#include <net/dst.h>
#include <linux/file.h>
#include <net/tcp_states.h>
#include <linux/netlink.h>
#include <net/sch_generic.h>
#include <net/pkt_sched.h>
#include <net/netfilter/nf_queue.h>
#include <linux/netfilter/xt_state.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_owner.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>


/*NLMSG_MIN_TYPE is 0x10,so we start at 0x11*/
enum{
    NF_HOOKS_ANDROID_PID    = 0x11,
	NF_HOOKS_WECHAT_PARAM   = 0x12,
	NF_HOOKS_LM_DETECTED    = 0x13,
};

#define MAX_FIXED_VALUE_LEN 20
#define MAX_WECHAT_PARAMS 10

struct wechat_pattern_info {
    u32 max_tot_len;
    u32 min_tot_len;
    int offset;
    u32 len;
    u8 fixed_value[MAX_FIXED_VALUE_LEN];
};

struct wechat_pattern_info wechat_infos[MAX_WECHAT_PARAMS];


static DEFINE_MUTEX(nf_hooks_netlink_mutex);
static struct ctl_table_header *oppo_nf_hooks_table_hrd;

static rwlock_t nf_hooks_lock;

#define nf_hooks_read_lock() 			read_lock_bh(&nf_hooks_lock);
#define nf_hooks_read_unlock() 			read_unlock_bh(&nf_hooks_lock);
#define nf_hooks_write_lock() 			write_lock_bh(&nf_hooks_lock);
#define nf_hooks_write_unlock()			write_unlock_bh(&nf_hooks_lock);

static u32 wechat_uid;
static u32 wechat_param_count;

static u32 nf_hooks_debug = 0;

//portid of android netlink socket
static u32 oppo_nf_hooks_pid;
//kernel sock
static struct sock *oppo_nf_hooks_sock;


/* send to user space */
static int oppo_nf_hooks_send_to_user(int msg_type, char *payload, int payload_len)
{
	int ret = 0;
	struct sk_buff *skbuff;
	struct nlmsghdr *nlh;

	/*allocate new buffer cache */
	skbuff = alloc_skb(NLMSG_SPACE(payload_len), GFP_ATOMIC);
	if (skbuff == NULL) {
		printk("oppo_nf_hooks_netlink: skbuff alloc_skb failed\n");
		return -1;
	}

	/* fill in the data structure */
	nlh = nlmsg_put(skbuff, 0, 0, msg_type, NLMSG_ALIGN(payload_len), 0);
	if (nlh == NULL) {
		printk("oppo_nf_hooks_netlink:nlmsg_put failaure\n");
		nlmsg_free(skbuff);
		return -1;
	}

	//compute nlmsg length
	nlh->nlmsg_len = NLMSG_HDRLEN + NLMSG_ALIGN(payload_len);

	if(NULL != payload){
		memcpy((char *)NLMSG_DATA(nlh),payload,payload_len);
	}

	/* set control field,sender's pid */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
	NETLINK_CB(skbuff).pid = 0;
#else
	NETLINK_CB(skbuff).portid = 0;
#endif

	NETLINK_CB(skbuff).dst_group = 0;

	/* send data */
	if(oppo_nf_hooks_pid){
		ret = netlink_unicast(oppo_nf_hooks_sock, skbuff, oppo_nf_hooks_pid, MSG_DONTWAIT);
	} else {
	    printk(KERN_ERR "oppo_nf_hooks_netlink: can not unicast skbuff, oppo_nf_hooks_pid=0\n");
	}
	if(ret < 0){
		printk(KERN_ERR "oppo_nf_hooks_netlink: can not unicast skbuff,ret = %d\n",ret);
		return 1;
	}

	return 0;
}


static bool is_wechat_skb(struct nf_conn *ct,struct sk_buff *skb)
{
	kuid_t k_wc_uid;
	kuid_t k_wc_fs_uid;
	kuid_t sk_uid;
	struct sock *sk = NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0))
	const struct file *filp = NULL;
#endif
	u32 wechat_fs_uid = wechat_uid + 99900000;

    if (ct->oppo_app_uid == -1 || wechat_uid == 0) {
        return false;
    } else if(ct->oppo_app_uid == 0) {

		sk = skb_to_full_sk(skb);
		if(NULL == sk || !sk_fullsock(sk) || NULL == sk->sk_socket){
			return false;
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0))
		filp = sk->sk_socket->file;
		if(NULL == filp){
			return false;
		}
		sk_uid = filp->f_cred->fsuid;
#else
		sk_uid = sk->sk_uid;
#endif

		k_wc_uid = make_kuid(&init_user_ns, wechat_uid);
		k_wc_fs_uid = make_kuid(&init_user_ns, wechat_fs_uid);
		if(uid_eq(sk_uid, k_wc_uid)){
		    ct->oppo_app_uid = wechat_uid;
		    //if (nf_hooks_debug) printk("oppo_nf_hooks_lm:this is wechat skb...\n");
		    return true;
		} else if (uid_eq(sk_uid, k_wc_fs_uid)) {
		    ct->oppo_app_uid = wechat_fs_uid;
		    //if (nf_hooks_debug) printk("oppo_nf_hooks_lm:this is wechat fs skb...\n");
		    return true;
		} else {
	        ct->oppo_app_uid = -1;
	        //if (nf_hooks_debug) printk("oppo_nf_hooks_lm:this is NOT wechat skb!!!\n");
	        return false;
	    }
	} else if(ct->oppo_app_uid == wechat_uid
	        || ct->oppo_app_uid == wechat_fs_uid) {
		return true;
	}

	return false;
}

/*
 *To detect incoming Lucky Money event.
*/
static unsigned int oppo_nf_hooks_lm_detect(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
    struct nf_conn *ct = NULL;
    enum ip_conntrack_info ctinfo;
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;
	u32 header_len, i;
	u8 *payload = NULL;
	u16 tot_len;

    ct = nf_ct_get(skb, &ctinfo);

	if(NULL == ct){
		return NF_ACCEPT;
	}

    if (is_wechat_skb(ct, skb)) {
    	if((iph = ip_hdr(skb)) != NULL && iph->protocol == IPPROTO_TCP){
    	    tot_len = ntohs(iph->tot_len);
    		tcph = tcp_hdr(skb);
    		header_len = iph->ihl * 4 + tcph->doff * 4;
            if (unlikely(skb_linearize(skb))) {
                return NF_ACCEPT;
            }
    		payload =(u8 *)(skb->data + header_len);
    	    for (i = 0; i < wechat_param_count; i++) {
    		    if (tot_len >= wechat_infos[i].min_tot_len && tot_len <= wechat_infos[i].max_tot_len) {
    		        if (memcmp(payload + wechat_infos[i].offset, wechat_infos[i].fixed_value, wechat_infos[i].len) == 0) {
    		            printk("oppo_nf_hooks_lm:i=%d received hong bao...\n", i);
    		            oppo_nf_hooks_send_to_user(NF_HOOKS_LM_DETECTED, NULL, 0);
    		            break;
    		        } else {
    		            if (nf_hooks_debug) printk("oppo_nf_hooks_lm:i=%d fixed value not match!!\n", i);
    		        }
    		    } else {
    		        if (nf_hooks_debug) printk("oppo_nf_hooks_lm:i=%d incorrect tot_len=%d\n", i, tot_len);
    		    }
    		}
    	}
    }

    return NF_ACCEPT;
}

static struct nf_hook_ops oppo_nf_hooks_ops[] __read_mostly = {
	{
		.hook		= oppo_nf_hooks_lm_detect,
		.pf		    = NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER + 1,
	},

	{ }
};

static struct ctl_table oppo_nf_hooks_sysctl_table[] = {
	{
		.procname	= "wechat_uid",
		.data		= &wechat_uid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nf_hooks_debug",
		.data		= &nf_hooks_debug,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

static int oppo_nf_hooks_sysctl_init(void)
{
	oppo_nf_hooks_table_hrd = register_net_sysctl(&init_net, "net/oppo_nf_hooks",
		                                          oppo_nf_hooks_sysctl_table);
	return oppo_nf_hooks_table_hrd == NULL ? -ENOMEM : 0;
}

static int oppo_nf_hooks_set_android_pid(struct sk_buff *skb)
{
    oppo_nf_hooks_pid = NETLINK_CB(skb).portid;
    printk("oppo_nf_hooks_set_android_pid pid=%d\n",oppo_nf_hooks_pid);
	return 0;
}

static int oppo_nf_hooks_set_wechat_param(struct nlmsghdr *nlh)
{
    int i, j;
    u32 *data;
    struct wechat_pattern_info * info = NULL;
    data = (u32 *)NLMSG_DATA(nlh);
    wechat_uid = *data;
    wechat_param_count = *(data + 1);
    if (nlh->nlmsg_len == NLMSG_HDRLEN + 2*sizeof(u32) + wechat_param_count*sizeof(struct wechat_pattern_info)) {
        for (i = 0; i < wechat_param_count && i < MAX_WECHAT_PARAMS; i++) {
            info = (struct wechat_pattern_info *)(data + 2) + i;
            memset(&(wechat_infos[i]), 0, sizeof(struct wechat_pattern_info));
            //wechat_infos[i].uid = info->uid;
            wechat_infos[i].max_tot_len = info->max_tot_len;
            wechat_infos[i].min_tot_len = info->min_tot_len;
            wechat_infos[i].offset = info->offset;
            wechat_infos[i].len = info->len;
            memcpy(wechat_infos[i].fixed_value, info->fixed_value, info->len);
            if (nf_hooks_debug) {
                printk("oppo_nf_hooks_set_wechat_param i=%d uid=%d,max=%d,min=%d,offset=%d,len=%d,value=",
                        i, wechat_uid, wechat_infos[i].max_tot_len, wechat_infos[i].min_tot_len,
                        wechat_infos[i].offset, wechat_infos[i].len);
                for (j = 0; j < wechat_infos[i].len; j++) {
                    printk("%d -> %02x  ", j, wechat_infos[i].fixed_value[j]);
                }
                printk("\n");
            }
        }
	    return 0;
	} else {
	    if (nf_hooks_debug) printk("oppo_nf_hooks_set_wechat_param invalid param!! nlmsg_len=%d\n", nlh->nlmsg_len);
	    return -1;
	}
}

static int nf_hooks_netlink_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, struct netlink_ext_ack *extack)
{
	int ret = 0;

	switch (nlh->nlmsg_type) {
    	case NF_HOOKS_ANDROID_PID:
    		ret = oppo_nf_hooks_set_android_pid(skb);
    		break;
    	case NF_HOOKS_WECHAT_PARAM:
    		ret = oppo_nf_hooks_set_wechat_param(nlh);
    		break;
    	default:
    		return -EINVAL;
	}

	return ret;
}


static void nf_hooks_netlink_rcv(struct sk_buff *skb)
{
	mutex_lock(&nf_hooks_netlink_mutex);
	netlink_rcv_skb(skb, &nf_hooks_netlink_rcv_msg);
	mutex_unlock(&nf_hooks_netlink_mutex);
}

static int oppo_nf_hooks_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input	= nf_hooks_netlink_rcv,
	};

	oppo_nf_hooks_sock = netlink_kernel_create(&init_net, NETLINK_OPPO_NF_HOOKS, &cfg);
	return oppo_nf_hooks_sock == NULL ? -ENOMEM : 0;
}

static void oppo_nf_hooks_netlink_exit(void)
{
	netlink_kernel_release(oppo_nf_hooks_sock);
	oppo_nf_hooks_sock = NULL;
}

static int __init oppo_nf_hooks_init(void)
{
	int ret = 0;

	ret = oppo_nf_hooks_netlink_init();
	if (ret < 0) {
		printk("oppo_nf_hooks_init module failed to init netlink.\n");
	} else {
		printk("oppo_nf_hooks_init module init netlink successfully.\n");
	}

	ret |= oppo_nf_hooks_sysctl_init();

	ret |= nf_register_net_hooks(&init_net,oppo_nf_hooks_ops,ARRAY_SIZE(oppo_nf_hooks_ops));
	if (ret < 0) {
		printk("oppo_nf_hooks_init module failed to register netfilter ops.\n");
	} else {
		printk("oppo_nf_hooks_init module register netfilter ops successfully.\n");
	}

	return ret;
}

static void __exit oppo_nf_hooks_fini(void)
{
    rwlock_init(&nf_hooks_lock);

	oppo_nf_hooks_netlink_exit();

	if(oppo_nf_hooks_table_hrd){
		unregister_net_sysctl_table(oppo_nf_hooks_table_hrd);
	}

	nf_unregister_net_hooks(&init_net,oppo_nf_hooks_ops, ARRAY_SIZE(oppo_nf_hooks_ops));
}

module_init(oppo_nf_hooks_init);
module_exit(oppo_nf_hooks_fini);
