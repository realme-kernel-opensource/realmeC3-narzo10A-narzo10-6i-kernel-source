/***********************************************************
** Copyright (C), 2008-2018, OPPO Mobile Comm Corp., Ltd.
** VENDOR_EDIT
** File: - oppo_sla.c
** Description: sla
**
** Version: 1.0
** Date : 2018/04/03
** Author: Junyuan.Huang@PSW.CN.WiFi.Network.internet.1197891
**
** ------------------ Revision History:------------------------
** <author> <data> <version > <desc>
** huangjunyuan 2018/04/03 1.0 build this module
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
#include <linux/workqueue.h>
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
#include <linux/netfilter_ipv4/ipt_REJECT.h>


#define MARK_MASK    0x0fff
#define RETRAN_MASK  0xf000
#define RTT_MASK     0xf000
#define GAME_UNSPEC_MASK 0x8000

#define WLAN_INDEX      0
#define CELLULAR_INDEX  1
#define DOWN_LOAD_FLAG  5
#define MAX_SYN_RETRANS 5
#define RTT_NUM         5

#define NORMAL_RTT      100
#define BACK_OFF_RTT_1	200   //200ms
#define BACK_OFF_RTT_2	350   //300ms
#define SYN_RETRAN_RTT	300   //500ms
#define MAX_RTT         500


#define CALC_DEV_SPEED_TIME 1
#define RECALC_WEIGHT_TIME  5    //5 seconds to recalc weight
#define MAX_SYN_NEW_COUNT   10   //statitis new syn count max number
#define LITTLE_FLOW_TIME    60   //small flow detect time internal

#define DOWNLOAD_SPEED      200   // download speed level min
#define MAX_CELLULAR_SPEED  300  //LTE CALC WEITHT MAX SPEED
#define MAX_WLAN_SPEED      1000  //WIFI CALC WEITHT MAX SPEED
#define LOW_RSSI_MAX_WLAN_SPEED 500


#define CALC_WEIGHT_MIN_SPEED_1 10 //10KB/s
#define CALC_WEIGHT_MIN_SPEED_2 50 //50KB/s
#define CALC_WEIGHT_MIN_SPEED_3 100 //100KB/s

#define GAME_NUM 7
#define IFACE_NUM 2
#define IFACE_LEN 16
#define WHITE_APP_BASE    100
#define WHITE_APP_NUM     64
#define MAX_GAME_RTT      300
#define MAX_DETECT_PKTS  100
#define UDP_RX_WIN_SIZE   20
#define TCP_RX_WIN_SIZE   5
#define TCP_DOWNLOAD_THRESHOLD  100*1024    //100KB/s
#define SLA_TIMER_EXPIRES HZ
#define MINUTE_LITTE_RATE  60     //60Kbit/s
#define INIT_APP_TYPE      0
#define UNKNOW_APP_TYPE    -1
#define WLAN_SCORE_BAD_NUM 5
#define GAME_SKB_TIME_OUT  120  //120s
#define WLAN_SCORE_GOOD    75
#define WLAN_SCORE_BAD     55
#define MIN_GAME_RTT       10 //ms
#define ENABLE_TO_USER_TIMEOUT 25 //second

#define MAX_FIXED_VALUE_LEN 20



/* dev info struct
  * if we need to consider wifi RSSI ?if we need consider screen state?
*/
struct oppo_dev_info{
	bool need_up;
	bool need_disable;
	int max_speed;    //KB/S
	int current_speed;
	int left_speed;
	int minute_speed; //kbit/s
	int download_flag;
	int congestion_flag;
	int if_up;
	int syn_retran;
	int wlan_score;
	int wlan_score_bad_count;
	int weight;
	int weight_state;
	int rtt_index;
	int rtt_congestion_count;
	u32 mark;
	u32 avg_rtt;
	u32 sum_rtt;     //ms
	u32 sla_rtt_num;     //ms
	u32 sla_sum_rtt;     //ms
	u32 sla_avg_rtt;     //ms
	u64 total_bytes;
	u64 minute_rx_bytes;
	char dev_name[IFACE_LEN];
};

struct oppo_speed_calc{
	int speed;
	int speed_done;
	int ms_speed_flag;
	u64	rx_bytes;
	u64 bytes_time;
	u64 first_time;
	u64 last_time;
	u64 sum_bytes;
};

struct oppo_sla_game_info{
	u32 game_type;
	u32 uid;
	u32 rtt;
	u32 mark;
	u32 switch_time;
	u32 rtt_150_num;
	u32 rtt_200_num;
	u32 rtt_250_num;
	u64 rtt_normal_num;
	u64 cell_bytes;
};


struct oppo_white_app_info{
    u32 count;
    u32 uid[WHITE_APP_NUM];
	u64 cell_bytes[WHITE_APP_NUM];
	u64 cell_bytes_normal[WHITE_APP_NUM];
};

struct oppo_game_online{
	bool game_online;
	struct timeval last_game_skb_tv;
	u32 udp_rx_pkt_count;   //count of all received game udp packets
	u64 tcp_rx_byte_count;  //count of all received game tcp bytes
};

struct game_traffic_info{
	bool game_in_front;
	u32 udp_rx_packet[UDP_RX_WIN_SIZE]; //udp packets received per second
	u64 tcp_rx_byte[TCP_RX_WIN_SIZE];   //tcp bytes received per second
	u32 window_index;
	u32 udp_rx_min; //min rx udp packets as valid game udp rx data
	u32 udp_rx_low_thr; //udp rx packet count low threshold
	u32 in_game_true_thr;  //greater than this -> inGame==true
	u32 in_game_false_thr;  //less than this -> inGame==false
	u32 rx_bad_thr; //greater than this -> game_rx_bad==true
};

struct oppo_syn_retran_statistic{
	u32 syn_retran_num;
	u32 syn_total_num;
};

struct oppo_rate_limit_info{
	int front_uid;
	int rate_limit_enable; //oppo_rate_limit enable or not
	int disable_rtt_num;
	int disable_rtt_sum;
	int disable_rtt;  //oppo_rate_limit disable front avg rtt
	int enable_rtt_num;
	int enable_rtt_sum;
	int enable_rtt;  //oppo_rate_limit enable front avg rtt
	struct timeval last_tv;
};

struct oppo_sla_rom_update_info{
	u32 sla_speed;        //sla speed threshold
	u32 cell_speed;       //cell max speed threshold
	u32 wlan_speed;       //wlan max speed threshold;
	u32 wlan_little_score_speed; //wlan little score speed threshold;
	u32 sla_rtt;          //sla rtt threshold
	u32 wzry_rtt;         //wzry rtt threshold
	u32 cjzc_rtt;         //cjzc rtt  threshold
	u32 wlan_bad_score;   //wifi bad score threshold
	u32 wlan_good_score;  //wifi good socre threshold
};

struct oppo_sla_game_rtt_params {
        int game_index;
        int tx_offset;
        int tx_len;
        u8  tx_fixed_value[MAX_FIXED_VALUE_LEN];
        int rx_offset;
        int rx_len;
        u8  rx_fixed_value[MAX_FIXED_VALUE_LEN];
};

enum{
	SLA_SKB_ACCEPT,
	SLA_SKB_CONTINUE,
	SLA_SKB_MARKED,
	SLA_SKB_REMARK,
	SLA_SKB_DROP,
};

enum{
	SLA_WEIGHT_NORMAL,
	SLA_WEIGHT_RECOVERY,
};

enum{
	WEIGHT_STATE_NORMAL,
	WEIGHT_STATE_USELESS,
	WEIGHT_STATE_RECOVERY,
};

enum{
	CONGESTION_LEVEL_NORMAL,
	CONGESTION_LEVEL_MIDDLE,
	CONGESTION_LEVEL_HIGH,
};

enum{
	WLAN_SCORE_LOW,
	WLAN_SCORE_HIGH,
};


enum{

	WLAN_MARK_BIT = 8,            //WLAN mark value,mask 0x0fff
	WLAN_MARK = (1 << WLAN_MARK_BIT),

	CELLULAR_MARK_BIT = 9,       //cellular mark value  mask 0x0fff
	CELLULAR_MARK = (1 << CELLULAR_MARK_BIT),

	RETRAN_BIT = 12,             //first retran mark value,  mask 0xf000
	RETRAN_MARK = (1 << RETRAN_BIT),

	RETRAN_SECOND_BIT = 13,     //second retran mark value, mask 0xf000
	RETRAN_SECOND_MARK = (1 << RETRAN_SECOND_BIT),

	RTT_MARK_BIT = 14,          //one ct only statitisc once rtt,mask 0xf000
	RTT_MARK = (1 << RTT_MARK_BIT),

	GAME_UNSPEC_MARK_BIT = 15,          //mark game skb when game not start
	GAME_UNSPEC_MARK = (1 << GAME_UNSPEC_MARK_BIT),

};


/*NLMSG_MIN_TYPE is 0x10,so we start at 0x11*/
enum{
	SLA_GET_WLAN_SCORE = 0x11,
	SLA_GET_PID = 0x12,
	SLA_ENABLE = 0x13,
	SLA_DISABLE = 0x14,
	SLA_WIFI_UP = 0x15,
	SLA_CELLULAR_UP = 0x16,
	SLA_WIFI_DOWN = 0x17,
	SLA_CELLULAR_DOWN = 0x18,
	SLA_CELLULAR_NET_MODE = 0x19,
	SLA_NOTIFY_APP_UID = 0x1A,
	SLA_NOTIFY_GAME_RTT = 0x1B,
	SLA_NOTIFY_WHITE_LIST_APP = 0x1C,
	SLA_ENABLED = 0x1D,
	SLA_DISABLED = 0x1E,
	SLA_ENABLE_GAME_RTT = 0x1F,
	SLA_DISABLE_GAME_RTT = 0x20,
	SLA_NOTIFY_SWITCH_STATE = 0x21,
	SLA_NOTIFY_SPEED_RTT = 0x22,
	SLA_SWITCH_GAME_NETWORK  = 0x23,
	SLA_NOTIFY_SCREEN_STATE	= 0x24,
	SLA_NOTIFY_CELL_QUALITY	= 0x25,
	SLA_SHOW_DIALOG_NOW = 0x26,
	SLA_NOTIFY_SHOW_DIALOG = 0x27,
	SLA_SEND_WHITE_LIST_APP_TRAFFIC = 0x28,
	SLA_SEND_GAME_APP_STATISTIC = 0x29,
	SLA_GET_SYN_RETRAN_INFO = 0x2A,
	SLA_GET_SPEED_UP_APP = 0x2B,
	SLA_SET_DEBUG = 0x2C,
	SLA_NOTIFY_DEFAULT_NETWORK = 0x2D,
	SLA_NOTIFY_PARAMS = 0x2E,
	SLA_NOTIFY_GAME_STATE = 0x2F,
	SLA_NOTIFY_GAME_PARAMS = 0x30,
	SLA_NOTIFY_GAME_RX_PKT = 0x31,
	SLA_NOTIFY_GAME_IN_FRONT = 0x32,
};


enum{
	GAME_SKB_DETECTING = 0,
	GAME_SKB_COUNT_ENOUGH = 1,
	GAME_RTT_STREAM = 2,
	GAME_VOICE_STREAM = 4,
};

enum{
	GAME_UNSPEC = 0,
	GAME_WZRY = 1,
	GAME_CJZC,
	GAME_QJCJ,
	GAME_HYXD_NM,
	GAME_HYXD,
	GAME_HYXD_ALI,
};

static int enable_to_user;
static int oppo_sla_rtt_detect = 1;
static int game_mark = 0;
static bool inGame = false;
static bool game_cell_to_wifi = false;
static bool game_rx_bad = false;
static int udp_rx_show_toast = 0;
static int game_rtt_show_toast = 0;
static int oppo_sla_enable;
static int oppo_sla_debug = 0;
static int oppo_sla_calc_speed;
static int oppo_sla_def_net = 0;    //WLAN->0	CELL->1
static int send_pop_win_msg_num = 0;
static int game_start_state[GAME_NUM];
//add for android Q statictis tcp tx and tx
static u64 wlan0_tcp_tx = 0;
static u64 wlan0_tcp_rx = 0;



static bool sla_switch_enable = false;
static bool	sla_screen_on =	true;
static bool cell_quality_good = true;
static bool need_pop_window = true;

static volatile u32 oppo_sla_pid;
static struct sock *oppo_sla_sock;
static struct timer_list sla_timer;

static struct timeval last_speed_tv;
static struct timeval last_weight_tv;
static struct timeval last_minute_speed_tv;
static struct timeval last_enable_cellular_tv;
static struct timeval disable_cellular_tv;
static struct timeval calc_wlan_rtt_tv;
static struct timeval last_enable_to_user_tv;
static struct timeval last_send_pop_window_msg_tv;
static struct timeval last_calc_small_speed_tv;

static struct oppo_rate_limit_info rate_limit_info;
static struct oppo_game_online game_online_info;
static struct oppo_white_app_info white_app_list;
static struct oppo_sla_game_info game_uid[GAME_NUM];
static struct oppo_dev_info oppo_sla_info[IFACE_NUM];
static struct oppo_speed_calc oppo_speed_info[IFACE_NUM];
static struct oppo_syn_retran_statistic syn_retran_statistic;

static struct work_struct oppo_sla_work;
static struct workqueue_struct *workqueue_sla;

static DEFINE_MUTEX(sla_netlink_mutex);
static struct ctl_table_header *oppo_sla_table_hrd;

/*we statistic rtt when tcp state is TCP_ESTABLISHED,for somtimes(when the network has qos to let syn pass first)
  * the three handshark (syn-synack) rtt is  good but the network is worse.
*/
extern void (*statistic_dev_rtt)(struct sock *sk,long rtt);

/*sometimes when skb reject by iptables,
*it will retran syn which may make the rtt much big
*so just mark the stream(ct) with mark RTT_MARK when this happens
*/
extern void (*mark_streams_for_iptables_reject)(struct sk_buff *skb,enum ipt_reject_with reject_type);

static rwlock_t sla_lock;
static rwlock_t sla_rtt_lock;
static rwlock_t sla_game_lock;


#define sla_read_lock() 			read_lock_bh(&sla_lock);
#define sla_read_unlock() 			read_unlock_bh(&sla_lock);
#define sla_write_lock() 			write_lock_bh(&sla_lock);
#define sla_write_unlock()			write_unlock_bh(&sla_lock);

#define sla_rtt_write_lock() 		write_lock_bh(&sla_rtt_lock);
#define sla_rtt_write_unlock()		write_unlock_bh(&sla_rtt_lock);

#define sla_game_write_lock() 		write_lock_bh(&sla_game_lock);
#define sla_game_write_unlock()		write_unlock_bh(&sla_game_lock);


static struct oppo_sla_rom_update_info rom_update_info ={
	.sla_speed = 200,
	.cell_speed = MAX_CELLULAR_SPEED,
	.wlan_speed = MAX_WLAN_SPEED,
	.wlan_little_score_speed = LOW_RSSI_MAX_WLAN_SPEED,
	.sla_rtt = 250,
	.wzry_rtt = 200,
	.cjzc_rtt = 250,
	.wlan_bad_score = WLAN_SCORE_BAD,
	.wlan_good_score = WLAN_SCORE_GOOD,
};

static struct oppo_sla_game_rtt_params game_params[GAME_NUM];

static struct game_traffic_info default_traffic_info = {
	.game_in_front = 0,
	//.udp_rx_packet[UDP_RX_WIN_SIZE];
	//.tcp_rx_byte[TCP_RX_WIN_SIZE];
	.window_index = 0,
	.udp_rx_min = 3,
	.udp_rx_low_thr = 12,
	.in_game_true_thr = 15,
	.in_game_false_thr = 10,
	.rx_bad_thr = 4,
};

static struct game_traffic_info wzry_traffic_info = {
	.game_in_front = 0,
	//.udp_rx_packet[UDP_RX_WIN_SIZE];
	//.tcp_rx_byte[TCP_RX_WIN_SIZE];
	.window_index = 0,
	.udp_rx_min = 3,
	.udp_rx_low_thr = 12,
	.in_game_true_thr = 15,
	.in_game_false_thr = 10,
	.rx_bad_thr = 3,
};

static struct game_traffic_info cjzc_traffic_info = {
	.game_in_front = 0,
	//.udp_rx_packet[UDP_RX_WIN_SIZE];
	//.tcp_rx_byte[TCP_RX_WIN_SIZE];
	.window_index = 0,
	.udp_rx_min = 3,
	.udp_rx_low_thr = 14,
	.in_game_true_thr = 15,
	.in_game_false_thr = 10,
	.rx_bad_thr = 4,
};


/* send to user space */
static int oppo_sla_send_to_user(int msg_type,char *payload,int payload_len)
{
	int ret = 0;
	struct sk_buff *skbuff;
	struct nlmsghdr *nlh;

    if (!oppo_sla_pid) {
		printk("oppo_sla_netlink: oppo_sla_pid == 0!!\n");
		return -1;
	}

	/*allocate new buffer cache */
	skbuff = alloc_skb(NLMSG_SPACE(payload_len), GFP_ATOMIC);
	if (skbuff == NULL) {
		printk("oppo_sla_netlink: skbuff alloc_skb failed\n");
		return -1;
	}

	/* fill in the data structure */
	nlh = nlmsg_put(skbuff, 0, 0, msg_type, NLMSG_ALIGN(payload_len), 0);
	if (nlh == NULL) {
		printk("oppo_sla_netlink:nlmsg_put failaure\n");
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
	ret = netlink_unicast(oppo_sla_sock, skbuff, oppo_sla_pid, MSG_DONTWAIT);
	if(ret < 0){
		printk(KERN_ERR "oppo_sla_netlink: can not unicast skbuff,ret = %d\n",ret);
		return 1;
	}

	return 0;
}

static void back_iface_speed(void)
{
	int i = 0;
	int avg_rtt = 0;
	int rtt_num = 2;

	if(oppo_sla_info[WLAN_INDEX].wlan_score <= rom_update_info.wlan_bad_score){
		rtt_num = 1;
	}

	sla_rtt_write_lock();
	for(i = 0; i < IFACE_NUM; i++){
		if(oppo_sla_info[i].if_up){

			if(oppo_sla_info[i].sla_rtt_num >= rtt_num){
				avg_rtt = 0;
				if(oppo_sla_info[i].download_flag < DOWN_LOAD_FLAG){
					avg_rtt = oppo_sla_info[i].sla_sum_rtt / oppo_sla_info[i].sla_rtt_num;
					oppo_sla_info[i].sla_avg_rtt = avg_rtt;
				}

				if(oppo_sla_debug){
                	printk("oppo_sla_rtt: sla rtt ,num = %d,sum = %d,avg rtt[%d] = %d\n",
					 		oppo_sla_info[i].sla_rtt_num,oppo_sla_info[i].sla_sum_rtt,i,avg_rtt);
				}

				oppo_sla_info[i].sla_rtt_num = 0;
				oppo_sla_info[i].sla_sum_rtt = 0;

				if(avg_rtt >= BACK_OFF_RTT_2){

					oppo_sla_info[i].congestion_flag = CONGESTION_LEVEL_HIGH;
					oppo_sla_info[i].max_speed /= 2;
				}
				else if(avg_rtt >= BACK_OFF_RTT_1){

					oppo_sla_info[i].congestion_flag = CONGESTION_LEVEL_MIDDLE;
					oppo_sla_info[i].max_speed /= 2;
				}
				else{
					if(WEIGHT_STATE_NORMAL == oppo_sla_info[i].weight_state){
						oppo_sla_info[i].congestion_flag = CONGESTION_LEVEL_NORMAL;
					}
				}
			}
		}
	}
	sla_rtt_write_unlock();

	return;
}

static void reset_wlan_info(void)
{
	struct timeval tv;

	if(oppo_sla_info[WLAN_INDEX].if_up){

		do_gettimeofday(&tv);
		last_minute_speed_tv = tv;

		oppo_sla_info[WLAN_INDEX].sum_rtt= 0;
		oppo_sla_info[WLAN_INDEX].avg_rtt = 0;
		oppo_sla_info[WLAN_INDEX].rtt_index = 0;
		oppo_sla_info[WLAN_INDEX].sla_avg_rtt = 0;
		oppo_sla_info[WLAN_INDEX].sla_rtt_num = 0;
		oppo_sla_info[WLAN_INDEX].sla_sum_rtt = 0;
		oppo_sla_info[WLAN_INDEX].syn_retran = 0;
		oppo_sla_info[WLAN_INDEX].max_speed /= 2;
		oppo_sla_info[WLAN_INDEX].left_speed = 0;
		oppo_sla_info[WLAN_INDEX].current_speed = 0;
		oppo_sla_info[WLAN_INDEX].minute_speed = MINUTE_LITTE_RATE;
		oppo_sla_info[WLAN_INDEX].congestion_flag = CONGESTION_LEVEL_NORMAL;
	}

}

static int enable_oppo_sla_module(void)
{
	struct timeval tv;

	sla_write_lock();

	if(oppo_sla_info[WLAN_INDEX].if_up &&
		oppo_sla_info[CELLULAR_INDEX].if_up){

		//oppo_sla_debug = 1;
		oppo_sla_enable = 1;
		do_gettimeofday(&tv);
		last_weight_tv = tv;
		last_enable_cellular_tv = tv;
		oppo_sla_info[WLAN_INDEX].weight = 30;
		oppo_sla_info[CELLULAR_INDEX].weight = 100;
		printk("oppo_sla_netlink: enable\n");
		oppo_sla_send_to_user(SLA_ENABLED, NULL, 0);
	}

	sla_write_unlock();
	return 0;
}

//add for rate limit function to statistics front uid rtt
static void statistics_front_uid_rtt(int rtt,struct sock *sk)
{
	kuid_t uid;
	const struct file *filp = NULL;
	if(sk && sk_fullsock(sk)){

		if(NULL == sk->sk_socket){
			return;
		}

		filp = sk->sk_socket->file;
		if(NULL == filp){
			return;
		}

		if(rate_limit_info.front_uid){
			uid = make_kuid(&init_user_ns,rate_limit_info.front_uid);
			if(uid_eq(filp->f_cred->fsuid, uid)){
				if(rate_limit_info.rate_limit_enable){
					rate_limit_info.enable_rtt_num++;
					rate_limit_info.enable_rtt_sum += rtt;
				}
				else{
					rate_limit_info.disable_rtt_num++;
					rate_limit_info.disable_rtt_sum += rtt;
				}
			}
		}
	}
	return;
}

static void calc_rtt_by_dev_index(int index,int tmp_rtt,struct sock *sk)
{

	/*do not calc rtt when the screen
	* is off which may make the rtt too big
	*/

	if(!sla_screen_on){
	   return;
	}

	if(tmp_rtt < 30) {
		return;
	}

	if(tmp_rtt > MAX_RTT){
		tmp_rtt = MAX_RTT;
	}

	sla_rtt_write_lock();

	statistics_front_uid_rtt(tmp_rtt,sk);

	if(!rate_limit_info.rate_limit_enable){
		oppo_sla_info[index].rtt_index++;
		oppo_sla_info[index].sum_rtt += tmp_rtt;
	}

	sla_rtt_write_unlock();
	return;

}


static int find_dev_index_by_mark(__u32 mark)
{
	int i;

	for(i = 0; i < IFACE_NUM; i++){
		if(oppo_sla_info[i].if_up &&
			mark == oppo_sla_info[i].mark){
			return i;
		}
	}

	return -1;
}

static int calc_retran_syn_rtt(struct sk_buff *skb,struct nf_conn *ct)
{
	int index = -1;
	int ret = SLA_SKB_CONTINUE;
	int tmp_mark = ct->mark & MARK_MASK;
	int rtt_mark = ct->mark & RTT_MASK;

	if(rtt_mark & RTT_MARK){
		skb->mark = ct->mark;
		return SLA_SKB_MARKED;
	}

	index = find_dev_index_by_mark(tmp_mark);

	if(-1 != index){

		calc_rtt_by_dev_index(index,SYN_RETRAN_RTT,NULL);

		oppo_sla_info[index].syn_retran++;
		syn_retran_statistic.syn_retran_num++;

		ct->mark |= RTT_MARK;
		skb->mark = ct->mark;

		ret = SLA_SKB_MARKED;
	}

	syn_retran_statistic.syn_total_num++;

	return ret;
}

static int get_syn_retran_dev_index(struct nf_conn *ct)
{
	int index = -1;
	int tmp_mark = ct->mark & MARK_MASK;

	if(WLAN_MARK == tmp_mark){
		if(oppo_sla_info[CELLULAR_INDEX].if_up &&
			CONGESTION_LEVEL_HIGH == oppo_sla_info[WLAN_INDEX].congestion_flag &&
			CONGESTION_LEVEL_NORMAL == oppo_sla_info[CELLULAR_INDEX].congestion_flag){

			index = WLAN_INDEX;
		}
	}
	else if(CELLULAR_MARK == tmp_mark){
		if(oppo_sla_info[WLAN_INDEX].if_up &&
			oppo_sla_info[WLAN_INDEX].max_speed > CALC_WEIGHT_MIN_SPEED_3 &&
			CONGESTION_LEVEL_NORMAL == oppo_sla_info[WLAN_INDEX].congestion_flag &&
			CONGESTION_LEVEL_HIGH == oppo_sla_info[CELLULAR_INDEX].congestion_flag){

			index = CELLULAR_INDEX;
		}
	}

	return index;
}



/*icsk_syn_retries*/
static int syn_retransmits_packet_do_specail(struct sock * sk,
				struct nf_conn *ct,
				struct sk_buff *skb)
{
	int index = -1;
	int ret = SLA_SKB_CONTINUE;
	u32 tmp_retran_mark = sk->oppo_sla_mark & RETRAN_MASK;
	struct iphdr *iph;
	struct tcphdr *th = NULL;
	struct inet_connection_sock *icsk = inet_csk(sk);

	if((iph = ip_hdr(skb)) != NULL && iph->protocol == IPPROTO_TCP){

		th = tcp_hdr(skb);
		/*only statictis syn retran packet,
		 * sometimes some rst packet also will be here
		 */
		if(NULL != th && th->syn &&
		   !th->rst && !th->ack && !th->fin){

			ret = calc_retran_syn_rtt(skb,ct);

			if(RETRAN_SECOND_MARK & tmp_retran_mark){
				skb->mark = ct->mark;
				return SLA_SKB_MARKED;
			}

			if(!nf_ct_is_dying(ct) &&
				nf_ct_is_confirmed(ct)){

				index = get_syn_retran_dev_index(ct);

				if(-1 != index){
					ct->mark = 0x0;
					//reset the tcp rto,so that can be faster to rertan to another dev
					icsk->icsk_rto = TCP_RTO_MIN;

					sk->oppo_sla_mark |= RETRAN_MARK;

					/*del the ct information,so that the syn packet can be send to network successfully later.
					  *lookup nf_nat_ipv4_fn()
					  */
					nf_ct_kill(ct);
					ret = SLA_SKB_REMARK;
				}
			}
		}
		else {
			ret = SLA_SKB_ACCEPT;
		}
	}

	return ret;
}

static int mark_retransmits_syn_skb(struct sock * sk,struct nf_conn *ct)
{
	u32 tmp_mark = sk->oppo_sla_mark & MARK_MASK;
	u32 tmp_retran_mark = sk->oppo_sla_mark & RETRAN_MASK;

	if(RETRAN_MARK & tmp_retran_mark){
		if(WLAN_MARK == tmp_mark){
			return (CELLULAR_MARK | RETRAN_SECOND_MARK);
		}
		else if(CELLULAR_MARK == tmp_mark){
			return (WLAN_MARK | RETRAN_SECOND_MARK);
		}
	}
	return 0;
}

static void is_http_get(struct nf_conn *ct,struct sk_buff *skb,
				struct tcphdr *tcph,int header_len)
{

	u32 *payload = NULL;
	payload =(u32 *)(skb->data + header_len);

	if(0 == ct->oppo_http_flag && (80 == ntohs(tcph->dest))){

		if(	*payload == 0x20544547){//http get

			ct->oppo_http_flag = 1;
			ct->oppo_skb_count = 1;
		}
	}

	return;
}

static struct tcphdr * is_valid_http_packet(struct sk_buff *skb, int *header_len)
{
	int datalen = 0;
	int tmp_len = 0;
	struct tcphdr *tcph = NULL;
	struct iphdr *iph;

	if((iph = ip_hdr(skb)) != NULL &&
		iph->protocol == IPPROTO_TCP){

		tcph = tcp_hdr(skb);
		datalen = ntohs(iph->tot_len);
		tmp_len = iph->ihl * 4 + tcph->doff * 4;

		if((datalen - tmp_len) > 64){
			*header_len = tmp_len;
			return tcph;
		}
	}
	return NULL;
}

static void reset_dev_info_syn_retran(struct nf_conn *ct,
				struct sk_buff *skb)
{
	int index = -1;
	u32 tmp_mark;
	struct tcphdr *tcph;
	int header_len = 0;
	int syn_retran_num = 0;

	tmp_mark= ct->mark & MARK_MASK;
	index = find_dev_index_by_mark(tmp_mark);

	if(-1 != index){

		syn_retran_num = oppo_sla_info[index].syn_retran;

		if(syn_retran_num &&
			NULL != (tcph = is_valid_http_packet(skb,&header_len))){

			oppo_sla_info[index].syn_retran = 0;

		}
	}
}

static u32 get_skb_mark_by_weight(void)
{
	u32 sla_random = prandom_u32() & 0x7FFFFFFF;

	/*0x147AE15 = 0x7FFFFFFF /100 + 1; for we let the weight * 100 to void
	  *decimal point operation at linux kernel
	  */
	if ( sla_random < (0x147AE15 * oppo_sla_info[WLAN_INDEX].weight)){
		return oppo_sla_info[WLAN_INDEX].mark;
	}

	return oppo_sla_info[CELLULAR_INDEX].mark;

}

//maybe this method can let calc dev speed more fairness
static int set_skb_mark_by_default(void)
{
	static int mark_flag = 0;

	if(mark_flag && oppo_sla_info[WLAN_INDEX].if_up){
		mark_flag = 0;
		return WLAN_MARK & MARK_MASK;
	}
	else{
		if(oppo_sla_info[CELLULAR_INDEX].if_up){
			mark_flag = 1;
			return CELLULAR_MARK & MARK_MASK;
		}
	}

	return WLAN_MARK & MARK_MASK;
}


static void reset_oppo_sla_calc_speed(struct timeval tv)
{
	int time_interval = 0;
	time_interval = tv.tv_sec - last_calc_small_speed_tv.tv_sec;

	if(time_interval >= 60 &&
		oppo_speed_info[WLAN_INDEX].speed_done){

		oppo_sla_calc_speed = 0;
		oppo_speed_info[WLAN_INDEX].speed_done = 0;

	}
}

static int wlan_get_speed_prepare(struct sk_buff *skb)
{

	int header_len = 0;
	struct tcphdr *tcph = NULL;
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);

	if(NULL == ct){
		return NF_ACCEPT;
	}

	if(ctinfo == IP_CT_ESTABLISHED){
		tcph = is_valid_http_packet(skb,&header_len);
		if(tcph){
			is_http_get(ct,skb,tcph,header_len);
		}
	}
	return NF_ACCEPT;
}


static int get_wlan_syn_retran(struct sk_buff *skb)
{
	int tmp_mark;
	int rtt_mark;
	struct iphdr *iph;
	struct sock *sk = NULL;
	struct nf_conn *ct = NULL;
	struct tcphdr *th = NULL;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);

	if(NULL == ct){
		return NF_ACCEPT;
	}

	if(ctinfo == IP_CT_NEW &&
		(iph = ip_hdr(skb)) != NULL &&
		iph->protocol == IPPROTO_TCP){

		th = tcp_hdr(skb);
		/*only statictis syn retran packet,
		 * sometimes some rst packet also will be here
		 */
		if(NULL != th && th->syn &&
		   !th->rst && !th->ack && !th->fin){

			rtt_mark = ct->mark & RTT_MASK;
			tmp_mark = ct->mark & MARK_MASK;

			if(rtt_mark & RTT_MARK){
				return NF_ACCEPT;
			}

			if(tmp_mark != WLAN_MARK){
				struct net_device *dev = NULL;
				dev = skb_dst(skb)->dev;

				if (dev && !memcmp(dev->name,oppo_sla_info[WLAN_INDEX].dev_name,strlen(dev->name))) {
					if (oppo_sla_debug) {
						printk("oppo_dev_info: dev name = %s\n",dev->name);
					}
					ct->mark = WLAN_MARK;

				sk = skb_to_full_sk(skb);
				if(sk){
					sk->oppo_sla_mark = WLAN_MARK;
				}

					syn_retran_statistic.syn_total_num++;
				}
				return NF_ACCEPT;
			}

			syn_retran_statistic.syn_retran_num++;
			calc_rtt_by_dev_index(WLAN_INDEX,SYN_RETRAN_RTT,sk);

			ct->mark |= RTT_MARK;
		}
	}
	return NF_ACCEPT;
}

static void game_rtt_estimator(int game_type, u32 rtt)
{
	long m = rtt; /* RTT */
	int shift = 3;
	u32 srtt = 0;

	if(game_type &&
		game_type < GAME_NUM){

		srtt = game_uid[game_type].rtt;
		if(GAME_WZRY == game_type) {
			shift = 2;
		}

		if (srtt != 0) {

			m -= (srtt >> shift);
			srtt += m;		/* rtt = 7/8 rtt + 1/8 new */

		} else {
		    //void first time the rtt bigger than 200ms which will switch game network
		    m = m >> 2;
			srtt = m << shift;

		}
		game_uid[game_type].rtt = srtt;

		if(rtt >= 250){
			game_uid[game_type].rtt_250_num++;
		}
		else if(rtt >= 200){
			game_uid[game_type].rtt_200_num++;
		}
		else if(rtt >= 150){
			game_uid[game_type].rtt_150_num++;
		}
		else {
			game_uid[game_type].rtt_normal_num++;
		}
	}
	return;
}

static void game_app_switch_network(u32 game_type)
{
	int index = -1;
	u32 game_rtt = 0;
	u32 uid	= 0;
	int shift = 3;
	u32 time_now = 0;
	int max_rtt = MAX_GAME_RTT;
	int game_bp_info[4];
	bool wlan_bad = false;

	if(!oppo_sla_enable){
		return;
	}

	if(GAME_WZRY !=	game_type &&
	   GAME_CJZC != game_type){
		return;
	}

	if(oppo_sla_info[WLAN_INDEX].wlan_score_bad_count >= WLAN_SCORE_BAD_NUM){
		wlan_bad = true;
	}

	index = game_type;
	uid = game_uid[game_type].uid;

	if(!game_start_state[index] && !inGame){
		return;
	}

	if(GAME_WZRY == game_type) {
		shift = 2;
		max_rtt = rom_update_info.wzry_rtt;
	}
	else if(GAME_CJZC == game_type){
		max_rtt = rom_update_info.cjzc_rtt;
	}

	time_now = ktime_get_ns() / 1000000;
	game_rtt = game_uid[game_type].rtt >> shift;

	if(cell_quality_good &&
	   !game_cell_to_wifi &&
	   (wlan_bad || game_rx_bad || game_rtt >= max_rtt) &&
	   game_uid[index].mark == WLAN_MARK){
	   if(!game_uid[index].switch_time ||
	   	 (time_now - game_uid[index].switch_time) > 60000){

		    game_uid[game_type].rtt = 0;
			game_uid[index].switch_time = time_now;
			game_uid[index].mark = CELLULAR_MARK;

			memset(game_bp_info,0x0,sizeof(game_bp_info));
			game_bp_info[0] = game_type;
			game_bp_info[1] = CELLULAR_MARK;
			game_bp_info[2] = wlan_bad;
			game_bp_info[3] = cell_quality_good;
			oppo_sla_send_to_user(SLA_SWITCH_GAME_NETWORK,(char *)game_bp_info,sizeof(game_bp_info));
			printk("oppo_sla_game_rtt:uid = %u,game rtt = %u,wlan_bad = %d,changing to cellular...\n",uid,game_rtt,wlan_bad);
			return;
	   }
	}

	if(!wlan_bad &&
	   game_uid[index].mark == CELLULAR_MARK &&
	   (!cell_quality_good || game_rx_bad || game_rtt >= max_rtt)){

		if(!game_uid[index].switch_time ||
	   	 (time_now - game_uid[index].switch_time) > 60000){

		    game_uid[game_type].rtt = 0;
			game_uid[index].switch_time = time_now;
			game_uid[index].mark = WLAN_MARK;
			game_cell_to_wifi = true;

			memset(game_bp_info,0x0,sizeof(game_bp_info));
			game_bp_info[0] = game_type;
			game_bp_info[1] = WLAN_MARK;
			game_bp_info[2] = wlan_bad;
			game_bp_info[3] = cell_quality_good;
			oppo_sla_send_to_user(SLA_SWITCH_GAME_NETWORK,(char *)game_bp_info,sizeof(game_bp_info));
			printk("oppo_sla_game_rtt:uid = %u,game rtt = %u,wlan_bad = %d,changing to wlan...\n",uid,game_rtt,wlan_bad);
			return;
	   }
	}
	return;
}

static bool is_game_rtt_skb(struct nf_conn *ct, struct sk_buff *skb, bool isTx) {
        struct iphdr *iph = NULL;
        struct udphdr *udph = NULL;
        u32 header_len;
        u8 *payload = NULL;
        u32 gameType = ct->oppo_app_type;

        if (gameType <= 0 || gameType >= GAME_NUM || game_params[gameType].game_index == 0 ||
                skb->len > 150 || (iph = ip_hdr(skb)) == NULL || iph->protocol != IPPROTO_UDP) {
                //printk("oppo_sla_game_rtt_detect: not game skb.");
                return false;
        }

        if ((isTx && ct->oppo_game_up_count >= MAX_DETECT_PKTS) ||
                (!isTx && ct->oppo_game_down_count >= MAX_DETECT_PKTS)) {
                ct->oppo_game_detect_status = GAME_SKB_COUNT_ENOUGH;
                return false;
        }

        udph = udp_hdr(skb);
        header_len = iph->ihl * 4 + sizeof(struct udphdr);
        payload =(u8 *)(skb->data + header_len);
        if (isTx) {
                int tx_offset = game_params[gameType].tx_offset;
                int tx_len = game_params[gameType].tx_len;
                u8  tx_fixed_value[MAX_FIXED_VALUE_LEN];
                memcpy(tx_fixed_value, game_params[gameType].tx_fixed_value, MAX_FIXED_VALUE_LEN);
                if (udph->len >= (tx_offset + tx_len) && memcmp(payload + tx_offset, tx_fixed_value, tx_len) == 0) {
                        if (oppo_sla_debug) {
                                printk("oppo_sla_game_rtt_detect: this is game RTT Tx skb.\n");
                        }
                        ct->oppo_game_detect_status |= GAME_RTT_STREAM;
                        return true;
                }
        } else {
                int rx_offset = game_params[gameType].rx_offset;
                int rx_len = game_params[gameType].rx_len;
                u8  rx_fixed_value[MAX_FIXED_VALUE_LEN];
                memcpy(rx_fixed_value, game_params[gameType].rx_fixed_value, MAX_FIXED_VALUE_LEN);
                if (udph->len >= (rx_offset + rx_len) && memcmp(payload + rx_offset, rx_fixed_value, rx_len) == 0) {
                        if (oppo_sla_debug) {
                                printk("oppo_sla_game_rtt_detect: this is game RTT Rx skb.\n");
                        }
                        ct->oppo_game_detect_status |= GAME_RTT_STREAM;
                        return true;
                }
        }
        //printk("oppo_sla_game_rtt_detect: this is NOT game RTT skb.");
        return false;
}

static int mark_game_app_skb(struct nf_conn *ct,struct sk_buff *skb,enum ip_conntrack_info ctinfo)
{
	int game_index = -1;
	struct iphdr *iph = NULL;
	u32 ct_mark = 0;
	int ret = SLA_SKB_CONTINUE;

	if(ct->oppo_app_type > 0 && ct->oppo_app_type < GAME_NUM){
		ret = SLA_SKB_ACCEPT;
		game_index = ct->oppo_app_type;

		if(GAME_WZRY != game_index &&
		   GAME_CJZC != game_index){
			return ret;
		}

		ct_mark = GAME_UNSPEC_MASK & ct->mark;
		if(!game_start_state[game_index] && !inGame &&
		   (GAME_UNSPEC_MARK & ct_mark)){

			return SLA_SKB_ACCEPT;
		}

		iph = ip_hdr(skb);
		if(iph &&
		   (IPPROTO_UDP == iph->protocol ||
		    IPPROTO_TCP == iph->protocol)){

			//WZRY can not switch tcp packets
			if(GAME_WZRY == game_index &&
			   IPPROTO_TCP == iph->protocol) {
				return SLA_SKB_ACCEPT;
			}

			ct_mark	= ct->mark & MARK_MASK;

			if(GAME_CJZC == game_index &&
				IPPROTO_TCP == iph->protocol &&
				((XT_STATE_BIT(ctinfo) & XT_STATE_BIT(IP_CT_ESTABLISHED)) ||
				(XT_STATE_BIT(ctinfo) & XT_STATE_BIT(IP_CT_RELATED)))) {
				if(WLAN_MARK == ct_mark && oppo_sla_info[WLAN_INDEX].wlan_score > 40){
					return SLA_SKB_ACCEPT;
				}
				else if(CELLULAR_MARK == ct_mark){
					skb->mark = CELLULAR_MARK;
					return SLA_SKB_MARKED;
				}
			}

			if (game_mark) {
			    skb->mark = game_mark;
			} else {
			    skb->mark = game_uid[game_index].mark;
			}

			if(ct_mark && skb->mark &&
			   ct_mark != skb->mark){
			   printk("oppo_sla_game_rtt:reset game ct proto= %u,game type = %d,ct mark = %x,skb mark = %x\n",
					  iph->protocol,game_index,ct_mark,skb->mark);
			   game_uid[game_index].rtt = 0;

			   if(!nf_ct_is_dying(ct) &&
				  nf_ct_is_confirmed(ct)){
					nf_ct_kill(ct);
					return SLA_SKB_DROP;
			   }
			   else{
					skb->mark = ct_mark;
					ret = SLA_SKB_MARKED;
			   }
			}

			if(!ct_mark){
				ct->mark = (ct->mark & RTT_MASK) | game_uid[game_index].mark;
			}
			ret = SLA_SKB_MARKED;
		}
	}

	return ret;
}

static bool is_game_app_skb(struct nf_conn *ct,struct sk_buff *skb,enum ip_conntrack_info ctinfo)
{
	int i = 0;
	kuid_t uid;
	struct sock *sk = NULL;
	struct iphdr *iph = NULL;
	const struct file *filp = NULL;

	if(INIT_APP_TYPE == ct->oppo_app_type){

		sk = skb_to_full_sk(skb);
		if(NULL == sk || NULL == sk->sk_socket){
			return false;
		}

		filp = sk->sk_socket->file;
		if(NULL == filp){
			return false;
		}

        iph = ip_hdr(skb);

		for(i = 1;i < GAME_NUM;i++){
			if(game_uid[i].uid){
				uid = make_kuid(&init_user_ns,game_uid[i].uid);
				if(uid_eq(filp->f_cred->fsuid, uid)){
					ct->oppo_app_type = i;
					if(oppo_sla_enable &&
					   CELLULAR_MARK == game_uid[i].mark){
						game_uid[i].cell_bytes += skb->len;
					}
					//ct->mark = (ct->mark & RTT_MASK) | game_uid[i].mark;
					if(!game_start_state[i] && !inGame &&
						iph && IPPROTO_TCP == iph->protocol){
						ct->mark = (ct->mark & RTT_MASK) | WLAN_MARK;
						ct->mark |= GAME_UNSPEC_MARK;
					}
					else{
						if (game_mark) {
						    ct->mark = (ct->mark & RTT_MASK) | game_mark;
						} else {
						    ct->mark = (ct->mark & RTT_MASK) | game_uid[i].mark;
						}
					}
					return true;
				}
			}
		}
	}
	else if(ct->oppo_app_type > 0 &&
		ct->oppo_app_type < GAME_NUM){
		i = ct->oppo_app_type;
		if(oppo_sla_enable &&
		   !oppo_sla_def_net &&
		   CELLULAR_MARK == game_uid[i].mark){
			game_uid[i].cell_bytes += skb->len;
		}
		return true;
	}

	return false;

}

static int detect_game_up_skb(struct sk_buff *skb)
{
	struct timeval tv;
	struct iphdr *iph = NULL;
	struct nf_conn *ct = NULL;
	int ret = SLA_SKB_ACCEPT;
	enum ip_conntrack_info ctinfo;

	if(!oppo_sla_rtt_detect){
		return SLA_SKB_CONTINUE;
	}

	ct = nf_ct_get(skb, &ctinfo);
	if(NULL == ct){
		return SLA_SKB_ACCEPT;
	}

	if(!is_game_app_skb(ct,skb,ctinfo)){
		return SLA_SKB_CONTINUE;
	}

	do_gettimeofday(&tv);
	game_online_info.game_online = true;
	game_online_info.last_game_skb_tv = tv;

	//TCP and udp need to switch network
	ret = SLA_SKB_CONTINUE;
	iph = ip_hdr(skb);
	if(iph && IPPROTO_UDP == iph->protocol){
		if (ct->oppo_game_up_count < MAX_DETECT_PKTS &&
		        ct->oppo_game_detect_status == GAME_SKB_DETECTING) {
		        ct->oppo_game_up_count++;
		}
		//only udp packet can trigger switching network to avoid updating game with cell.
		sla_game_write_lock();
		game_app_switch_network(ct->oppo_app_type);
		sla_game_write_unlock();

		if (is_game_rtt_skb(ct, skb, true)) {
		        s64 time_now = ktime_get_ns() / 1000000;
		        if (ct->oppo_game_timestamp && time_now - ct->oppo_game_timestamp > 200) {
        		        //Tx done and no Rx, we've lost a response pkt
        		        ct->oppo_game_lost_count++;
        		        sla_game_write_lock();
        		        game_rtt_estimator(ct->oppo_app_type, MAX_GAME_RTT);
        		        sla_game_write_unlock();

            			if(game_rtt_show_toast) {
            			        u32 game_rtt = MAX_GAME_RTT;
            			        oppo_sla_send_to_user(SLA_NOTIFY_GAME_RTT,(char *)&game_rtt,sizeof(game_rtt));
            			}
    			        if(oppo_sla_debug) {
    			                u32 shift = 3;
    			                if(GAME_WZRY == ct->oppo_app_type) {
    			                        shift = 2;
    			                }
    			                printk("oppo_sla_game_rtt: lost packet!! game_rtt=%u, srtt=%u\n",
    			                        MAX_GAME_RTT, game_uid[ct->oppo_app_type].rtt >> shift);
    			        }
		        }
		        ct->oppo_game_timestamp = time_now;
		}
	}

	return ret;
}

static bool is_game_voice_packet(struct nf_conn *ct, struct sk_buff *skb) {
	struct iphdr *iph = NULL;
	struct udphdr *udph = NULL;
	u32 header_len;
	u8 *payload = NULL;
	u8 wzry_fixed_value[2] = {0x55, 0xf4};
	u32 wzry_offset = 9;
	u32 wzry_len = 2;
	u8 cjzc_fixed_value[3] = {0x10, 0x01, 0x01};
	u32 cjzc_offset = 4;
	u32 cjzc_len = 3;
	int game_type = ct->oppo_app_type;
	//u16 tot_len;

	if (ct->oppo_game_down_count >= MAX_DETECT_PKTS) {
	        ct->oppo_game_detect_status = GAME_SKB_COUNT_ENOUGH;
	        return false;
	}

	if ((iph = ip_hdr(skb)) != NULL && iph->protocol == IPPROTO_UDP) {
	        //tot_len = ntohs(iph->tot_len);
	        udph = udp_hdr(skb);
	        header_len = iph->ihl * 4 + sizeof(struct udphdr);
	        payload =(u8 *)(skb->data + header_len);
	        if (game_type == GAME_WZRY) {
        	        if (udph->len >= (wzry_offset + wzry_len) &&
            	                memcmp(payload + wzry_offset, wzry_fixed_value, wzry_len) == 0) {	//for cjzc voice pkt
                    	        //printk("oppo_sla_game_rx_voice:this is voice skb!\n");
                    	        ct->oppo_game_detect_status |= GAME_VOICE_STREAM;
                    	        return true;
        	        } else {
                    	        //memcpy(fixed_value, payload + 4, 3);
                    	        //printk("oppo_sla_game_rx_voice:this is NOT voice skb, value=%02x%02x%02x\n",
                    	        //        fixed_value[0], fixed_value[1], fixed_value[2]);
                    	        return false;
        	        }
	        } else if (game_type == GAME_CJZC) {
        	        if (udph->len >= (cjzc_offset + cjzc_len) &&
            	                memcmp(payload + cjzc_offset, cjzc_fixed_value, cjzc_len) == 0) {	//for cjzc voice pkt
                    	        //printk("oppo_sla_game_rx_voice:this is voice skb!\n");
                    	        ct->oppo_game_detect_status |= GAME_VOICE_STREAM;
                    	        return true;
        	        } else {
                    	        //memcpy(fixed_value, payload + 4, 3);
                    	        //printk("oppo_sla_game_rx_voice:this is NOT voice skb, value=%02x%02x%02x\n",
                    	        //        fixed_value[0], fixed_value[1], fixed_value[2]);
                    	        return false;
        	        }
	        }
	}
	return false;
}

static void record_sla_app_cell_bytes(struct nf_conn *ct, struct sk_buff *skb)
{
	int index = 0;
	u32 ct_mark = 0x0;
	//calc game or white list app cell bytes
	if(oppo_sla_enable &&
		!oppo_sla_def_net){
		if(ct->oppo_app_type > 0 &&
			ct->oppo_app_type < GAME_NUM){
			index = ct->oppo_app_type;
			if(CELLULAR_MARK == game_uid[index].mark){
				game_uid[index].cell_bytes += skb->len;
			}
		}
		else if(ct->oppo_app_type >= WHITE_APP_BASE){
			ct_mark = ct->mark & MARK_MASK;
			if(CELLULAR_MARK == ct_mark){
				index = ct->oppo_app_type - WHITE_APP_BASE;
				if(index < WHITE_APP_NUM){
                    white_app_list.cell_bytes[index] += skb->len;
				}
			}
		}
		//reset dev retran num
		reset_dev_info_syn_retran(ct,skb);
	}

	//calc white app cell bytes when sla is not enable
	if(oppo_sla_info[CELLULAR_INDEX].if_up ||
		oppo_sla_info[CELLULAR_INDEX].need_up){

		if(!oppo_sla_info[WLAN_INDEX].if_up || oppo_sla_def_net){
			if(ct->oppo_app_type >= WHITE_APP_BASE){
				index = ct->oppo_app_type - WHITE_APP_BASE;
				if(index < WHITE_APP_NUM){
                    white_app_list.cell_bytes_normal[index] += skb->len;
				}
			}
		}
	}
}

//add for android Q statictis tcp tx and tx
static void statistics_wlan_tcp_tx_rx(const struct nf_hook_state *state,struct sk_buff *skb)
{
	struct iphdr *iph;
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;
	struct net_device *dev = NULL;
	unsigned int hook = state->hook;

	if((oppo_sla_info[WLAN_INDEX].if_up || oppo_sla_info[WLAN_INDEX].need_up) &&
		(iph = ip_hdr(skb)) != NULL && iph->protocol == IPPROTO_TCP){

		ct = nf_ct_get(skb, &ctinfo);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
		if(NULL == ct || (0 == ct->mark)){
#else
		if(NULL == ct || nf_ct_is_untracked(ct) || (0 == ct->mark)){
#endif
			if (NF_INET_LOCAL_OUT == hook) {
				dev = skb_dst(skb)->dev;
			} else if (NF_INET_LOCAL_IN == hook){
				dev = state->in;
			}

			if (dev && !memcmp(dev->name,oppo_sla_info[WLAN_INDEX].dev_name,strlen(dev->name))) {
				if (oppo_sla_debug) {
					printk("hook = %d,dev name = %s\n",hook,dev->name);
				}
				if (NF_INET_LOCAL_OUT == hook) {
					wlan0_tcp_tx++;
				} else if (NF_INET_LOCAL_IN == hook) {
					wlan0_tcp_rx++;
				}
			}
		} else if (WLAN_MARK == (ct->mark & MARK_MASK)) {
			if (oppo_sla_debug) {
				printk("hook = %d,ct mark\n",hook);
			}
			if (NF_INET_LOCAL_OUT == hook) {
				wlan0_tcp_tx++;
			} else if (NF_INET_LOCAL_IN == hook) {
				wlan0_tcp_rx++;
			}
		}
	}
}

static unsigned int oppo_sla_game_rtt_calc(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
	int shift = 3;
	s64 time_now;
	u32 game_rtt = 0;
	struct iphdr *iph = NULL;
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;

	//add for android Q statictis tcp tx and tx
	statistics_wlan_tcp_tx_rx(state,skb);

	if(!oppo_sla_rtt_detect){
		return NF_ACCEPT;
	}

	ct = nf_ct_get(skb, &ctinfo);
	if(NULL == ct){
		return NF_ACCEPT;
	}

	iph = ip_hdr(skb);

	if(oppo_sla_debug){
		if(iph &&
		   IPPROTO_UDP == iph->protocol &&
		   ct->oppo_app_type > 0 && ct->oppo_app_type < GAME_NUM){
		   printk("oppo_sla_game_debug_down:ct_state = %d,skb dev = %s,game_status = %u,game type = %d,"
	              "lost count = %d,interval_time = %u,game mark = %x,protp = %d,src_port = %d,skb len = %d,timeStamp = %llu\n",
	              XT_STATE_BIT(ctinfo),skb->dev->name,ct->oppo_game_detect_status,ct->oppo_app_type,ct->oppo_game_lost_count,
	              ct->oppo_game_time_interval,game_uid[ct->oppo_app_type].mark,iph->protocol,ntohs(udp_hdr(skb)->dest),skb->len,ct->oppo_game_timestamp);
		}
	}

	//calc game or white list app cell bytes
	record_sla_app_cell_bytes(ct,skb);

	//calc game app udp packet count(except for voice packets) and tcp bytes
	if (ct->oppo_app_type > 0 && ct->oppo_app_type < GAME_NUM) {
	        if (iph && IPPROTO_UDP == iph->protocol) {
         	        if (ct->oppo_game_down_count < MAX_DETECT_PKTS &&
         	                ct->oppo_game_detect_status == GAME_SKB_DETECTING) {
         	                ct->oppo_game_down_count++;
        	        }
        	        if (!is_game_voice_packet(ct, skb)) {
         	                game_online_info.udp_rx_pkt_count++;
        	        }
	        } else if (iph && IPPROTO_TCP == iph->protocol) {
	                game_online_info.tcp_rx_byte_count += skb->len;
	        }
	}

	if (is_game_rtt_skb(ct, skb, false)) {
		time_now = ktime_get_ns() / 1000000;
		if(ct->oppo_game_timestamp && time_now > ct->oppo_game_timestamp){

			game_rtt = (u32)(time_now - ct->oppo_game_timestamp);
			if (game_rtt < MIN_GAME_RTT) {
				if(oppo_sla_debug){
					printk("oppo_sla_game_rtt:invalid RTT %dms\n", game_rtt);
				}
				ct->oppo_game_timestamp = 0;
			    return NF_ACCEPT;
			}

			ct->oppo_game_timestamp = 0;

			if(game_rtt > MAX_GAME_RTT){
				game_rtt = MAX_GAME_RTT;
			}

			if(!enable_to_user &&
			   !oppo_sla_enable &&
			   sla_switch_enable &&
			   cell_quality_good){

				enable_to_user = 1;

				if(oppo_sla_info[WLAN_INDEX].need_up){
					oppo_sla_info[WLAN_INDEX].if_up = 1;
				}

				do_gettimeofday(&last_enable_to_user_tv);
				if(oppo_sla_info[CELLULAR_INDEX].need_up){
					oppo_sla_info[CELLULAR_INDEX].if_up = 1;
					oppo_sla_info[CELLULAR_INDEX].need_up = false;
				}
				printk("oppo_sla_netlink: game app send enable sla to user\n");
				oppo_sla_send_to_user(SLA_ENABLE,NULL,0);
			}
			if(game_rtt_show_toast) {
			        oppo_sla_send_to_user(SLA_NOTIFY_GAME_RTT,(char *)&game_rtt,sizeof(game_rtt));
			}
			ct->oppo_game_lost_count = 0;

			sla_game_write_lock();
			game_rtt_estimator(ct->oppo_app_type,game_rtt);
			sla_game_write_unlock();

			if(oppo_sla_debug){
				if(GAME_WZRY == ct->oppo_app_type) {
					shift = 2;
				}
				printk("oppo_sla_game_rtt: game_rtt=%u, srtt=%u\n", game_rtt, game_uid[ct->oppo_app_type].rtt >> shift);
			}
		}
	}

	return NF_ACCEPT;
}

static bool is_skb_pre_bound(struct sk_buff *skb)
{
	u32 pre_mark = skb->mark & 0x10000;

	if(0x10000 == pre_mark){
		return true;
	}

	return false;
}

static bool is_oppo_sla_white_or_game_skb(struct nf_conn *ct,struct sk_buff *skb)
{

	if(ct->oppo_app_type > 0){//game app skb
		return true;
	}

	return false;
}


static int sla_skb_reroute(struct sk_buff *skb,struct nf_conn *ct,const struct nf_hook_state *state)
{
	int err;

	err = ip_route_me_harder(state->net, skb, RTN_UNSPEC);
	if (err < 0){
		return NF_DROP_ERR(err);
	}

	return NF_ACCEPT;
}

static int dns_skb_need_sla(struct sk_buff *skb)
{
	int ret = SLA_SKB_CONTINUE;
	struct iphdr *iph = NULL;

	iph = ip_hdr(skb);
	if(NULL != iph &&
	   (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP) &&
		53 == ntohs(udp_hdr(skb)->dest)){

		ret = SLA_SKB_ACCEPT;

		if(cell_quality_good &&
		   (WEIGHT_STATE_USELESS == oppo_sla_info[WLAN_INDEX].weight_state ||
		   (oppo_sla_info[CELLULAR_INDEX].max_speed >= 100 &&
		    oppo_sla_info[WLAN_INDEX].wlan_score_bad_count >= WLAN_SCORE_BAD_NUM))){
			skb->mark = CELLULAR_MARK;
			ret = SLA_SKB_MARKED;
		}
	}
	return ret;
}

static void detect_white_list_app_skb(struct sk_buff *skb)
{
	int i = 0;
	int index = -1;
	kuid_t uid;
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;
	struct sock *sk = NULL;
	const struct file *filp = NULL;

	ct = nf_ct_get(skb, &ctinfo);

	if(NULL == ct){
		return;
	}

	if(INIT_APP_TYPE == ct->oppo_app_type){
		sk = skb_to_full_sk(skb);
		if(NULL == sk || NULL == sk->sk_socket){
			return;
		}

		filp = sk->sk_socket->file;
		if(NULL == filp){
			return;
		}

		for(i = 0;i < white_app_list.count;i++){
			if(white_app_list.uid[i]){
				uid = make_kuid(&init_user_ns,white_app_list.uid[i]);
				if(uid_eq(filp->f_cred->fsuid, uid)){
					ct->oppo_app_type = i + WHITE_APP_BASE;
					return;
				}
			}
		}
		ct->oppo_app_type = UNKNOW_APP_TYPE;
	}
	else if(ct->oppo_app_type >= WHITE_APP_BASE){
		//calc white app cell bytes when sla is not enable
		if(oppo_sla_info[CELLULAR_INDEX].if_up ||
			oppo_sla_info[CELLULAR_INDEX].need_up){
			if(!oppo_sla_info[WLAN_INDEX].if_up ||
				oppo_sla_def_net){
				index = ct->oppo_app_type - WHITE_APP_BASE;
				if(index < WHITE_APP_NUM){
                    white_app_list.cell_bytes_normal[index] += skb->len;
				}
			}
		}
	}
	return;
}

static int sla_mark_skb(struct sk_buff *skb, const struct nf_hook_state *state)
{
	int index = 0;
	u32 ct_mark = 0x0;
	int retran_mark = 0;
	struct sock *sk = NULL;
	struct nf_conn *ct = NULL;
	int ret = SLA_SKB_CONTINUE;
	enum ip_conntrack_info ctinfo;

	//if wlan assistant has change network to cell,do not mark SKB
	if(oppo_sla_def_net){
		return NF_ACCEPT;
	}

	ct = nf_ct_get(skb, &ctinfo);

	if(NULL == ct){
		return NF_ACCEPT;
	}

	/*
	  * when the wifi is poor,the dns request allways can not rcv respones,
	  * so please let the dns packet with the cell network mark.
	  */
	ret = dns_skb_need_sla(skb);
	if(SLA_SKB_MARKED == ret){
		goto sla_reroute;
	}
	else if(SLA_SKB_ACCEPT == ret){
		return NF_ACCEPT;
	}

	if(!is_oppo_sla_white_or_game_skb(ct,skb)){
		return NF_ACCEPT;
	}

	if(is_skb_pre_bound(skb)){
		return NF_ACCEPT;
	}

	ret = mark_game_app_skb(ct,skb,ctinfo);
	if(SLA_SKB_MARKED == ret){
		goto sla_reroute;
	}
	else if(SLA_SKB_ACCEPT == ret){
		return NF_ACCEPT;
	}
	else if(SLA_SKB_DROP ==	ret){
		return NF_DROP;
	}

	if(ctinfo == IP_CT_NEW){

		sk = skb_to_full_sk(skb);
		if(NULL != sk){
			ret = syn_retransmits_packet_do_specail(sk,ct,skb);
			if(SLA_SKB_MARKED == ret){
				goto sla_reroute;
			}
			else if(SLA_SKB_REMARK == ret){
				return NF_DROP;
			}
			else if(SLA_SKB_ACCEPT == ret){
				return NF_ACCEPT;
			}

			retran_mark = mark_retransmits_syn_skb(sk,ct);
			if(retran_mark){
				if(oppo_sla_debug){
					printk("oppo_sla_retran:retran mark = %x\n",retran_mark & MARK_MASK);
				}
				skb->mark = retran_mark;
			}
			else{

				sla_read_lock();
				if(oppo_sla_info[WLAN_INDEX].weight == 0 &&
					oppo_sla_info[CELLULAR_INDEX].weight == 0){
					skb->mark = set_skb_mark_by_default();
				}
				else{
					skb->mark = get_skb_mark_by_weight();
				}
				sla_read_unlock();

			}

			ct->mark = skb->mark;
			sk->oppo_sla_mark = skb->mark;
		}
	}
	else if((XT_STATE_BIT(ctinfo) &	XT_STATE_BIT(IP_CT_ESTABLISHED)) ||
			(XT_STATE_BIT(ctinfo) &	XT_STATE_BIT(IP_CT_RELATED))){

		//reset_dev_info_syn_retran(ct,skb);
		skb->mark = ct->mark & MARK_MASK;
	}

	//calc white list app cell bytes
	if(ct->oppo_app_type >= WHITE_APP_BASE){
		ct_mark = ct->mark & MARK_MASK;
		if(CELLULAR_MARK == ct_mark){
			index = ct->oppo_app_type - WHITE_APP_BASE;
			if(index < WHITE_APP_NUM){
				white_app_list.cell_bytes[index] += skb->len;
			}
		}
	}

sla_reroute:
	ret = sla_skb_reroute(skb,ct,state);
	return ret;

}

/* oppo sla hook function, mark skb and rerout skb
*/
static unsigned int oppo_sla(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state)
{
	int ret = NF_ACCEPT;
	int game_ret = NF_ACCEPT;

	game_ret = detect_game_up_skb(skb);
	if(SLA_SKB_ACCEPT == game_ret){
		ret = NF_ACCEPT;
		goto end_sla_output;
	}

	//we need to calc white list app cell bytes when sla not enabled
	detect_white_list_app_skb(skb);

    if(oppo_sla_enable){

		ret = sla_mark_skb(skb,state);
    }
	else{

		if(!sla_screen_on){
			ret = NF_ACCEPT;
			goto end_sla_output;
		}

		if(oppo_sla_info[WLAN_INDEX].if_up){

			ret = get_wlan_syn_retran(skb);

			if(oppo_sla_calc_speed &&
			   !oppo_speed_info[WLAN_INDEX].speed_done){
				ret = wlan_get_speed_prepare(skb);
			}
		}
	}
end_sla_output:
	//add for android Q statictis tcp tx and tx
	statistics_wlan_tcp_tx_rx(state,skb);
	return ret;
}

/*
  * so how can we calc speed when app connect server with 443 dport(https)
  */
static void get_content_lenght(struct nf_conn *ct,struct sk_buff *skb,int header_len,int index){

	char *p = (char *)skb->data + header_len;

	char *start = NULL;
	char *end = NULL;
	int temp_len = 0;
	u64 tmp_time;
	u32 content_len = 0;
	char data_buf[256];
	char data_len[11];
	memset(data_len,0x0,sizeof(data_len));
	memset(data_buf,0x0,sizeof(data_buf));

	if(ct->oppo_http_flag != 1 || ct->oppo_skb_count > 3){
		return;
	}
	ct->oppo_skb_count++;

	temp_len = (char *)skb_tail_pointer(skb) - p;
	if(temp_len < 25){//HTTP/1.1 200 OK + Content-Length
		return;
	}

	p += 25;

	temp_len = (char *)skb_tail_pointer(skb) - p;
	if(temp_len){
		if(temp_len > (sizeof(data_buf)-1)){

			temp_len = (sizeof(data_buf)-1);
		}
		memcpy(data_buf,p,temp_len);
		start = strstr(data_buf,"Content-Length");
		if(start != NULL){
			ct->oppo_http_flag = 2;
			start += 16; //add Content-Length:

			end = strchr(start,0x0d);//get '\r\n'

			if(NULL != end){
				if((end - start) < 11){
					memcpy(data_len,start,end - start);
					sscanf(data_len,"%u",&content_len);
					//printk("oppo_sla_speed:content = %u\n",content_len);
				}
				else{
					content_len = 0x7FFFFFFF;
				}

				tmp_time = ktime_get_ns();
				oppo_speed_info[index].sum_bytes += content_len;
				if(0 == oppo_speed_info[index].bytes_time){
					oppo_speed_info[index].bytes_time = tmp_time;
				}
				else{
					if(oppo_speed_info[index].sum_bytes >= 20000 ||
						(tmp_time - oppo_speed_info[index].bytes_time) > 5000000000){
						oppo_speed_info[index].bytes_time = tmp_time;
						if(oppo_speed_info[index].sum_bytes >= 20000){
							oppo_speed_info[index].ms_speed_flag = 1;
						}
						oppo_speed_info[index].sum_bytes = 0;
					}
				}
			}
		}
	}
}

static unsigned int oppo_sla_speed_calc(void *priv,
				      struct sk_buff *skb,
				      const struct nf_hook_state *state){

	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = NULL;
	int index = -1;
	int tmp_speed = 0;
	u64 time_now = 0;
	u64 tmp_time = 0;
	struct iphdr *iph;
	struct tcphdr *tcph;
	int datalen = 0;
	int header_len = 0;

	if(!sla_switch_enable){
		return NF_ACCEPT;
	}

	//only calc wlan speed
	if(oppo_sla_info[WLAN_INDEX].if_up &&
	   !oppo_sla_info[CELLULAR_INDEX].if_up){
		index = WLAN_INDEX;
	}
	else{
		return NF_ACCEPT;
	}

	if(!enable_to_user &&
		oppo_sla_calc_speed &&
		!oppo_speed_info[index].speed_done){

		ct = nf_ct_get(skb, &ctinfo);

		if(NULL == ct){
			return NF_ACCEPT;

		}

		if(XT_STATE_BIT(ctinfo) & XT_STATE_BIT(IP_CT_ESTABLISHED)){
			if((iph = ip_hdr(skb)) != NULL &&
					iph->protocol == IPPROTO_TCP){

				tcph = tcp_hdr(skb);
				datalen = ntohs(iph->tot_len);
				header_len = iph->ihl * 4 + tcph->doff * 4;

				if((datalen - header_len) >= 64){//ip->len > tcphdrlen

					if(!oppo_speed_info[index].ms_speed_flag){
						get_content_lenght(ct,skb,header_len,index);
					}

					if(oppo_speed_info[index].ms_speed_flag){

						time_now = ktime_get_ns();
						if(0 == oppo_speed_info[index].last_time &&
							0 == oppo_speed_info[index].first_time){
							oppo_speed_info[index].last_time = time_now;
							oppo_speed_info[index].first_time = time_now;
							oppo_speed_info[index].rx_bytes = skb->len;
							return NF_ACCEPT;
						}

						tmp_time = time_now - oppo_speed_info[index].first_time;
						oppo_speed_info[index].rx_bytes += skb->len;
						oppo_speed_info[index].last_time = time_now;
						if(tmp_time > 500000000){
							oppo_speed_info[index].ms_speed_flag = 0;
							tmp_time = oppo_speed_info[index].last_time - oppo_speed_info[index].first_time;
							tmp_speed = (1000 *1000*oppo_speed_info[index].rx_bytes) / (tmp_time);//kB/s

							oppo_speed_info[index].speed_done = 1;
							do_gettimeofday(&last_calc_small_speed_tv);

							if(cell_quality_good &&
								!rate_limit_info.rate_limit_enable &&
								tmp_speed > CALC_WEIGHT_MIN_SPEED_1 &&
								tmp_speed < CALC_WEIGHT_MIN_SPEED_3 &&
								oppo_sla_info[index].max_speed < 100){

								enable_to_user = 1;

								if(oppo_sla_info[WLAN_INDEX].need_up){
									oppo_sla_info[WLAN_INDEX].if_up = 1;
								}

								do_gettimeofday(&last_enable_to_user_tv);
								if(oppo_sla_info[CELLULAR_INDEX].need_up){
									oppo_sla_info[CELLULAR_INDEX].if_up = 1;
									oppo_sla_info[CELLULAR_INDEX].need_up = false;
								}
								oppo_sla_send_to_user(SLA_ENABLE,NULL,0);
								printk("oppo_sla_netlink: calc speed is small,send enable sla to user,cellular need up = %d\n",
										oppo_sla_info[CELLULAR_INDEX].need_up);
							}

							printk("oppo_sla_speed: speed[%d] = %d\n",index,tmp_speed);
						}
					}
				}
			}
		}
	}

	return NF_ACCEPT;
}

static void oppo_statistic_dev_rtt(struct sock *sk,long rtt)
{
	int index = -1;
	int tmp_rtt = rtt / 1000; //us -> ms
	u32 mark = sk->oppo_sla_mark & MARK_MASK;

	if(oppo_sla_def_net){
		index  = CELLULAR_INDEX;
	}else if(!oppo_sla_enable) {
		if (oppo_sla_info[WLAN_INDEX].if_up){
		    index = WLAN_INDEX;
		} else if (oppo_sla_info[CELLULAR_INDEX].if_up || oppo_sla_info[CELLULAR_INDEX].need_up){
		    index = CELLULAR_INDEX;
		}
	} else {
		index = find_dev_index_by_mark(mark);
    }

	if(-1 != index){

		//sk->oppo_sla_mark |= RTT_MARK;

		calc_rtt_by_dev_index(index,tmp_rtt,sk);

	}
}

/*sometimes when skb reject by iptables,
*it will retran syn which may make the rtt much big
*so just mark the stream(ct) with mark RTT_MARK when this happens
*/
static void sla_mark_streams_for_iptables_reject(struct sk_buff *skb,enum ipt_reject_with reject_type)
{
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);
	if(NULL == ct){
		return;
	}
	ct->mark |= RTT_MARK;

	if (oppo_sla_debug) {
		if(ctinfo == IP_CT_NEW){
			struct sock *sk = skb_to_full_sk(skb);
			const struct file *filp = NULL;
			if(sk && sk_fullsock(sk)){
				if(NULL == sk->sk_socket){
					return;
				}

				filp = sk->sk_socket->file;
				if(NULL == filp){
					return;
				}
				printk("oppo_sla_iptables:uid = %u,reject type = %u\n",filp->f_cred->fsuid.val,reject_type);
			}
		}
	}

	return;
}

static void is_need_calc_wlan_small_speed(int speed)
{
	if(sla_screen_on &&
		!enable_to_user &&
		!oppo_sla_enable &&
		!oppo_sla_calc_speed &&
		!rate_limit_info.rate_limit_enable){

		if(speed <= 100 &&
			speed > CALC_WEIGHT_MIN_SPEED_1 &&
			oppo_sla_info[WLAN_INDEX].minute_speed > MINUTE_LITTE_RATE){

			oppo_sla_calc_speed = 1;
			memset(&oppo_speed_info[WLAN_INDEX],0x0,sizeof(struct oppo_speed_calc));
		}
	}
}

static void auto_disable_sla_for_good_wlan(void)
{
	int time_interval;
	struct timeval tv;

	do_gettimeofday(&tv);
	time_interval = tv.tv_sec - last_enable_cellular_tv.tv_sec;

	if(enable_to_user &&
	   oppo_sla_enable &&
	   time_interval >= 300 &&
	   !game_online_info.game_online &&
	   oppo_sla_info[WLAN_INDEX].sla_avg_rtt < 150 &&
	   oppo_sla_info[WLAN_INDEX].max_speed >= 300){

	   enable_to_user = 0;
	   disable_cellular_tv = tv;

	   sla_write_lock();
	   oppo_sla_info[WLAN_INDEX].weight = 100;
	   oppo_sla_info[CELLULAR_INDEX].weight = 0;
	   oppo_sla_info[CELLULAR_INDEX].need_disable = true;
	   sla_write_unlock();

	   printk("oppo_sla_netlink: need to disable cell\n");
	}
}

static void detect_wlan_state_with_speed(void)
{
	auto_disable_sla_for_good_wlan();
	is_need_calc_wlan_small_speed(oppo_sla_info[WLAN_INDEX].max_speed);
}


static inline int dev_isalive(const struct net_device *dev)
{
	return dev->reg_state <= NETREG_REGISTERED;
}

static void statistic_dev_speed(struct timeval tv,int time_interval)
{
	int i=0;
	int temp_speed;
	u64	temp_bytes;
	u64 total_bytes = 0;
	int tmp_minute_time;
	int tmp_minute_speed;
	int do_calc_minute_speed = 0;
	struct net_device *dev;
	const struct rtnl_link_stats64 *stats;
	struct rtnl_link_stats64 temp;

	tmp_minute_time = tv.tv_sec - last_minute_speed_tv.tv_sec;
	if(tmp_minute_time >= LITTLE_FLOW_TIME){
		last_minute_speed_tv = tv;
		do_calc_minute_speed = 1;
	}

	for(i = 0; i < IFACE_NUM; i++){
		if(oppo_sla_info[i].if_up || oppo_sla_info[i].need_up){
			dev = dev_get_by_name(&init_net, oppo_sla_info[i].dev_name);
			if(dev) {
				if (dev_isalive(dev) && (stats = dev_get_stats(dev, &temp))){
					//first time have no value,and  maybe oppo_sla_info[i].rx_bytes will more than stats->rx_bytes
					total_bytes = stats->rx_bytes + stats->tx_bytes;
					if(0 == oppo_sla_info[i].total_bytes ||
						oppo_sla_info[i].total_bytes > total_bytes){

						oppo_sla_info[i].total_bytes = total_bytes;
						oppo_sla_info[i].minute_rx_bytes = total_bytes;

					}
					else{

						if(do_calc_minute_speed){

							if(WLAN_INDEX == i){
								detect_wlan_state_with_speed();
							}

							temp_bytes = total_bytes - oppo_sla_info[i].minute_rx_bytes;
							oppo_sla_info[i].minute_rx_bytes = total_bytes;
							tmp_minute_speed = (8 * temp_bytes) / tmp_minute_time; //kbit/s
							oppo_sla_info[i].minute_speed = tmp_minute_speed / 1000;

						}

						temp_bytes = total_bytes - oppo_sla_info[i].total_bytes;
						oppo_sla_info[i].total_bytes = total_bytes;

						temp_speed = temp_bytes / time_interval;
						temp_speed = temp_speed / 1000;//kB/s
						oppo_sla_info[i].current_speed = temp_speed;

						if(temp_speed > DOWNLOAD_SPEED &&
							temp_speed > (oppo_sla_info[i].max_speed / 2)){

							if(oppo_sla_info[i].download_flag < (2 * DOWN_LOAD_FLAG)){
								oppo_sla_info[i].download_flag++;
							}
						}
						else if(temp_speed < (oppo_sla_info[i].max_speed / 3)){
							if(oppo_sla_info[i].download_flag >= 2){
								oppo_sla_info[i].download_flag -= 2;
							}
							else if(oppo_sla_info[i].download_flag){
								oppo_sla_info[i].download_flag--;
							}
						}

						if(temp_speed > oppo_sla_info[i].max_speed){
							oppo_sla_info[i].max_speed = temp_speed;
						}

						if(CELLULAR_INDEX == i &&
							oppo_sla_info[i].max_speed > rom_update_info.cell_speed){

							oppo_sla_info[i].max_speed = rom_update_info.cell_speed;
						}

						if(WLAN_INDEX == i){
							if(oppo_sla_info[i].wlan_score <= rom_update_info.wlan_bad_score &&
							   oppo_sla_info[i].max_speed > rom_update_info.wlan_little_score_speed){
								oppo_sla_info[i].max_speed = rom_update_info.wlan_little_score_speed;
							}
							else if(oppo_sla_info[i].max_speed > rom_update_info.wlan_speed){
								oppo_sla_info[i].max_speed = rom_update_info.wlan_speed;
							}
						}

						//for lte current_speed may bigger than max_speed
						if(oppo_sla_info[i].current_speed > oppo_sla_info[i].max_speed ){
							oppo_sla_info[i].left_speed = oppo_sla_info[i].current_speed - oppo_sla_info[i].max_speed;
						}
						else{
							oppo_sla_info[i].left_speed = oppo_sla_info[i].max_speed - oppo_sla_info[i].current_speed;
						}

						if(temp_speed > 400) {
							if (oppo_sla_info[i].avg_rtt > 150) {
							    oppo_sla_info[i].avg_rtt -= 50;
							}
							if (oppo_sla_info[i].sla_avg_rtt > 150) {
								oppo_sla_info[i].sla_avg_rtt -=50;
							}
							oppo_sla_info[i].congestion_flag = CONGESTION_LEVEL_NORMAL;
						}

					}
				}
				dev_put(dev);
			}

			if(oppo_sla_debug /*&& net_ratelimit()*/){
				printk("oppo_sla: dev_name = %s,if_up = %d,max_speed = %d,current_speed = %d,avg_rtt = %d,congestion = %d,"
						"is_download = %d,syn_retran = %d,minute_speed = %d,weight_state = %d\n",
	    				oppo_sla_info[i].dev_name,oppo_sla_info[i].if_up,
	     				oppo_sla_info[i].max_speed,oppo_sla_info[i].current_speed,
	     				oppo_sla_info[i].avg_rtt,oppo_sla_info[i].congestion_flag,oppo_sla_info[i].download_flag,
						oppo_sla_info[i].syn_retran,oppo_sla_info[i].minute_speed,oppo_sla_info[i].weight_state);
			}

		}
	}
}

static void reset_dev_info_by_retran(struct oppo_dev_info *node)
{
	struct timeval tv;

	//to avoid when weight_state change WEIGHT_STATE_USELESS now ,
	//but the next moment change to WEIGHT_STATE_RECOVERY
	//because of minute_speed is little than MINUTE_LITTE_RATE;
	do_gettimeofday(&tv);
	last_minute_speed_tv = tv;
	node->minute_speed = MINUTE_LITTE_RATE;

	sla_rtt_write_lock();
	node->rtt_index = 0;
	node->sum_rtt= 0;
	node->avg_rtt = 0;
	node->sla_avg_rtt = 0;
	node->sla_rtt_num = 0;
	node->sla_sum_rtt = 0;
	sla_rtt_write_unlock();

	node->max_speed = 0;
	node->left_speed = 0;
	node->current_speed = 0;
	node->syn_retran = 0;
	node->weight_state = WEIGHT_STATE_USELESS;

	sla_write_lock();
	node->weight = 0;
	sla_write_unlock();

	if(oppo_sla_debug){
		printk("oppo_sla: reset_dev_info_by_retran,dev_name = %s\n",node->dev_name);
	}
}

static void detect_network_is_available(void)
{
	int i;
	if(!oppo_sla_enable){
		return;
	}

	for(i = 0; i < IFACE_NUM; i++){

		if(oppo_sla_info[i].syn_retran >= MAX_SYN_RETRANS){

			reset_dev_info_by_retran(&oppo_sla_info[i]);
			oppo_sla_info[i].congestion_flag = CONGESTION_LEVEL_HIGH;

		}
	}

	return;
}

static void change_weight_state(void)
{
	int i;

	if(!oppo_sla_enable){
		return;
	}

	for(i = 0; i < IFACE_NUM; i++){

		if(WEIGHT_STATE_NORMAL != oppo_sla_info[i].weight_state){

			if(oppo_sla_info[i].max_speed >= CALC_WEIGHT_MIN_SPEED_2){

				oppo_sla_info[i].weight_state = WEIGHT_STATE_NORMAL;
				printk("oppo_sla_reset_weight:WEIGHT_STATE_NORMAL\n");

			}
			else if((oppo_sla_info[i].syn_retran ||
				oppo_sla_info[i].sla_avg_rtt > NORMAL_RTT) &&
				WEIGHT_STATE_RECOVERY == oppo_sla_info[i].weight_state){
				reset_dev_info_by_retran(&oppo_sla_info[i]);
			}
		}
	}

}

static int calc_weight_with_speed(int wlan_speed,int cellular_speed)
{
	int tmp_weight;
	int sum_speed = wlan_speed + cellular_speed;

	tmp_weight = (100 * wlan_speed) / sum_speed;

	return tmp_weight;
}

static int calc_weight_with_left_speed(void)
{

	int wlan_speed;
	int cellular_speed;

	if(oppo_sla_info[WLAN_INDEX].download_flag >= DOWN_LOAD_FLAG &&
		(oppo_sla_info[WLAN_INDEX].congestion_flag > CONGESTION_LEVEL_NORMAL ||
			oppo_sla_info[CELLULAR_INDEX].congestion_flag > CONGESTION_LEVEL_NORMAL) &&
			(oppo_sla_info[WLAN_INDEX].max_speed > CALC_WEIGHT_MIN_SPEED_2 &&
			oppo_sla_info[CELLULAR_INDEX].max_speed > CALC_WEIGHT_MIN_SPEED_2)){

		wlan_speed = oppo_sla_info[WLAN_INDEX].left_speed;
	    cellular_speed = oppo_sla_info[CELLULAR_INDEX].left_speed;
		oppo_sla_info[WLAN_INDEX].weight = calc_weight_with_speed(wlan_speed,cellular_speed);
		oppo_sla_info[CELLULAR_INDEX].weight = 100;

		return 1;

	}

	return 0;
}

static bool is_wlan_speed_good(void)
{
	bool ret = false;

	if(oppo_sla_info[WLAN_INDEX].max_speed >= 300 &&
	   oppo_sla_info[WLAN_INDEX].sla_avg_rtt < 150 &&
	   oppo_sla_info[WLAN_INDEX].wlan_score >= 60){
		ret = true;
		oppo_sla_info[WLAN_INDEX].weight = 100;
		oppo_sla_info[CELLULAR_INDEX].weight = 0;
	}
	return ret;
}

static void recalc_dev_weight(void)
{
	int wlan_speed = oppo_sla_info[WLAN_INDEX].max_speed;
	int cellular_speed = oppo_sla_info[CELLULAR_INDEX].max_speed;

	if(!oppo_sla_enable){
		return;
	}

	sla_write_lock();

	if(oppo_sla_info[WLAN_INDEX].if_up &&
		oppo_sla_info[CELLULAR_INDEX].if_up){

		if(is_wlan_speed_good() ||
			calc_weight_with_left_speed()){
			goto calc_weight_finish;
		}

		if((wlan_speed >= CALC_WEIGHT_MIN_SPEED_2 &&
			cellular_speed >= CALC_WEIGHT_MIN_SPEED_2) ||
			(wlan_speed >= CALC_WEIGHT_MIN_SPEED_1 &&
			 cellular_speed >= (3*wlan_speed))){
			/*speed must bigger than 50KB*/
			oppo_sla_info[WLAN_INDEX].weight = calc_weight_with_speed(wlan_speed,cellular_speed);

		}
		else if((CONGESTION_LEVEL_MIDDLE == oppo_sla_info[WLAN_INDEX].congestion_flag ||
				CONGESTION_LEVEL_MIDDLE == oppo_sla_info[CELLULAR_INDEX].congestion_flag) &&
				(wlan_speed >= CALC_WEIGHT_MIN_SPEED_1 && cellular_speed >= CALC_WEIGHT_MIN_SPEED_1)){

				oppo_sla_info[WLAN_INDEX].weight = calc_weight_with_speed(wlan_speed,cellular_speed);

		}
		else if(CONGESTION_LEVEL_HIGH == oppo_sla_info[WLAN_INDEX].congestion_flag||
				CONGESTION_LEVEL_HIGH == oppo_sla_info[CELLULAR_INDEX].congestion_flag){

			oppo_sla_info[WLAN_INDEX].weight = calc_weight_with_speed(wlan_speed,cellular_speed);

		}
		else{
			//oppo_sla_info[WLAN_INDEX].weight = 0;
			//oppo_sla_info[CELLULAR_INDEX].weight = 0;
			oppo_sla_info[WLAN_INDEX].weight = 30;
			oppo_sla_info[CELLULAR_INDEX].weight = 100;
			goto calc_weight_finish;
		}
	}
	else if(oppo_sla_info[WLAN_INDEX].if_up){
		oppo_sla_info[WLAN_INDEX].weight = 100;
	}

	if(oppo_sla_info[WLAN_INDEX].max_speed < CALC_WEIGHT_MIN_SPEED_1 &&
		CONGESTION_LEVEL_HIGH == oppo_sla_info[WLAN_INDEX].congestion_flag){
		oppo_sla_info[WLAN_INDEX].weight = 0;
	}
	oppo_sla_info[CELLULAR_INDEX].weight = 100;

calc_weight_finish:

	if(oppo_sla_info[WLAN_INDEX].max_speed <= 50 &&
	   oppo_sla_info[CELLULAR_INDEX].max_speed >= 200 &&
	   oppo_sla_info[WLAN_INDEX].wlan_score_bad_count >= WLAN_SCORE_BAD_NUM){

		oppo_sla_info[WLAN_INDEX].weight = 0;
		oppo_sla_info[CELLULAR_INDEX].weight = 100;
	}
	sla_write_unlock();
	if(oppo_sla_debug){
		printk("oppo_sla_weight: wifi weight = %d,cellular weight = %d\n",
				oppo_sla_info[WLAN_INDEX].weight,oppo_sla_info[CELLULAR_INDEX].weight);
	}

	return;
}

static void change_weight_state_for_small_minute_speed(void)
{
	int index_1 = -1;
	int index_2 = -1;
	int sum_speed = MINUTE_LITTE_RATE;

	if(!oppo_sla_enable){
		return;
	}

	//only when two iface are up that need WEIGHT_STATE_RECOVERY state
	if(oppo_sla_info[WLAN_INDEX].if_up &&
		oppo_sla_info[CELLULAR_INDEX].if_up){

		sum_speed = oppo_sla_info[WLAN_INDEX].minute_speed + oppo_sla_info[CELLULAR_INDEX].minute_speed;

		if(WEIGHT_STATE_USELESS == oppo_sla_info[WLAN_INDEX].weight_state){
			index_1 = WLAN_INDEX;
		}

		if(WEIGHT_STATE_USELESS == oppo_sla_info[CELLULAR_INDEX].weight_state){
			index_2 = CELLULAR_INDEX;
		}

		if(((-1 != index_1)|| (-1 != index_2))&&(sum_speed < MINUTE_LITTE_RATE)){

			if(-1 != index_1){
				printk("oppo_sla_weight_state:reset_weight_for_small_minute_speed,index = %d\n",index_1);
				oppo_sla_info[index_1].syn_retran= 0;
				oppo_sla_info[index_1].weight_state = WEIGHT_STATE_RECOVERY;
			}

			if(-1 != index_2){
				printk("oppo_sla_weight_state:reset_weight_for_small_minute_speed,index = %d\n",index_2);
				oppo_sla_info[index_2].syn_retran= 0;
				oppo_sla_info[index_2].weight_state = WEIGHT_STATE_RECOVERY;
			}

			oppo_sla_info[WLAN_INDEX].minute_speed = MINUTE_LITTE_RATE;
			oppo_sla_info[CELLULAR_INDEX].minute_speed = MINUTE_LITTE_RATE;

		}
	}
}

static int need_to_recovery_weight(void)
{
	int ret = SLA_WEIGHT_NORMAL;
	int sum_current_speed = 0;
	int index = -1;

	if(!oppo_sla_enable){
		return ret;
	}

	if(WEIGHT_STATE_RECOVERY == oppo_sla_info[WLAN_INDEX].weight_state){
		index = WLAN_INDEX;
	}
	else if(WEIGHT_STATE_RECOVERY == oppo_sla_info[CELLULAR_INDEX].weight_state){
		index = CELLULAR_INDEX;
	}

	if(-1 != index){

		sum_current_speed = oppo_sla_info[WLAN_INDEX].current_speed + oppo_sla_info[CELLULAR_INDEX].current_speed;

		sum_current_speed *= 8;//kbit/s

		if(sum_current_speed > MINUTE_LITTE_RATE){

			sla_write_lock();

			oppo_sla_info[WLAN_INDEX].sla_avg_rtt = 0;
			oppo_sla_info[CELLULAR_INDEX].sla_avg_rtt = 0;
			oppo_sla_info[WLAN_INDEX].weight = 0;
			oppo_sla_info[CELLULAR_INDEX].weight = 0;

			sla_write_unlock();

			ret = SLA_WEIGHT_RECOVERY;
			printk("oppo_sla_weight:need_to_recovery_weight\n");

		}
	}

	return ret;
}

/*when disable cell,we need 10s to let the skb which at data network to pass*/
static void disable_cellular_by_timer(struct timeval tv)
{
	if(oppo_sla_info[CELLULAR_INDEX].need_disable &&
		(tv.tv_sec - disable_cellular_tv.tv_sec) >= 10){

		oppo_sla_info[CELLULAR_INDEX].need_disable = false;
		oppo_sla_send_to_user(SLA_DISABLE,NULL,0);
		printk("oppo_sla_netlink:speed good,disable sla\n");
	}
}

/*for when wlan up,but cell down,this will affect the rtt calc at wlan network(will calc big)*/
static void up_wlan_iface_by_timer(struct timeval tv)
{
	if(oppo_sla_info[WLAN_INDEX].need_up &&
		(tv.tv_sec - calc_wlan_rtt_tv.tv_sec) >= 10){

		oppo_sla_info[WLAN_INDEX].if_up = 1;
		oppo_sla_info[WLAN_INDEX].need_up = false;

		printk("oppo_sla:up wlan iface actually\n");
	}
}

static void enable_to_user_time_out(struct timeval tv)
{
	if(enable_to_user &&
		!oppo_sla_enable &&
		(tv.tv_sec - last_enable_to_user_tv.tv_sec) >= ENABLE_TO_USER_TIMEOUT){
		enable_to_user = 0;
		printk("oppo_sla:enable_to_user timeout\n");
	}
	return;
}

static void send_speed_and_rtt_to_user(void)
{
	int ret = 0;
	if (oppo_sla_info[WLAN_INDEX].if_up || oppo_sla_info[WLAN_INDEX].need_up ||
	    oppo_sla_info[CELLULAR_INDEX].if_up || oppo_sla_info[CELLULAR_INDEX].need_up) {
	    int payload[5];
	    //add for android Q statictis tcp tx and tx
	    u64 tcp_tx_rx[2];
	    char total_payload[36] = {0};

	    payload[0] = oppo_sla_info[WLAN_INDEX].max_speed;
	    payload[1] = oppo_sla_info[WLAN_INDEX].avg_rtt;

	    payload[2] = oppo_sla_info[CELLULAR_INDEX].max_speed;
	    payload[3] = oppo_sla_info[CELLULAR_INDEX].avg_rtt;

	    payload[4] = oppo_sla_enable;

	    tcp_tx_rx[0] = wlan0_tcp_rx;
	    tcp_tx_rx[1] = wlan0_tcp_tx;

	    memcpy(total_payload,payload,sizeof(payload));
	    memcpy(total_payload + sizeof(payload),tcp_tx_rx,sizeof(tcp_tx_rx));

	    ret = oppo_sla_send_to_user(SLA_NOTIFY_SPEED_RTT, (char *) total_payload, sizeof(total_payload));
	    if (oppo_sla_debug) {
	        printk("oppo_sla:send_speed_and_rtt_to_user wlan:max_speed=%d, avg_rtt=%d  "
	               "cell:max_speed=%d, avg_rtt=%d, oppo_sla_enable=%d\n",
	               payload[0], payload[1], payload[2], payload[3], oppo_sla_enable);
	    }
	}
	return;
}

static void calc_network_congestion(struct timeval tv)
{

	int index = 0;
	int avg_rtt = 0;

	sla_rtt_write_lock();
	for(index = 0; index < IFACE_NUM; index++){
		avg_rtt = 0;
		if(oppo_sla_info[index].if_up || oppo_sla_info[index].need_up){

			if(oppo_sla_info[index].rtt_index >= RTT_NUM){

				avg_rtt = oppo_sla_info[index].sum_rtt / oppo_sla_info[index].rtt_index;
				if(oppo_sla_info[index].download_flag >= DOWN_LOAD_FLAG) {
 					avg_rtt = avg_rtt / 2;
				}

				oppo_sla_info[index].sla_rtt_num++;
				oppo_sla_info[index].sla_sum_rtt += avg_rtt;
				oppo_sla_info[index].avg_rtt = (7*oppo_sla_info[index].avg_rtt + avg_rtt) / 8;
				oppo_sla_info[index].sum_rtt = 0;
				oppo_sla_info[index].rtt_index = 0;
			}

			avg_rtt = oppo_sla_info[index].sla_avg_rtt;

			if(oppo_sla_debug){
				printk("oppo_sla_rtt: index = %d,wlan_rtt = %d,"
					   "wlan score bad = %u,cell_good = %d,need_pop_window = %d,"
					   "sceen_on = %d,enable_to_user = %d,sla_switch_enable = %d,"
					   "oppo_sla_enable= %d,oppo_sla_def_net = %d,sla_avg_rtt = %d,game_cell_to_wifi = %d\n",
					   index,oppo_sla_info[index].avg_rtt,
					   oppo_sla_info[index].wlan_score_bad_count,cell_quality_good,need_pop_window,
					   sla_screen_on,enable_to_user,sla_switch_enable,oppo_sla_enable,oppo_sla_def_net,avg_rtt,game_cell_to_wifi);
			}

			if(sla_screen_on &&
				!sla_switch_enable &&
				need_pop_window &&
				cell_quality_good &&
				!send_pop_win_msg_num &&
				(WLAN_INDEX == index) &&
				oppo_sla_info[WLAN_INDEX].if_up &&
				!rate_limit_info.rate_limit_enable &&
				(oppo_sla_info[index].max_speed <= rom_update_info.sla_speed) &&
				(oppo_sla_info[index].download_flag < DOWN_LOAD_FLAG) &&
				(avg_rtt >= rom_update_info.sla_rtt)){

				send_pop_win_msg_num = 1;
				do_gettimeofday(&last_send_pop_window_msg_tv);
				oppo_sla_send_to_user(SLA_SHOW_DIALOG_NOW,NULL,0);
				if(oppo_sla_debug){
					printk("oppo_sla_window:send sla pop window\n");
				}
			}

			if(sla_screen_on &&
				!enable_to_user &&
				!oppo_sla_enable &&
				sla_switch_enable &&
				cell_quality_good &&
				(WLAN_INDEX == index)&&
				!rate_limit_info.rate_limit_enable &&
				((oppo_sla_info[index].wlan_score && oppo_sla_info[index].wlan_score <= (rom_update_info.wlan_bad_score - 10)) ||
				((oppo_sla_info[index].max_speed <= rom_update_info.sla_speed) &&
				(oppo_sla_info[index].download_flag < DOWN_LOAD_FLAG) &&
				(avg_rtt >= rom_update_info.sla_rtt|| oppo_sla_info[index].wlan_score_bad_count >= WLAN_SCORE_BAD_NUM)))){

				enable_to_user = 1;
				if(oppo_sla_info[WLAN_INDEX].need_up){
					oppo_sla_info[WLAN_INDEX].if_up = 1;
				}

				do_gettimeofday(&last_enable_to_user_tv);
				if(oppo_sla_info[CELLULAR_INDEX].need_up){
					oppo_sla_info[CELLULAR_INDEX].if_up = 1;
					oppo_sla_info[CELLULAR_INDEX].need_up = false;
				}
				oppo_sla_send_to_user(SLA_ENABLE,NULL,0);
				reset_wlan_info();
				printk("oppo_sla_netlink:rtt send SLA_ENABLE to user,cellular need up = %d\n",oppo_sla_info[CELLULAR_INDEX].need_up);
			}
		}
	}
	sla_rtt_write_unlock();

	return;

}

static void init_game_online_info(void)
{
	int i = 0;
	u32 time_now = 0;

	game_cell_to_wifi = false;

	time_now = ktime_get_ns() / 1000000;
	sla_game_write_lock();
	for(i = 1; i < GAME_NUM; i++){
		game_uid[i].mark = WLAN_MARK;
		game_uid[i].switch_time = time_now;
	}
	sla_game_write_unlock();
	if(oppo_sla_debug){
		printk("oppo_sla:init_game_online_info\n");
	}
	memset(&game_online_info,0x0,sizeof(struct oppo_game_online));
}

static void init_game_start_state(void)
{
    int i = 0;
    for(i = 1; i < GAME_NUM; i++){
        if(game_start_state[i]){
            game_start_state[i] = 0;
        }
    }
}

static void game_rx_update(void)
{
        if (game_online_info.game_online) {
                struct game_traffic_info* cur_info;
                int i;
                u32 game_index = 0;
                u32 tcp_rx_large_count = 0;
                u32 delta_udp_pkts = game_online_info.udp_rx_pkt_count;
                u64 delta_tcp_bytes = game_online_info.tcp_rx_byte_count;
                game_online_info.udp_rx_pkt_count = 0;
                game_online_info.tcp_rx_byte_count = 0;

                if (wzry_traffic_info.game_in_front) {
                        cur_info = &wzry_traffic_info;
                        game_index = 1;
                } else if (cjzc_traffic_info.game_in_front) {
                        cur_info = &cjzc_traffic_info;
                        game_index = 2;
                } else {
                        //for debug only...
                        cur_info = &default_traffic_info;
                        cur_info->game_in_front = 0;
                }
                //fill in the latest delta udp rx packets and tcp rx bytes
                cur_info->udp_rx_packet[cur_info->window_index % UDP_RX_WIN_SIZE] = delta_udp_pkts;
                cur_info->tcp_rx_byte[cur_info->window_index % TCP_RX_WIN_SIZE] = delta_tcp_bytes;
                cur_info->window_index++;
                //deal with the control logic
                if (udp_rx_show_toast) {
                        oppo_sla_send_to_user(SLA_NOTIFY_GAME_RX_PKT, (char *)&delta_udp_pkts, sizeof(delta_udp_pkts));
                }

                for (i = 0; i < TCP_RX_WIN_SIZE; i++) {
                        if (cur_info->tcp_rx_byte[i] > TCP_DOWNLOAD_THRESHOLD) {
                                tcp_rx_large_count++;
                        }
                }
                if (tcp_rx_large_count == TCP_RX_WIN_SIZE) {
                        if (oppo_sla_debug) {
                                //tcp downloading.. this should not be in a game.
                                if (oppo_sla_debug) {
                                        printk("oppo_sla_game_rx_update:TCP downloading...should not be in a game.\n");
                                }
                        }
                        if (inGame) {
                                inGame = false;
                                game_cell_to_wifi = false;
                                memset(cur_info->udp_rx_packet, 0x0, sizeof(u32)*UDP_RX_WIN_SIZE);
                                cur_info->window_index = 0;
                        }
                } else {
                        int trafficCount = 0;
                        int lowTrafficCount = 0;
                        for (i = 0; i < UDP_RX_WIN_SIZE; i++) {
                                if (cur_info->udp_rx_packet[i] >= cur_info->udp_rx_min) {
                                        trafficCount++;
                                        if (cur_info->udp_rx_packet[i] <= cur_info->udp_rx_low_thr) {
                                                lowTrafficCount++;
                                        }
                                }
                        }
                        if (trafficCount >= cur_info->in_game_true_thr) {
                                if (!inGame) {
                                        int index;
                                        inGame = true;
                                        game_cell_to_wifi = false;
                                        for (index = 0; index < UDP_RX_WIN_SIZE; index++) {
                                                cur_info->udp_rx_packet[index] = 20;
                                        }
                                        lowTrafficCount = 0;
                                }
                        } else if (trafficCount <= cur_info->in_game_false_thr) {
                                if (inGame) {
                                        inGame = false;
                                        game_cell_to_wifi = false;
                                        memset(cur_info->udp_rx_packet, 0x0, sizeof(u32)*UDP_RX_WIN_SIZE);
                                        cur_info->window_index = 0;
                                }
                        }
                        if (inGame && cur_info->game_in_front) {
                                if (lowTrafficCount >= cur_info->rx_bad_thr &&
                                        game_index > 0 && game_start_state[game_index]) {   //wzry has low traffic when game ends
                                        game_rx_bad = true;
                                } else {
                                        game_rx_bad = false;
                                }
                        }
                        if (oppo_sla_debug) {
                                printk("oppo_sla_game_rx_update:delta_udp_pkts=%d, lowTrafficCount=%d, inGame=%d, "
                                        "game_rx_bad=%d, delta_tcp_bytes=%llu, tcp_rx_large_count=%d, trafficCount=%d\n",
                                        delta_udp_pkts, lowTrafficCount, inGame,
                                        game_rx_bad, delta_tcp_bytes, tcp_rx_large_count, trafficCount);
                        }
                }
        }
}

static void game_online_time_out(struct timeval tv)
{
	if(game_online_info.game_online &&
	   (tv.tv_sec - game_online_info.last_game_skb_tv.tv_sec) >= GAME_SKB_TIME_OUT){
		init_game_start_state();
		init_game_online_info();
	}
}

static void pop_window_msg_time_out(struct timeval tv)
{
	int time_interval = 0;
	int time_out = 30;
	time_interval = tv.tv_sec - last_send_pop_window_msg_tv.tv_sec;

	if(time_interval >= time_out && send_pop_win_msg_num){
		send_pop_win_msg_num = 0;
	}
}

//add for rate limit function to statistics front uid rtt
static void oppo_rate_limit_rtt_calc(struct timeval tv)
{
	int num = 0;
	int sum = 0;
	int time_interval = 0;

	time_interval = tv.tv_sec - rate_limit_info.last_tv.tv_sec;
	if(time_interval >= 10){
		sla_rtt_write_lock();

		if(rate_limit_info.disable_rtt_num >= 10){
			num = rate_limit_info.disable_rtt_num;
		    sum = rate_limit_info.disable_rtt_sum;
			rate_limit_info.disable_rtt = sum / num;
		}

		if(rate_limit_info.enable_rtt_num >= 10){
			num = rate_limit_info.enable_rtt_num;
		    sum = rate_limit_info.enable_rtt_sum;
			rate_limit_info.enable_rtt = sum / num;
		}

		rate_limit_info.disable_rtt_num = 0;
		rate_limit_info.disable_rtt_sum = 0;
		rate_limit_info.enable_rtt_num = 0;
		rate_limit_info.enable_rtt_sum = 0;
		rate_limit_info.last_tv = tv;

		sla_rtt_write_unlock();
	}
}

static void oppo_sla_work_queue_func(struct work_struct *work)
{
	int time_interval;
	int ret;
	struct timeval tv;

	do_gettimeofday(&tv);

	time_interval = tv.tv_sec - last_speed_tv.tv_sec;

	if(time_interval >= CALC_DEV_SPEED_TIME){
		last_speed_tv = tv;
		change_weight_state();
		back_iface_speed();
		statistic_dev_speed(tv,time_interval);
		calc_network_congestion(tv);
		change_weight_state_for_small_minute_speed();
	}

	if(SLA_WEIGHT_RECOVERY != (ret = need_to_recovery_weight())){
		time_interval = tv.tv_sec - last_weight_tv.tv_sec;
		if(time_interval >= RECALC_WEIGHT_TIME){
			last_weight_tv = tv;
			detect_network_is_available();
			recalc_dev_weight();
		}
	}

	enable_to_user_time_out(tv);
	up_wlan_iface_by_timer(tv);
	disable_cellular_by_timer(tv);
	game_online_time_out(tv);
	game_rx_update();
	pop_window_msg_time_out(tv);
	reset_oppo_sla_calc_speed(tv);
	oppo_rate_limit_rtt_calc(tv);
	send_speed_and_rtt_to_user();
}

static void oppo_sla_timer_function(void)
{
	if (workqueue_sla){
		queue_work(workqueue_sla, &oppo_sla_work);
	}
	mod_timer(&sla_timer, jiffies + SLA_TIMER_EXPIRES);

}

/*
	timer to statistic each dev speed,
	start it when sla is enabled
*/
static void oppo_sla_timer_init(void)
{
	init_timer(&sla_timer);
	sla_timer.function = (void*)oppo_sla_timer_function;
	sla_timer.expires = jiffies +  SLA_TIMER_EXPIRES;// timer expires in ~1s
	add_timer (&sla_timer);

	do_gettimeofday(&last_speed_tv);
	do_gettimeofday(&last_weight_tv);
	do_gettimeofday(&last_minute_speed_tv);
	do_gettimeofday(&rate_limit_info.last_tv);
}


/*
	timer to statistic each dev speed,
	stop it when sla is disabled
*/
static void oppo_sla_timer_fini(void)
{
	del_timer(&sla_timer);
}


static struct nf_hook_ops oppo_sla_ops[] __read_mostly = {
	{
		.hook		= oppo_sla,
		.pf		    = NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_OUT,
		//must be here,for  dns packet will do DNAT at mangle table with skb->mark
		.priority	= NF_IP_PRI_CONNTRACK + 1,
	},
	{
		.hook		= oppo_sla_speed_calc,
		.pf		    = NFPROTO_IPV4,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP_PRI_MANGLE + 1,
	},
	{
		.hook		= oppo_sla_game_rtt_calc,
		.pf		    = NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER + 1,
	},

	{ }
};

static struct ctl_table oppo_sla_sysctl_table[] = {
	{
		.procname	= "oppo_sla_enable",
		.data		= &oppo_sla_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "oppo_sla_debug",
		.data		= &oppo_sla_debug,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "oppo_sla_calc_speed",
		.data		= &oppo_sla_calc_speed,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "oppo_sla_rtt_detect",
		.data		= &oppo_sla_rtt_detect,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "game_mark",
		.data		= &game_mark,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "front_uid",
		.data		= &rate_limit_info.front_uid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "rate_limit_enable",
		.data		= &rate_limit_info.rate_limit_enable,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "disable_rtt",
		.data		= &rate_limit_info.disable_rtt,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "enable_rtt",
		.data		= &rate_limit_info.enable_rtt,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "udp_rx_show_toast",
		.data		= &udp_rx_show_toast,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "game_rtt_show_toast",
		.data		= &game_rtt_show_toast,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

static int oppo_sla_sysctl_init(void)
{
	oppo_sla_table_hrd = register_net_sysctl(&init_net, "net/oppo_sla",
		                                          oppo_sla_sysctl_table);
	return oppo_sla_table_hrd == NULL ? -ENOMEM : 0;
}

static void oppo_sla_send_white_list_app_traffic(void)
{
    char *p = NULL;
    char send_msg[1284];

    memset(send_msg,0x0,sizeof(send_msg));

    p = send_msg;
    memcpy(p,&white_app_list.count,sizeof(u32));

    p += sizeof(u32);
    memcpy(p,white_app_list.uid,WHITE_APP_NUM*sizeof(u32));

    p += WHITE_APP_NUM*sizeof(u32);
    memcpy(p,white_app_list.cell_bytes,WHITE_APP_NUM*sizeof(u64));

	p += WHITE_APP_NUM*sizeof(u64);
		memcpy(p,white_app_list.cell_bytes_normal,WHITE_APP_NUM*sizeof(u64));

	oppo_sla_send_to_user(SLA_SEND_WHITE_LIST_APP_TRAFFIC,
		     send_msg,sizeof(send_msg));

	return;
}

static int oppo_sla_set_debug(struct nlmsghdr *nlh)
{
    oppo_sla_debug = *(u32 *)NLMSG_DATA(nlh);
	printk("oppo_sla_netlink:set debug = %d\n", oppo_sla_debug);
	return	0;
}

static int oppo_sla_set_default_network(struct nlmsghdr *nlh)
{
    oppo_sla_def_net = *(u32 *)NLMSG_DATA(nlh);
	printk("oppo_sla_netlink:set default network = %d\n", oppo_sla_def_net);
	return	0;
}

static void oppo_sla_send_game_app_traffic(void)
{
	oppo_sla_send_to_user(SLA_SEND_GAME_APP_STATISTIC,
		     (char *)game_uid,GAME_NUM*sizeof(struct oppo_sla_game_info));

	return;
}

static void oppo_sla_send_syn_retran_info(void)
{
	oppo_sla_send_to_user(SLA_GET_SYN_RETRAN_INFO,
		     (char *)&syn_retran_statistic,sizeof(struct oppo_syn_retran_statistic));

	memset(&syn_retran_statistic,0x0,sizeof(struct oppo_syn_retran_statistic));
	return;
}

static int disable_oppo_sla_module(void)
{
	sla_write_lock();
	if(oppo_sla_enable){
		reset_wlan_info();
		enable_to_user = 0;
		//oppo_sla_debug = 0;
		oppo_sla_enable = 0;
		init_game_online_info();
		oppo_sla_send_to_user(SLA_DISABLED, NULL, 0);
		printk("oppo_sla_netlink: disable\n");
    }
	sla_write_unlock();
	return 0;
}

static int oppo_sla_iface_up(struct nlmsghdr *nlh)
{
	int index = -1;
	u32 mark = 0x0;
	struct oppo_dev_info *node = NULL;

	char *p = (char *)NLMSG_DATA(nlh);
	sla_write_lock();
	if(SLA_WIFI_UP == nlh->nlmsg_type){
		printk("oppo_sla_netlink:wifi up\n");
		mark = WLAN_MARK;
		index = WLAN_INDEX;

		do_gettimeofday(&last_speed_tv);
		do_gettimeofday(&last_weight_tv);
		do_gettimeofday(&last_minute_speed_tv);
		do_gettimeofday(&calc_wlan_rtt_tv);

		oppo_sla_info[WLAN_INDEX].if_up = 0;
		oppo_sla_info[WLAN_INDEX].need_up = true;
		oppo_sla_info[CELLULAR_INDEX].max_speed = 0;
	}
	else if(SLA_CELLULAR_UP == nlh->nlmsg_type){
		printk("oppo_sla_netlink:cellular up\n");

		if(!enable_to_user){
			oppo_sla_info[CELLULAR_INDEX].if_up = 0;
			oppo_sla_info[CELLULAR_INDEX].need_up = true;
		}
		else{
			oppo_sla_info[CELLULAR_INDEX].if_up = 1;
		}
		mark = CELLULAR_MARK;
		index = CELLULAR_INDEX;
	}

	if( -1 != index && p){
		node = &oppo_sla_info[index];
		node->mark = mark;
		node->minute_speed= MINUTE_LITTE_RATE;
		memcpy(node->dev_name,p,IFACE_LEN);
		printk("oppo_sla_netlink:ifname = %s,ifup = %d\n",node->dev_name,node->if_up);
	}
	sla_write_unlock();
	return 0;
}

static int oppo_sla_iface_down(struct nlmsghdr *nlh)
{
	int index = -1;

	if(SLA_WIFI_DOWN == nlh->nlmsg_type){
		printk("oppo_sla_netlink:wifi down\n");
		//add for android Q statictis tcp tx and tx
		wlan0_tcp_tx = 0;
		wlan0_tcp_rx = 0;

		index = WLAN_INDEX;
		enable_to_user = 0;
		oppo_sla_calc_speed = 0;
		oppo_sla_send_game_app_traffic();
		memset(game_uid,0x0,GAME_NUM*sizeof(struct oppo_sla_game_info));
	}
	else if(SLA_CELLULAR_DOWN == nlh->nlmsg_type){
		printk("oppo_sla_netlink:cellular down\n");
		index = CELLULAR_INDEX;
	}

	if( -1 != index){

		sla_write_lock();

		memset(&oppo_speed_info[index],0x0,sizeof(struct oppo_speed_calc));
		memset(&oppo_sla_info[index],0x0,sizeof(struct oppo_dev_info));

		sla_write_unlock();

	}

	return 0;
}

static int oppo_sla_get_pid(struct sk_buff *skb,struct nlmsghdr *nlh)
{
	oppo_sla_pid = NETLINK_CB(skb).portid;
	printk("oppo_sla_netlink:get oppo_sla_pid = %u\n",oppo_sla_pid);

	return 0;
}

static int oppo_sla_get_wlan_score(struct nlmsghdr *nlh)
{
	int *score = (int *)NLMSG_DATA(nlh);

	if(NULL != score){
		//when wlan+ not be enabled
		if(*score <= (rom_update_info.wlan_bad_score - 10)){
		   if(sla_screen_on &&
		  	  !enable_to_user &&
		  	  !oppo_sla_enable &&
		  	  cell_quality_good &&
		  	  sla_switch_enable &&
		  	  !rate_limit_info.rate_limit_enable &&
		  	  !oppo_sla_info[CELLULAR_INDEX].need_up &&
		  	  !oppo_sla_info[CELLULAR_INDEX].if_up){
				printk("oppo_sla_netlink:send enable sla to user by wifi score\n");
		   		enable_to_user = 1;
				reset_wlan_info();

				if(oppo_sla_info[WLAN_INDEX].need_up){
					oppo_sla_info[WLAN_INDEX].if_up = 1;
				}

				do_gettimeofday(&last_enable_to_user_tv);
				oppo_sla_send_to_user(SLA_ENABLE,NULL,0);
		   	}
		}

		oppo_sla_info[WLAN_INDEX].wlan_score = *score;

		if(*score <= (rom_update_info.wlan_bad_score - 10)) {
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count += WLAN_SCORE_BAD_NUM;
		}
		else if(*score <= (rom_update_info.wlan_bad_score - 5)) {
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count += 3;
		}
		else if(*score <= rom_update_info.wlan_bad_score){
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count++;
		}
		else if(*score >= rom_update_info.wlan_good_score){
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count = 0;
		}
		else if(*score >= (rom_update_info.wlan_good_score - 5) &&
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count >= 3){
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count -= 3;
		}
		else if(*score >= (rom_update_info.wlan_good_score - 10) && oppo_sla_info[WLAN_INDEX].wlan_score_bad_count){
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count--;
		}

		if(oppo_sla_info[WLAN_INDEX].wlan_score_bad_count > (2*WLAN_SCORE_BAD_NUM)){
			oppo_sla_info[WLAN_INDEX].wlan_score_bad_count = 2*WLAN_SCORE_BAD_NUM;
		}
	}

	return 0;
}

static int oppo_sla_set_game_app_uid(struct nlmsghdr *nlh)
{
	u32 *uidInfo = (u32 *)NLMSG_DATA(nlh);
	u32 index = uidInfo[0];
	u32 uid = uidInfo[1];

	if (index < GAME_NUM) {
	    game_uid[index].uid = uid;
		game_uid[index].switch_time = 0;
		game_uid[index].game_type = index;
		game_uid[index].mark = WLAN_MARK;
	    printk("oppo_sla_netlink oppo_sla_set_game_app_uid:index=%d uid=%d\n", index, uid);
	}

	return 0;
}

static int oppo_sla_set_white_list_app_uid(struct nlmsghdr *nlh)
{
    u32 *info = (u32 *)NLMSG_DATA(nlh);
	memset(&white_app_list,0x0,sizeof(struct oppo_white_app_info));
    white_app_list.count = info[0];
    if (white_app_list.count > 0 && white_app_list.count < WHITE_APP_NUM) {
        int i;
        for (i = 0; i < white_app_list.count; i++) {
            white_app_list.uid[i] = info[i + 1];
            printk("oppo_sla_netlink oppo_sla_set_white_list_app_uid count=%d, uid[%d]=%d\n",
                    white_app_list.count, i, white_app_list.uid[i]);
        }
    }
	return 0;
}

static int oppo_sla_set_game_rtt_detecting(struct nlmsghdr *nlh)
{
    if(SLA_ENABLE_GAME_RTT== nlh->nlmsg_type){
        oppo_sla_rtt_detect = 1;
    } else {
        oppo_sla_rtt_detect = 0;
    }
	printk("oppo_sla_netlink: set game rtt detect:%d\n",nlh->nlmsg_type);
	return 0;
}

static int oppo_sla_set_switch_state(struct nlmsghdr *nlh)
{
	u32 *switch_enable = (u32 *)NLMSG_DATA(nlh);

	if(*switch_enable){
		sla_switch_enable = true;
		printk("oppo_sla_netlink:sla switch enabled\n");
	} else {
		sla_switch_enable = false;
		printk("oppo_sla_netlink:sla switch disabled\n");
	}
	return 0;
}

static int oppo_sla_update_screen_state(struct nlmsghdr *nlh)
{
	u32 *screen_state = (u32 *)NLMSG_DATA(nlh);
	sla_screen_on =	(*screen_state)	? true : false;
	printk("oppo_sla_netlink:update screen state = %u\n",sla_screen_on);
	return	0;
}

static int oppo_sla_update_cell_quality(struct nlmsghdr *nlh)
{
	u32 *cell_quality = (u32 *)NLMSG_DATA(nlh);
	cell_quality_good =	(*cell_quality)	? true : false;
	printk("oppo_sla_netlink:update cell quality = %u\n", cell_quality_good);
	return	0;
}

static int oppo_sla_set_show_dialog_state(struct nlmsghdr *nlh)
{
	u32 *window_state = (u32 *)NLMSG_DATA(nlh);
	need_pop_window = (*window_state) ? true : false;
	printk("oppo_sla_netlink:set show dialog = %u\n", need_pop_window);
	return	0;
}

static int oppo_sla_set_params(struct nlmsghdr *nlh)
{
	u32* params = (u32 *)NLMSG_DATA(nlh);
	u32 count = params[0];
	params++;
	if (count == 9) {
		rom_update_info.sla_speed = params[0];
		rom_update_info.cell_speed = params[1];
		rom_update_info.wlan_speed = params[2];
		rom_update_info.wlan_little_score_speed = params[3];
		rom_update_info.sla_rtt = params[4];
		rom_update_info.wzry_rtt = params[5];
		rom_update_info.cjzc_rtt = params[6];
		rom_update_info.wlan_bad_score = params[7];
		rom_update_info.wlan_good_score = params[8];
		printk("oppo_sla_netlink:set params count=%u params[0] = %u,params[1] = %u,params[2] = %u,params[3] = %u,"
				    "params[4] = %u,params[5] = %u,params[6] = %u,params[7] = %u,params[8] = %u\n",
				    count, params[0],params[1],params[2],params[3],params[4],params[5],params[6],params[7],params[8]);
	} else {
		printk("oppo_sla_netlink:set params invalid param count:%d", count);
	}

	return	0;
}

static int oppo_sla_set_game_start_state(struct nlmsghdr *nlh)
{
	int *data = (int *)NLMSG_DATA(nlh);
	int index = data[0];

	game_cell_to_wifi = false;

	if(index && index < GAME_NUM){
	        game_start_state[index] = data[1];
	        printk("oppo_sla_netlink:set game_start_state[%d] = %d\n",index,game_start_state[index]);
	        //reset udp rx data
	        if (wzry_traffic_info.game_in_front) {
	                memset(wzry_traffic_info.udp_rx_packet, 0x0, sizeof(u32)*UDP_RX_WIN_SIZE);
	                wzry_traffic_info.window_index = 0;
	        } else if (cjzc_traffic_info.game_in_front) {
	                memset(cjzc_traffic_info.udp_rx_packet, 0x0, sizeof(u32)*UDP_RX_WIN_SIZE);
	                cjzc_traffic_info.window_index = 0;
	        }
	} else {
	        printk("oppo_sla_netlink: set game_start_state error,index = %d\n",index);
	}

	return	0;
}

static int oppo_sla_set_game_rtt_params(struct nlmsghdr *nlh)
{
	int *data = (int *)NLMSG_DATA(nlh);
	int index = data[0];
	int i;

	if(index > 0 && index < GAME_NUM) {
	        memcpy(&game_params[index], data, sizeof(struct oppo_sla_game_rtt_params));
		printk("oppo_sla_netlink:set game params index=%d tx_offset=%d tx_len=%d tx_fix=",
		        game_params[index].game_index, game_params[index].tx_offset, game_params[index].tx_len);
		for (i = 0; i < game_params[index].tx_len; i++) {
		        printk("%02x", game_params[index].tx_fixed_value[i]);
		}
		printk(" rx_offset=%d rx_len=%d rx_fix=", game_params[index].rx_offset, game_params[index].rx_len);
		for (i = 0; i < game_params[index].rx_len; i++) {
		        printk("%02x", game_params[index].rx_fixed_value[i]);
		}
		printk("\n");
	} else {
		printk("oppo_sla_netlink: set game params error,index = %d\n", index);
	}

	return	0;
}

static int oppo_sla_set_game_in_front(struct nlmsghdr *nlh)
{
	int *data = (int *)NLMSG_DATA(nlh);
	int index = data[0];
	int in_front = data[1];

	if(index > 0 && index < GAME_NUM) {
	        if (index == GAME_WZRY) {
        	        if (in_front) {
            	        wzry_traffic_info.game_in_front = 1;
            	        cjzc_traffic_info.game_in_front = 0;
        	        } else {
        	            wzry_traffic_info.game_in_front = 0;
        	        }
        	        printk("oppo_sla_netlink: set_game_in_front game index = %d, in_front=%d\n", index, in_front);
	        } else if (index == GAME_CJZC) {
        	        if (in_front) {
                    	        wzry_traffic_info.game_in_front = 0;
                    	        cjzc_traffic_info.game_in_front = 1;
        	        } else {
                    	        cjzc_traffic_info.game_in_front = 0;
        	        }
        	        printk("oppo_sla_netlink: set_game_in_front game index = %d, in_front=%d\n", index, in_front);
	        } else {
	                printk("oppo_sla_netlink: set_game_in_front unknow game, index = %d\n", index);
	        }
	} else {
	        printk("oppo_sla_netlink: set_game_in_front error, index = %d\n", index);
	}

	return	0;
}

static int sla_netlink_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, struct netlink_ext_ack *extack)
{
	int ret = 0;

	switch (nlh->nlmsg_type) {
	case SLA_ENABLE:
		ret = enable_oppo_sla_module();
		break;

	case SLA_DISABLE:
		ret = disable_oppo_sla_module();
		break;

	case SLA_WIFI_UP:
	case SLA_CELLULAR_UP:
		ret = oppo_sla_iface_up(nlh);
		break;

	case SLA_WIFI_DOWN:
	case SLA_CELLULAR_DOWN:
		ret =oppo_sla_iface_down(nlh);
		break;
	case SLA_GET_PID:
		ret = oppo_sla_get_pid(skb,nlh);
		break;
	case SLA_GET_WLAN_SCORE:
		ret = oppo_sla_get_wlan_score(nlh);
		break;
	case SLA_NOTIFY_APP_UID:
	    ret = oppo_sla_set_game_app_uid(nlh);
	    break;
	case SLA_NOTIFY_WHITE_LIST_APP:
		oppo_sla_send_white_list_app_traffic();
	    ret = oppo_sla_set_white_list_app_uid(nlh);
	    break;
	case SLA_ENABLE_GAME_RTT:
	case SLA_DISABLE_GAME_RTT:
	    ret = oppo_sla_set_game_rtt_detecting(nlh);
	    break;
	case SLA_NOTIFY_SWITCH_STATE:
		ret = oppo_sla_set_switch_state(nlh);
		break;
	case SLA_NOTIFY_SCREEN_STATE:
		ret = oppo_sla_update_screen_state(nlh);
		break;
	case SLA_NOTIFY_CELL_QUALITY:
		ret = oppo_sla_update_cell_quality(nlh);
		break;
	case SLA_NOTIFY_SHOW_DIALOG:
		ret = oppo_sla_set_show_dialog_state(nlh);
		break;
	case SLA_GET_SYN_RETRAN_INFO:
		oppo_sla_send_syn_retran_info();
		break;
	case SLA_GET_SPEED_UP_APP:
		oppo_sla_send_white_list_app_traffic();
		break;
	case SLA_SET_DEBUG:
		oppo_sla_set_debug(nlh);
		break;
	case SLA_NOTIFY_DEFAULT_NETWORK:
		oppo_sla_set_default_network(nlh);
		break;
	case SLA_NOTIFY_PARAMS:
		oppo_sla_set_params(nlh);
		break;
	case SLA_NOTIFY_GAME_STATE:
		oppo_sla_set_game_start_state(nlh);
		break;
	case SLA_NOTIFY_GAME_PARAMS:
		oppo_sla_set_game_rtt_params(nlh);
		break;
	case SLA_NOTIFY_GAME_IN_FRONT:
		oppo_sla_set_game_in_front(nlh);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}


static void sla_netlink_rcv(struct sk_buff *skb)
{
	mutex_lock(&sla_netlink_mutex);
	netlink_rcv_skb(skb, &sla_netlink_rcv_msg);
	mutex_unlock(&sla_netlink_mutex);
}

static int oppo_sla_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input	= sla_netlink_rcv,
	};

	oppo_sla_sock = netlink_kernel_create(&init_net, NETLINK_OPPO_SLA, &cfg);
	return oppo_sla_sock == NULL ? -ENOMEM : 0;
}

static void oppo_sla_netlink_exit(void)
{
	netlink_kernel_release(oppo_sla_sock);
	oppo_sla_sock = NULL;
}

static int __init oppo_sla_init(void)
{
	int ret = 0;

	rwlock_init(&sla_lock);
	rwlock_init(&sla_rtt_lock);
	rwlock_init(&sla_game_lock);

	ret = oppo_sla_netlink_init();
	if (ret < 0) {
		printk("oppo_sla module can not init oppo sla netlink.\n");
	}

	ret |= oppo_sla_sysctl_init();

	ret |= nf_register_net_hooks(&init_net,oppo_sla_ops,ARRAY_SIZE(oppo_sla_ops));
	if (ret < 0) {
		printk("oppo_sla module can not register netfilter ops.\n");
	}

	workqueue_sla= create_singlethread_workqueue("workqueue_sla");
	if (workqueue_sla) {
		INIT_WORK(&oppo_sla_work, oppo_sla_work_queue_func);
	}
	else {
		printk("oppo_sla module can not create workqueue_sla\n");
	}

	oppo_sla_timer_init();
	statistic_dev_rtt = oppo_statistic_dev_rtt;
	mark_streams_for_iptables_reject = sla_mark_streams_for_iptables_reject;

	return ret;
}


static void __exit oppo_sla_fini(void)
{
	oppo_sla_timer_fini();
	statistic_dev_rtt = NULL;
	mark_streams_for_iptables_reject = NULL;

	if (workqueue_sla) {
		flush_workqueue(workqueue_sla);
		destroy_workqueue(workqueue_sla);
	}

	oppo_sla_netlink_exit();

	if(oppo_sla_table_hrd){
		unregister_net_sysctl_table(oppo_sla_table_hrd);
	}

	nf_unregister_net_hooks(&init_net,oppo_sla_ops, ARRAY_SIZE(oppo_sla_ops));
}


module_init(oppo_sla_init);
module_exit(oppo_sla_fini);
