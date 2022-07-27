#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "KafkaProducer.h"
#include "cJSON.h"

#define	TRAFFIC_IDENTIFY_AV 	"traffic_identify_av.so"
#define MAX_PATH_LEN		256

typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;

typedef enum
{
	TCP,
	UDP,
	SSL,
	TYPE_NUM
}stream_type;

struct ack_str
{
    long payload_bytes;
    unsigned int pak_count;
    unsigned long ack_id;
};

typedef enum
{
	LEN_RELATED,//total_bytes,avg_pkt_len
	TIME_RELATED//duration,Bps
}identi_type;

typedef struct PackHead
{
 
    BYTE ver_hlen;      //IP协议版本和IP首部长度。高4位为版本，低4位为首部的长度(单位为4bytes)
    BYTE byTOS;       //服务类型
    WORD wPacketLen; //IP包总长度。包括首部，单位为byte。[Big endian]
    WORD wSequence;    //标识，一般每个IP包的序号递增。[Big endian] 
    WORD Flags; //标志
    
    BYTE byTTL;         //生存时间 
    BYTE byProtocolType; //协议类型，见PROTOCOL_TYPE定义
    WORD wHeadCheckSum;    //IP首部校验和[Big endian]
    WORD dwIPSrc;         //源地址
    WORD dwIPSrc2;
    WORD dwIPDes;         //目的地址
    WORD dwIPDes2;
    //BYTE Options;          //选项s 
    WORD SrcPort;       //源端口
    WORD DesPort;       //目的端口
    WORD SeqNum;
    WORD SeqNum2;
    WORD AckNum;
    WORD AckNum2;
    WORD tcpFlag;
    WORD WinSize;
    WORD CheckSum;
    WORD UrgentPoin;
} pak_HEAD;

typedef struct _traffic_identify_parameter
{
    FILE* file;
    void* log_handle;
    short csv_record_flag;
    unsigned int ack_payload_threshlod;
    unsigned int ack_paknum_threshlod;
    unsigned int time_win_size;
    unsigned int csv_output_state;
    unsigned int kafka_output_feature_state;
    unsigned int kafka_output_stream_state;
    unsigned int	send_kafka_flag;
    unsigned int use_kafka;
    short	burst_timestp_type;//0:rpt_time,1:sys_time
    short	identifier_type;//bit expression
	unsigned int	min_pktsnum;// > min_pktsnum for identify
	unsigned int	total_bytes;
	unsigned int	duration;
	unsigned int	avg_pkt_len;
	unsigned int	Bps;
	unsigned int	min_bytes;// >min_bytes for runtimelog
	unsigned int	max_outoforder;

	unsigned int burst_interval;
    unsigned int burst_payload_threshlod;
    unsigned int burst_paknum_threshlod;
    unsigned int burst_chunkcount_threshlod;
    unsigned int burst_feature_output_chunk_count;

    unsigned int burst_list_len;
    unsigned int ack_list_len;

    unsigned int run_mode;

    KafkaProducer*	kafka_producer;
    
    char kafka_brokers[512];
	char topic_name[128];
}traffic_identify_parameter;

typedef struct _traffic_identify_pme
{   
    bool rule_label;
    bool ML_ACK_labeled_flag;//标志是否已进行ML和ACK预测
    bool burst_label;
    char ACK_label;

    unsigned int payload_cnt;
    int SSL_flag;
    int ack_count;
    unsigned long current_ack_num;

    short c2s_payload_max;
    short s2c_payload_max;
    unsigned short win_c2s_max;
    unsigned short win_c2s_min;
    unsigned short win_s2c_max;
    unsigned short win_s2c_min;
    long long win_c2s_sum;
    long long win_s2c_sum;
    int found_time;
    //long time_interval_min_c2s;//ms
    //long time_interval_min_s2c;
    //long time_interval_max_c2s;
    //long time_interval_max_s2c;
    long* burst_time_interval_s2c;//ms

    char SNI[1024];
    
    struct ack_str* ack_list;
    
    unsigned long long pre_time_s2c;
    unsigned long long pre_time_c2s;
    int burst_chunk_count;

    long* payload_burst_list_s2c;
    int* paknum_burst_list_s2c;
    long* payload_burst_list_c2s;
    int* paknum_burst_list_c2s;

    cJSON*	cjson_obj;

}traffic_identify_pmeinfo;


#ifdef __cplusplus
extern "C" {
#endif

UCHAR TRAFFIC_IDENTIFY_AV_UDP_ENTRY(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet);
UCHAR TRAFFIC_IDENTIFY_AV_TCP_ENTRY(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet);
//UCHAR TRAFFIC_IDENTIFY_SSL_ENTRY(stSessionInfo *session_info, void **pme, int thread_seq, struct streaminfo *a_stream, void *a_packet);
int TRAFFIC_IDENTIFY_AV_INIT(void);
void TRAFFIC_IDENTIFY_AV_DESTROY(void);
#ifdef __cplusplus
}
#endif