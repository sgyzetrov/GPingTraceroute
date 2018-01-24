/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Name: C implementation of ICMP ping and traceroute       
 * Program: GPingTraceroute.c
 * Auther: Guo Yang <guoyang@webmail.hzau.edu.cn>
 * Version: 0.0.2
 * Date(mm/dd/yyyy): 12/19/2017
 * Description: 
 *  main function for my own implements of shell command: ping / traceroute
 *  Usage: ./GPingTraceroute -p [hostname/IP address] for ping
 *      or ./GPingTraceroute -t [hostname/IP address] for traceroute
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

# include "./GPingTraceroute.h"

struct hostent * pHost = NULL;		//保存主机信息
int sock_icmp;						//icmp套接字
int nSend = 0;
int nAttempts = 0;
char *IP = NULL;
struct sockaddr_in recv_addr;
struct sockaddr_in dest_addr; 	//IPv4专用socket地址,保存目的地址

int main(int argc, char *argv[])
{
	struct protoent *protocol;

	in_addr_t inaddr;				//ip地址（网络字节序）

	if (argc < 3 || (strcmp(argv[1], "-P") != 0 &&
                     strcmp(argv[1], "-p") != 0 &&
                     strcmp(argv[1], "-T") != 0 &&
                     strcmp(argv[1], "-t") != 0 )
        ) {//处理异常
		printf("Usage: %s -p [hostname/IP address] for ping\n or %s -t [hostname/IP address] for traceroute", argv[0], argv[0]);
		exit(EXIT_FAILURE);	
	}

	if ((protocol = getprotobyname("icmp")) == NULL) {//处理异常
		perror("error in getprotobyname(), unkown protocol");
		exit(EXIT_FAILURE);
	}

    // ping的情况
    if(!strcmp(argv[1], "-P") || !strcmp(argv[1], "-p")){
        /* 创建ICMP套接字 */
        //AF_INET:IPv4, SOCK_RAW:IP协议数据报接口, IPPROTO_ICMP:ICMP协议
        if ((sock_icmp = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0) {
            perror("socket failed, you need root");
            exit(EXIT_FAILURE);
        }
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;

        /* 将点分十进制ip地址转换为网络字节序 */
        if ((inaddr = inet_addr(argv[2])) == INADDR_NONE) { //转换失败，表明是主机名,需通过主机名获取ip
            //gethostbyname函数用来向DNS查询一个域名的IP地址
            if ((pHost = gethostbyname(argv[2])) == NULL) {
                herror("error in gethostbyname(), invalid argument");
                exit(EXIT_FAILURE);
            }
            memmove(&dest_addr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
        }
        else {//用户输入的就是IP，那就不用转换了
            memmove(&dest_addr.sin_addr, &inaddr, sizeof(struct in_addr));
        }

        if (pHost != NULL)
            printf("PING %s (%s) %d data bytes.\n" , pHost->h_name, inet_ntoa(dest_addr.sin_addr), ICMP_DATA_LEN);
        else
            printf("PING %s (%s) %d data bytes.\n" , argv[2], inet_ntoa(dest_addr.sin_addr), ICMP_DATA_LEN);

        IP = argv[2];
        signal(SIGINT, final_statistics_print);
        while (nSend < MAX_PACKETS_NUM_PER_PING) { //一共发送MAX_PACKETS_NUM_PER_PING次报文
            int unpack_ret;
            send_packet(sock_icmp, &dest_addr, nSend);
            unpack_ret = receive_packet_Ping(sock_icmp);//, &dest_addr);
            if (-1 == unpack_ret)	//（ping回环时）收到了自己发出的报文,重新等待接收
                receive_packet_Ping(sock_icmp);//, &dest_addr);
            sleep(1);
            nSend++;
        }
        final_statistics_print(0);	//输出统计信息
    }

    // traceroute的情况
    else if(!strcmp(argv[1], "-T") || !strcmp(argv[1], "-t")){
        int i, unpack_ret_traceroute, ttl;
        int nSend_traceroute = 0;

        while(nAttempts < MAX_PACKETS_NUM_PER_TRACETOUTE){
            ttl = nAttempts + 1;
            if ((sock_icmp = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0)
            {
                printf("socket fail, you need root\n");
                return -1;
            }

            setsockopt(sock_icmp, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

            memset(&dest_addr, 0, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            
            /* 将点分十进制ip地址转换为网络字节序 */
            if ((inaddr = inet_addr(argv[2])) == INADDR_NONE) { //转换失败，表明是主机名,需通过主机名获取ip
                //gethostbyname函数用来向DNS查询一个域名的IP地址
                if ((pHost = gethostbyname(argv[2])) == NULL) {
                    herror("error in gethostbyname(), invalid argument");
                    exit(EXIT_FAILURE);
                }
                memmove(&dest_addr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
            }
            else {//用户输入的就是IP，那就不用转换了
                memmove(&dest_addr.sin_addr, &inaddr, sizeof(struct in_addr));
            }

            printf("* %d  ", nAttempts + 1);
            send_packet(sock_icmp, &dest_addr, nSend_traceroute++);
            unpack_ret_traceroute = receive_packet_Traceroute(sock_icmp);
            close(sock_icmp);
            if(1 == unpack_ret_traceroute) break;
            nAttempts++;
        }   
    }
	return 0;
}
