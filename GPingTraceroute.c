/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Name: C implementation of ICMP ping and traceroute       
 * Program: GPingTraceroute.c
 * Auther: Guo Yang <guoyang@webmail.hzau.edu.cn>
 * Version: 0.0.3
 * Date(mm/dd/yyyy): 3/14/2018
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
 *              5. http://blog.sina.com.cn/s/blog_6a1837e90100uhl3.html 
 * 
 * History:
 *  1. 11/04/2017 guoyang
 *      first edit, gcc-7.1.0 + macOS 10.12.6
 *  2. 12/21/2017 guoyang
 *      added traceroute, gcc-7.1.0 + macOS 10.12.6
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

# include "./GPingTraceroute.h"

struct hostent * pHost = NULL;		//host info
int sock_icmp;						//icmp socket
int nSend = 0;
int nAttempts = 0;
char *IP = NULL;
struct sockaddr_in recv_addr;
struct sockaddr_in dest_addr; 	//IPv4 socket address, destination address

int main(int argc, char *argv[])
{
	struct protoent *protocol;

	in_addr_t inaddr;				//ip

	if (argc < 3 || (strcmp(argv[1], "-P") != 0 &&
                     strcmp(argv[1], "-p") != 0 &&
                     strcmp(argv[1], "-T") != 0 &&
                     strcmp(argv[1], "-t") != 0 )
        ) {//exception handling
		printf("Usage: %s -p [hostname/IP address] for ping\n or %s -t [hostname/IP address] for traceroute", argv[0], argv[0]);
		exit(EXIT_FAILURE);	
	}

	if ((protocol = getprotobyname("icmp")) == NULL) {//exception handling
		perror("error in getprotobyname(), unkown protocol");
		exit(EXIT_FAILURE);
	}

    // case: ping
    if(!strcmp(argv[1], "-P") || !strcmp(argv[1], "-p")){
        /* create ICMP socket */
        //AF_INET:IPv4, SOCK_RAW:IP protocol interface, IPPROTO_ICMP: ICMP protocol
        if ((sock_icmp = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0) {
            perror("socket failed, you need root");
            exit(EXIT_FAILURE);
        }
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;

        /* change ip address's dotted decimal notation to network byte order */
        if ((inaddr = inet_addr(argv[2])) == INADDR_NONE) { //user input hostname, not ip, need to resolve
            //gethostbyname() request ip address from DNS
            if ((pHost = gethostbyname(argv[2])) == NULL) {
                herror("error in gethostbyname(), invalid argument");
                exit(EXIT_FAILURE);
            }
            memmove(&dest_addr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
        }
        else {//user input ip, no need to resolve
            memmove(&dest_addr.sin_addr, &inaddr, sizeof(struct in_addr));
        }

        if (pHost != NULL)
            printf("PING %s (%s) %d data bytes.\n" , pHost->h_name, inet_ntoa(dest_addr.sin_addr), ICMP_DATA_LEN);
        else
            printf("PING %s (%s) %d data bytes.\n" , argv[2], inet_ntoa(dest_addr.sin_addr), ICMP_DATA_LEN);

        IP = argv[2];
        signal(SIGINT, final_statistics_print);
        while (nSend < MAX_PACKETS_NUM_PER_PING) { //MAX_PACKETS_NUM_PER_PING num of datagram
            int unpack_ret;
            send_packet(sock_icmp, &dest_addr, nSend);
            unpack_ret = receive_packet_Ping(sock_icmp);//, &dest_addr);
            if (-1 == unpack_ret)	//received own datagram, waiting for new income
                receive_packet_Ping(sock_icmp);//, &dest_addr);
            sleep(1);
            nSend++;
        }
        final_statistics_print(0);	//print statistics
    }

    // case: traceroute
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
            
            /* change ip address's dotted decimal notation to network byte order */
            if ((inaddr = inet_addr(argv[2])) == INADDR_NONE) { //user input hostname, not ip, need to resolve
                //gethostbyname() request ip address from DNS
                if ((pHost = gethostbyname(argv[2])) == NULL) {
                    herror("error in gethostbyname(), invalid argument");
                    exit(EXIT_FAILURE);
                }
                memmove(&dest_addr.sin_addr, pHost->h_addr_list[0], pHost->h_length);
            }
            else {//user input ip, no need to resolve
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
