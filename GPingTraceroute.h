/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Name: C implementation of ICMP ping and traceroute       
 * Program: GPingTraceroute.h
 * Auther: Guo Yang <guoyang@hzau.edu.cn>
 * Version: 0.0.2
 * Date(mm/dd/yyyy): 12/19/2017
 * Description: 
 *  header file for my own implements of shell command: ping / traceroute
 * 
 * References：
 * 		1. http://blog.csdn.net/qq_33724710/article/details/51576444
 * 		2. http://blog.csdn.net/joeblackzqq/article/details/41549243
 * 		3. http://www.cnblogs.com/wolflion/p/3345850.html
 * 		4. http://blog.csdn.net/qy532846454/article/details/6915835
 *      5. http://blog.sina.com.cn/s/blog_6a1837e90100uhl3.html 
 * 
 * History:
 *  1. 11/04/2017 guoyang
 *      first edit, gcc-7.1.0 + macOS 10.12.6
 *  2. 12/21/2017 guoyang
 *      added traceroute, gcc-7.1.0 + macOS 10.12.6
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

# ifndef __GPINGTRACEROUTE_H__  
# define __GPINGTRACEROUTE_H__

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>//包含sleep函数
# include <sys/types.h> //socket连接用
# include <sys/socket.h>//socket连接用
# include <netinet/ip.h>//ip数据结构定义
# include <netinet/ip_icmp.h>//icmp数据结构定义
# include <netinet/in.h>
# include <netdb.h> //包含返回对应于给定主机名的主机信息，如IP地址等。
# include <sys/time.h> //包含gettimeofday()函数、timeval结构体，获取系统时间用于计算往返时间rtt
                        /* 通过函数 int gettimeofday(struct timeval *tp,void *tzp)；来获取系统当前时间。
                        其中tv_sec为秒数，tv_usec为微秒数。
                        在发送报文和接收报文时各通过gettimeofday函数获取一次时间，两次时间差就可以求出往返时间rtt。*/
# include <arpa/inet.h>//包含inet_ntoa()函数，将一个十进制网络字节序转换为点分十进制IP格式的字符串。
# include <signal.h>//包含signal(SIGINT)函数
# include <math.h>//包含sqrt()函数

# define ICMP_DATA_LEN 56        //ICMP默认数据长度  
# define ICMP_HEAD_LEN 8         //ICMP默认头部长度  
# define ICMP_LEN  (ICMP_DATA_LEN + ICMP_HEAD_LEN)  
# define BUFFER_SIZE 128          //ICMP发送接收缓存区大小与linux icmp模块中sk_sndbuf一样设置为128K  
# define MAX_PACKETS_NUM_PER_PING  6 // 一次ping发送6个包
# define MAX_PACKETS_NUM_PER_TRACETOUTE 30 // 一次traceroute最多尝试发送30个包
# define MAX_WAIT_TIME   5   //最长等待时间，作参数传入alarm(), alarm也称为闹钟函数,用来设置信号SIGALRM在经过参数seconds指定的秒数后传送给目前的进程。
     
int nReceived = 0;  //实际接收到的报文数

char sendpacket[BUFFER_SIZE];
char recvpacket[BUFFER_SIZE];

double min = 0.0;  
double avg = 0.0;  
double max = 0.0;  
double stddev = 0.0; 

struct timeval FirstSendTime;   //用以计算总的时间  
struct timeval LastRecvTime;    //用以计算总的时间

//Gping.c中的4个变量 
extern struct hostent *pHost;   //保存主机信息
extern int sock_icmp;           //icmp套接字
extern int nSend;               //num of send
extern int nAttempts;
extern char *IP;
extern struct sockaddr_in recv_addr;
extern struct sockaddr_in dest_addr; 	//IPv4专用socket地址,保存目的地址


void final_statistics_print(int signo)
{       
    double tmp;
	avg /= nReceived;
	tmp = stddev / nReceived - avg * avg;
    stddev = sqrt(tmp);
    
    if (pHost != NULL)   
        printf("--- %s  ping statistics ---\n", pHost->h_name);  
    else  
        printf("--- %s  ping statistics ---\n", IP);
    //printf("/n--------------------PING statistics-------------------/n");
    printf("%d packets transmitted, %d packets received , %d%% packet loss\n",
        (nSend >= nReceived ? nSend : nSend+1),
            //在设置signal(SIGINT, final_statistics_print)后遇到bug
            //如果提前终止程序会导致最终打印的统计信息中nSend<nReceived，显然不对
            //所以这里加入三目运算符，屏蔽nSend<nReceived的情况
        nReceived,
        (nSend-nReceived)/nSend*100);
    printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",min, avg, max, stddev);
    
    close(sock_icmp);
	exit(1);
}
/*
统计信息中mdev的计算方法(一说stddev)
在运行 ping 命令的时候，里面有一项输出叫 mdev(一说stddev)：
在ping的源代码ping_common.c中
    tsum += triptime;
    tsum2 += (long long)triptime * (long long)triptime
以及
    tsum /= nreceived + nrepeats;
    tsum2 /= nreceived + nrepeats;
    tmdev = llsqrt(tsum2 – tsum * tsum);
所以我们可以得出：
    mdev = SQRT(SUM(RTT*RTT) / N – (SUM(RTT)/N)^2)
公式详见http://img.blog.csdn.net/20160612112244228                                      
所以 mdev(一说stddev)就是 Mean Deviation 的缩写，它表示这些 ICMP 包的 RTT 偏离平均值的程度，这个值越大说明你的网速越不稳定。
*/

/*校验和算法*/
unsigned short Compute_cksum(struct icmp *pIcmp)
{  
    unsigned short *data = (unsigned short *)pIcmp;  
    int len = ICMP_LEN;  
    unsigned int sum = 0;  
      
    while (len > 1) {  /*把ICMP报头二进制数据以2字节为单位累加起来*/
        sum += *data++;  
        len -= 2;  
    }  
    if (1 == len) {    /*若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，这个2字节数据的低字节为0，继续累加*/  
        unsigned short tmp = *data;  
        tmp &= 0xff00;  
        sum += tmp;  
    }  
  
    //ICMP校验和带进位  
    //while (sum >> 16)  
    sum = (sum >> 16) + (sum & 0x0000ffff);  
    sum += (sum>>16);
    sum = ~sum;  
      
    return sum;  
}  

/*ICMP报文封装*/
void Pack_ICMP(unsigned int seq)  
{  
    struct icmp *pIcmp;  
    struct timeval *pTime;//时间戳，用于rtt计算  
    //int packetsize = ICMP_LEN;

    pIcmp = (struct icmp*)sendpacket;  
      
    /* 类型和代码分别为ICMP_ECHO,0代表请求回送 */  
    pIcmp->icmp_type = ICMP_ECHO;  
    pIcmp->icmp_code = 0;  
    pIcmp->icmp_cksum = 0;       //校验和  
    pIcmp->icmp_seq = seq;       //序号  
    pIcmp->icmp_id = getpid();   //取进程号作为标志  
    pTime = (struct timeval *)pIcmp->icmp_data;//数据段存放发送时间  
    gettimeofday(pTime, NULL); // 记录发送时间，用于计算rtt，此函数的第二个参数tzp指针表示时区，这里只是要时间差，所以不需要，赋为NULL值。
    pIcmp->icmp_cksum = Compute_cksum(pIcmp);  //校验算法
      
    if (1 == seq) //记录时间，用于计算rtt 
        FirstSendTime = *pTime;  
    
}  

/*发送ICMP报文*/
void send_packet(int sock_icmp, struct sockaddr_in *dest_addr, int nSend)
{       
    Pack_ICMP(nSend); /*设置ICMP报头*/
    if( sendto(sock_icmp,sendpacket,ICMP_LEN,0,
                (struct sockaddr *)dest_addr,sizeof(dest_addr) )<0  ) {       
        perror("error occured in send_packer()...");
        return;
    }
}

/*计算往返时间rtt*/
double GetRtt(struct timeval *RecvTime, struct timeval *SendTime)  
{  
    /* 
    * 实现方法是通过ICMP数据报携带一个时间戳，在回送请求报文中包含了的可选数据，在应答报文中包含了该可选数据的一个副本
    * 要用到的struct timevl的结构体为
    * struct timeval{  
    *      xxxx tv_sec;  
    *      xxxx tv_usec;  
    *  }  
    */
    if ((RecvTime->tv_usec -= SendTime->tv_usec) < 0) {  
        --(RecvTime->tv_sec);  
        RecvTime->tv_usec += 1000000;  // 秒数清零，将减去的秒数加给毫秒
    }  
    RecvTime->tv_sec -= SendTime->tv_sec; 

    return RecvTime->tv_sec * 1000.0 + RecvTime->tv_usec / 1000.0; //转换单位为毫秒 
}  

/*ICMP报文解析-给ping用*/
int unPack_ICMP_Ping(struct timeval *RecvTime)  
{  
    struct ip *Ip = (struct ip *)recvpacket;  
    struct icmp *Icmp;  
    int ipHeadLen;
    int len = ICMP_LEN;  
    double rtt;  
  
    ipHeadLen = Ip->ip_hl << 2;    //求Ip报头长度,ip_hl字段单位为4字节,即Ip报头的长度标志乘4  
    Icmp = (struct icmp *)(recvpacket + ipHeadLen);  //越过Ip报头,指向ICMP报头
    
    len -= ipHeadLen;            //ICMP报头及ICMP数据报的总长度
    if(len < 8) {                 //小于ICMP报头长度则不合理  
        printf("ICMP packets/'s length is less than 8\n");  
        return -1;  
    }  
    if ((Icmp->icmp_type == ICMP_ECHOREPLY) && Icmp->icmp_id == getpid()) {  
        struct timeval *SendTime = (struct timeval *)Icmp->icmp_data;  
        GetRtt(RecvTime, SendTime);   
        rtt = RecvTime->tv_sec * 1000.0 + RecvTime->tv_usec / 1000.0; //单位毫秒  
          
        printf("%d bytes from %s: icmp_seq=%d ttl=%u time=%.3f ms\n",  
            ICMP_LEN,  
            inet_ntoa(Ip->ip_src),  
            Icmp->icmp_seq,  
            Ip->ip_ttl,  
            rtt);

        if (rtt < min || 0 == min)
			min = rtt;
		if (rtt > max)
			max = rtt;
		avg += rtt;
        stddev += rtt * rtt;  
        
        return 0;  
    }  
    else  
        return -1;  
}

/*ICMP报文解析-给traceroute用*/
int unPack_ICMP_Traceroute(int sockfd, char *recv_pkt, struct sockaddr_in *src_addr)  
{  
    struct ip *Ip = (struct ip *)recvpacket;
    struct icmp *Icmp;
    int ipHeadLen;
    double rtt;
    

    struct sockaddr_in *tmp_src_addr = NULL;
    int len = ICMP_LEN;
    
    ipHeadLen = Ip->ip_hl<<2;
    Icmp = (struct icmp *)(recv_pkt + ipHeadLen);
    
    len -= ipHeadLen;            //ICMP报头及ICMP数据报的总长度
    if(len < 8) {                 //小于ICMP报头长度则不合理  
        printf("ICMP packets/'s length is less than 8\n");  
        return -1;  
    }
    
    if ((Icmp->icmp_type == ICMP_TIMXCEED)) {
        tmp_src_addr = (struct sockaddr_in *)src_addr;
        printf("%s\n", inet_ntoa(tmp_src_addr->sin_addr));
    }
    // 到达最终目的地IP，返回1给main中的unpack_ret_traceroute，使main函数中while循环终止
    else if((Icmp->icmp_type == ICMP_ECHOREPLY) && (Icmp->icmp_id == getpid())) {
        tmp_src_addr = (struct sockaddr_in *)src_addr;
        printf("%s\n", inet_ntoa(tmp_src_addr->sin_addr));
        return 1;
    }
    return 0;
}

/*接收ICMP报文-给ping用*/
int receive_packet_Ping(int sock_icmp)//, struct sockaddr_in *dest_addr)
{
	int addrlen = sizeof(struct sockaddr_in);
	struct timeval RecvTime;
	
	signal(SIGALRM, final_statistics_print);
	alarm(MAX_WAIT_TIME);
	if (recvfrom(sock_icmp, recvpacket, ICMP_LEN,
            0, (struct sockaddr *)&dest_addr,&addrlen) < 0 ) {
		perror("In ping recvfrom()");
		return 0;
	}
    //记录接收时间，用于计算rtt，此函数的第二个参数tzp指针表示时区，这里只是要时间差，所以不需要，赋为NULL值。
    gettimeofday(&RecvTime, NULL);
	LastRecvTime = RecvTime;

	if (unPack_ICMP_Ping(&RecvTime) == -1) {
		return -1; 
	}
	nReceived ++;
} 

/*接收ICMP报文-给traceroute用*/
int receive_packet_Traceroute(int sock_icmp)
{
    fd_set rd_set;
    struct timeval time;
    time.tv_sec = 5;
    time.tv_usec = 0;
    int ret = 0, nread = 0;
    int addrlen = sizeof(struct sockaddr_in);
    
    FD_ZERO(&rd_set);// 将套节字集合清空
    FD_SET(sock_icmp, &rd_set);// 加入我要的套节字到集合,这里是一个读数据的套节字sock_icmp 
    ret = select(sock_icmp + 1, &rd_set, NULL, NULL, &time);// 检查是否超时
    if (ret <= 0) return -1;
    else if (FD_ISSET(sock_icmp, &rd_set)) {//检查sock_icmp是否在这个集合里面
    
        if (recvfrom(sock_icmp, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&recv_addr, &addrlen) < 0 ){
            perror("In traceroute recvfrom()");
            return -1;
        }
        ret = unPack_ICMP_Traceroute(sock_icmp, recvpacket, &recv_addr);
        return ret;
    }
}


# endif