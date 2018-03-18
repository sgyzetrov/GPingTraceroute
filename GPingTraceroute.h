/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Name: C implementation of ICMP ping and traceroute       
 * Program: GPingTraceroute.h
 * Auther: Guo Yang <guoyang@webmail.hzau.edu.cn>
 * Version: 0.0.3
 * Date(mm/dd/yyyy): 3/18/2018
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
# include <unistd.h>//sleep()
# include <sys/types.h> //socket connection 
# include <sys/socket.h>//socket connection 
# include <netinet/ip.h>//ip数据结构定义
# include <netinet/ip_icmp.h>//icmp data structure 
# include <netinet/in.h>
# include <netdb.h> //contain host info, like ip address
# include <sys/time.h> //contain gettimeofday()、timeval data structure for system time for rtt calculation
                        /*  int gettimeofday(struct timeval *tp,void *tzp)；for system time
                        tv_sec is second，tv_usec is millisecond 
                        use gettimeofday() to get time at sending and receiving packets. make a subtraction for rtt*/
# include <arpa/inet.h>//inet_ntoa() change ip address's dotted decimal notation to network byte order
# include <signal.h>//signal(SIGINT)
# include <math.h>//sqrt()

# define ICMP_DATA_LEN 56        //ICMP default length  
# define ICMP_HEAD_LEN 8         //ICMP default head length
# define ICMP_LEN  (ICMP_DATA_LEN + ICMP_HEAD_LEN)  
# define BUFFER_SIZE 128          //ICMP send's buffer use the same as linux icmp modules' sk_sndbuf: 128K  
# define MAX_PACKETS_NUM_PER_PING  6 // one ping 6 packets
# define MAX_PACKETS_NUM_PER_TRACETOUTE 30 // one traceroute 30 packets at most
# define MAX_WAIT_TIME   5   //max wait time，for alarm()

int nReceived = 0;  //actual packets received 

char sendpacket[BUFFER_SIZE];
char recvpacket[BUFFER_SIZE];

double min = 0.0;  
double avg = 0.0;  
double max = 0.0;  
double stddev = 0.0; 

struct timeval FirstSendTime;    
struct timeval LastRecvTime;    

//Gping.c 4 variables
extern struct hostent *pHost;   //host info
extern int sock_icmp;           //icmp socket
extern int nSend;               //num of send
extern int nAttempts;
extern char *IP;
extern struct sockaddr_in recv_addr;
extern struct sockaddr_in dest_addr; 	//IPv4 socket address


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
            //incase nSend<nReceived is printed
        nReceived,
        (nSend-nReceived)/nSend*100);
    printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",min, avg, max, stddev);
    
    close(sock_icmp);
	exit(1);
}
/*
    mdev = SQRT(SUM(RTT*RTT) / N – (SUM(RTT)/N)^2) mdev-->Mean Deviation 
*/

/*checksum*/
unsigned short Compute_cksum(struct icmp *pIcmp)
{  
    unsigned short *data = (unsigned short *)pIcmp;  
    int len = ICMP_LEN;  
    unsigned int sum = 0;  
      
    while (len > 1) { 
        sum += *data++;  
        len -= 2;  
    }  
    if (1 == len) {   
        unsigned short tmp = *data;  
        tmp &= 0xff00;  
        sum += tmp;  
    }  
  
    sum = (sum >> 16) + (sum & 0x0000ffff);  
    sum += (sum>>16);
    sum = ~sum;  
      
    return sum;  
}  


void Pack_ICMP(unsigned int seq)  
{  
    struct icmp *pIcmp;  
    struct timeval *pTime;//for rtt

    pIcmp = (struct icmp*)sendpacket;  
      

    pIcmp->icmp_type = ICMP_ECHO;  
    pIcmp->icmp_code = 0;  
    pIcmp->icmp_cksum = 0;       //checksun  
    pIcmp->icmp_seq = seq;        
    pIcmp->icmp_id = getpid();    
    pTime = (struct timeval *)pIcmp->icmp_data; 
    gettimeofday(pTime, NULL); 
    pIcmp->icmp_cksum = Compute_cksum(pIcmp);  
      
    if (1 == seq) 
        FirstSendTime = *pTime;  
    
}  


void send_packet(int sock_icmp, struct sockaddr_in *dest_addr, int nSend)
{       
    Pack_ICMP(nSend); 
    if( sendto(sock_icmp,sendpacket,ICMP_LEN,0,
                (struct sockaddr *)dest_addr,sizeof(dest_addr) )<0  ) {       
        perror("error occured in send_packer()...");
        return;
    }
}


double GetRtt(struct timeval *RecvTime, struct timeval *SendTime)  
{  
    if ((RecvTime->tv_usec -= SendTime->tv_usec) < 0) {  
        --(RecvTime->tv_sec);  
        RecvTime->tv_usec += 1000000;  
    }  
    RecvTime->tv_sec -= SendTime->tv_sec; 

    return RecvTime->tv_sec * 1000.0 + RecvTime->tv_usec / 1000.0; 
}  

int unPack_ICMP_Ping(struct timeval *RecvTime)  
{  
    struct ip *Ip = (struct ip *)recvpacket;  
    struct icmp *Icmp;  
    int ipHeadLen;
    int len = ICMP_LEN;  
    double rtt;  
  
    ipHeadLen = Ip->ip_hl << 2;    
    Icmp = (struct icmp *)(recvpacket + ipHeadLen);  
    
    len -= ipHeadLen;          
    if(len < 8) {                  
        printf("ICMP packets/'s length is less than 8\n");  
        return -1;  
    }  
    if ((Icmp->icmp_type == ICMP_ECHOREPLY) && Icmp->icmp_id == getpid()) {  
        struct timeval *SendTime = (struct timeval *)Icmp->icmp_data;  
        GetRtt(RecvTime, SendTime);   
        rtt = RecvTime->tv_sec * 1000.0 + RecvTime->tv_usec / 1000.0; 
          
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
    
    len -= ipHeadLen;            
    if(len < 8) {                  
        printf("ICMP packets/'s length is less than 8\n");  
        return -1;  
    }
    
    if ((Icmp->icmp_type == ICMP_TIMXCEED)) {
        tmp_src_addr = (struct sockaddr_in *)src_addr;
        printf("%s\n", inet_ntoa(tmp_src_addr->sin_addr));
    }

    else if((Icmp->icmp_type == ICMP_ECHOREPLY) && (Icmp->icmp_id == getpid())) {
        tmp_src_addr = (struct sockaddr_in *)src_addr;
        printf("%s\n", inet_ntoa(tmp_src_addr->sin_addr));
        return 1;
    }
    return 0;
}


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
    
    gettimeofday(&RecvTime, NULL);
	LastRecvTime = RecvTime;

	if (unPack_ICMP_Ping(&RecvTime) == -1) {
		return -1; 
	}
	nReceived ++;
} 

int receive_packet_Traceroute(int sock_icmp)
{
    fd_set rd_set;
    struct timeval time;
    time.tv_sec = 5;
    time.tv_usec = 0;
    int ret = 0, nread = 0;
    int addrlen = sizeof(struct sockaddr_in);
    
    FD_ZERO(&rd_set);
    FD_SET(sock_icmp, &rd_set);
    ret = select(sock_icmp + 1, &rd_set, NULL, NULL, &time);
    if (ret <= 0) return -1;
    else if (FD_ISSET(sock_icmp, &rd_set)) {
    
        if (recvfrom(sock_icmp, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&recv_addr, &addrlen) < 0 ){
            perror("In traceroute recvfrom()");
            return -1;
        }
        ret = unPack_ICMP_Traceroute(sock_icmp, recvpacket, &recv_addr);
        return ret;
    }
}


# endif
