/*
 * Author: Wenzhao Zhang;
 * UnityID: wzhang27;
 *
 * CSC501 OS P3
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
/*#include <sys/socket.h>
#include <bits/socket.h>   SOMAXCONN */
#include <bits/socket.h>

#ifndef MASTER_DEBUG
#define MASTER_DEBUG 0
#endif

/* #define MAX_Q_NUM 20
#define BASE_PORT 1025  every player's port is BASE_PORT+index */
#define MAX_TRY_CONN 10
#define SEND_BLOCK 256

#define MY_MAXHOST 192
#define MY_MAXSERV 16

/* define some types for _msg */
#define SEND_P 1	/* send potato */
#define REPORT_P 2  /* player's hops is 0, report to master */
#define EXIT 3      /* master will shutdown the ring */
typedef struct _msg_st{
	int type;
	int hops;
	int sent_times;
	int sender; /* -1: master, other number: player's index */
	int receiver;
}_msg;

typedef struct _player_info_st{
	char host[MY_MAXHOST+1];
	char port[MY_MAXSERV+1];
	int index;
}_player_info;


typedef struct _master_core_st{
	char host[MY_MAXHOST+1];
	char port[MY_MAXSERV+1];
	int port1;
	int player_number;
	int hops;
	int lis_socket;   /* init in init_master(), destroyed in main() */
	_player_info * player_list;
}_master_core;

typedef struct _neighbor_msg_st{
	int exit;
	int left_index;
	char left_host[MY_MAXHOST+1];
	char left_port[MY_MAXSERV+1];
	int right_index;
	char right_host[MY_MAXHOST+1];
	char right_port[MY_MAXSERV+1];
}_neighbor_msg;

static _master_core * master_engine=NULL;


int generate_random(){
	int value;

	value = rand();
	return value%master_engine->player_number;
}

void send_msg(_msg * msg, int sock){
	int struct_size;
	char * buf;

	if(msg==NULL || sock<0){
#if MASTER_DEBUG
		printf("**** send_msg, msg or socket is invalid\n");
#endif
		return;
	}
	struct_size = sizeof( _msg );
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );

	if(buf==NULL){
#if MASTER_DEBUG
		printf("**** send_msg, memory error.\n");
#endif
		exit(0);
	}

	memset(buf, 0, sizeof(char)*(struct_size+1) );
	memcpy( buf, msg, sizeof(char)*(struct_size) );
	send(sock, buf, sizeof(char)*(struct_size), 0);
	free(buf);
}

_msg * recv_msg(int sock){
	int struct_size;
	char * buf;
	_msg * msg=NULL;

	if(sock<0){
#if MASTER_DEBUG
		printf("**** recv_msg, msg or socket is invalid\n");
#endif
		return NULL;
	}
	struct_size = sizeof( _msg );
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );
	msg = (_msg*)malloc( sizeof(_msg) );

	if(buf==NULL || msg==NULL){
#if MASTER_DEBUG
		printf("**** recv_msg, memory error.\n");
#endif
		exit(0);
	}

	memset(buf, 0, sizeof(char)*(struct_size+1) );
	recv(sock, buf, sizeof(char)*struct_size, 0);
	memcpy(msg, buf, sizeof(char)*struct_size);

	free(buf);

	return msg;
}

/* caller is responsible to free this msg */
_msg * create_msg(int receiver, int type){
	_msg * msg = NULL;

	msg = (_msg*)malloc( sizeof(_msg) );
	if(msg==NULL){
#if MASTER_DEBUG
		printf("**** create_msg, memory error.\n");
#endif
		exit(0);
	}

	msg->sender=-1;
	msg->type=type;
	msg->hops=master_engine->hops;
	msg->sent_times=0;
	msg->receiver=receiver;

	return msg;
}

_neighbor_msg * create_neighbor_msg(){
	_neighbor_msg * msg = NULL;

	msg = (_neighbor_msg*)malloc( sizeof(_neighbor_msg) );

	if(msg==NULL){
#if MASTER_DEBUG
		printf("**** create_neighbor_msg, memory error.\n");
#endif
		exit(0);
	}

	memset( msg->left_host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( msg->left_port, 0, sizeof(char)*(MY_MAXSERV+1) );
	memset( msg->right_host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( msg->right_port, 0, sizeof(char)*(MY_MAXSERV+1) );
	msg->left_index=-1;
	msg->right_index=-1;
	msg->exit=0;

	return msg;
}

int find_socket(const char *hostname, const char *service, int family, int socktype) {

    struct addrinfo hints, *res, *ressave;
    int n, sockfd;

#if MASTER_DEBUG
		printf("**** find_socket, to find socket for(%s, %s)\n", hostname, service );
#endif

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if (n <0) {
#if MASTER_DEBUG
		printf("**** find_socket(%s, %s), getaddrinfo error:%s\n", hostname, service, gai_strerror(n) );
#endif
        return -1;
    }

    ressave = res;
    sockfd=-1;
    while (res) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        if (!(sockfd < 0)) {
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {

                break;
            }

            close(sockfd);
            sockfd=-1;
        }
        res=res->ai_next;
    }

    freeaddrinfo(ressave);
    return sockfd;
}  /* end of find socket */

int setup_listener(const char *hostname, const char *service, int family, int socktype) {
	struct addrinfo hints, *res, *ressave;
	int n, sockfd;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = family;
	hints.ai_socktype = socktype;

	n = getaddrinfo(hostname, service, &hints, &res);
	if (n < 0) {
#if MASTER_DEBUG
		printf("**** setup_listener(%s, %s), getaddrinfo error:%s\n", hostname, service, gai_strerror(n));
#endif
		return -1;
	}

	ressave = res;
	sockfd = -1;
	while (res) {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); /* int socket(int domain,int type,int protocol); */

		if (!(sockfd < 0)) {
			if (bind(sockfd, res->ai_addr, res->ai_addrlen) == 0){

				break;
			}

			close(sockfd);
			sockfd = -1;
		}
		res = res->ai_next;
	}

	if (sockfd < 0) {
		freeaddrinfo(ressave);
#if MASTER_DEBUG
		printf("**** setup_listener(%s, %s), socket error:: could not open socket\n", hostname, service);
#endif
		return -1;
	}

	listen(sockfd, SOMAXCONN);
	freeaddrinfo(ressave);
	return sockfd;
}  /* end of setup_listener */

/* setup a listener, and wait for N players to connect */
void init_master(){
	int listenfd;
	int i=0;
	int connfd;
	int player_number;

	socklen_t addrlen;
	struct sockaddr_storage clientaddr;

	srand( time(NULL) );  /* also generate random seed */

	listenfd = setup_listener(NULL, master_engine->port, AF_INET , SOCK_STREAM);

	if (listenfd < 0) {
#if MASTER_DEBUG
		printf("**** init_master, cannot setup listener for master\n");
#endif
		exit(0);
	}
	master_engine->lis_socket = listenfd;

	addrlen = sizeof(clientaddr);
	player_number=master_engine->player_number;
	i = 0;
	while( i<player_number ){
		connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &addrlen);

		if (connfd < 0){
#if MASTER_DEBUG
			printf("**** init_master, invalid connection\n");
#endif
			i++;
			continue;
		}

		getnameinfo((struct sockaddr *) &clientaddr, addrlen, master_engine->player_list[i].host,
				sizeof(char)*MY_MAXHOST, master_engine->player_list[i].port, sizeof(char)*MY_MAXSERV, NI_NUMERICSERV);

#if MASTER_DEBUG
		printf("**** init_master, player %d, (%s, %s) connects\n", i, master_engine->player_list[i].host, master_engine->player_list[i].port);
#endif

		memset( master_engine->player_list[i].port, 0, sizeof(char)*(MY_MAXSERV+1) );
		sprintf(master_engine->player_list[i].port, "%d", ( master_engine->port1 + 1 + i) );
#if MASTER_DEBUG
		printf("**** init_master, player %d's permanent connection info (%s, %s)\n", i, master_engine->player_list[i].host, master_engine->player_list[i].port);
#endif

		send(connfd, &i, sizeof(int), 0);

		/* print out required player's info */
		printf("player %i is on %s\n", i, master_engine->player_list[i].host);

		i++;

		close(connfd);
	}
	/*close(listenfd);*/
}   /* end of init_master */

void shutdown_nk(){
	int i;
	int send_socket;
	_msg * msg;

	for(i=0; i<master_engine->player_number; i++){
		send_socket = find_socket( master_engine->player_list[i].host, master_engine->player_list[i].port, AF_INET , SOCK_STREAM);
		if(send_socket<0){
#if MASTER_DEBUG
			printf("**** shutdown_nk, master cannot send shutdown msg to %d, %s, %s\n", master_engine->player_list[i].index,
					master_engine->player_list[i].host, master_engine->player_list[i].port);
#endif
			continue;
		}
		msg = create_msg(i, EXIT);

		send_msg(msg, send_socket);
		close(send_socket);
		free(msg);
	} /* end of for */
}

/* notify each player its own neighbors' info */
void send_neighbors_info(){
	int i;
	int connfd;
	int left_neighbor, right_neighbor;

	_neighbor_msg * n_msg = NULL;
	char * buf=NULL;
	int struct_size;
	int j=0;

	struct_size = sizeof(_neighbor_msg);
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );
	if(buf==NULL){
#if MASTER_DEBUG
		printf("**** sends_neighbors_info, memory error1.\n");
#endif
		exit(0);
	}

	for(i=0; i<master_engine->player_number; i++){
		/* connect to a socket */
		j=1;
		connfd = find_socket(master_engine->player_list[i].host, master_engine->player_list[i].port, AF_INET , SOCK_STREAM);
		while( connfd<0 && j<=MAX_TRY_CONN ){
			sleep(1);
			connfd = find_socket(master_engine->player_list[i].host, master_engine->player_list[i].port, AF_INET , SOCK_STREAM);
			j++;
		}

		if (connfd < 0) {
#if MASTER_DEBUG
			printf("**** sends_neighbors_info, cannot connect to player(%s, %s)\n", master_engine->player_list[i].host, master_engine->player_list[i].port);
#endif

			/* !!!!! should do some cleanup work !!!!! */
			shutdown_nk();
			exit(0);
		}

		/* if the this player is in a "middle" position */
		if( i>0 && i< master_engine->player_number-1 ){
			left_neighbor = i-1;
			right_neighbor = i+1;
		}else{
			if( i==0 ){
				left_neighbor = master_engine->player_number-1;
				right_neighbor = i+1;
			}else if( i==master_engine->player_number-1 ){
				left_neighbor = i-1;
				right_neighbor = 0;
			}else{
#if MASTER_DEBUG
			printf("**** sends_neighbors_info, falls into un-handled case, i:%d\n", i);
#endif
			}
		} /* end of, calculating left/right neighbor */


#if MASTER_DEBUG
			printf("**** sends_neighbors_info, i:%d, left_n:%d, right_n:%d \n", i, left_neighbor, right_neighbor);
#endif

		n_msg = create_neighbor_msg();
		memcpy(n_msg->left_host, master_engine->player_list[left_neighbor].host, sizeof(char)*MY_MAXHOST );
		memcpy(n_msg->left_port, master_engine->player_list[left_neighbor].port, sizeof(char)*MY_MAXSERV );
		n_msg->left_index = left_neighbor;
		memcpy(n_msg->right_host, master_engine->player_list[right_neighbor].host, sizeof(char)*MY_MAXHOST );
		memcpy(n_msg->right_port, master_engine->player_list[right_neighbor].port, sizeof(char)*MY_MAXSERV );
		n_msg->right_index = right_neighbor;


		memset(buf, 0, sizeof(char)*(struct_size+1) );
		memcpy(buf, n_msg, sizeof(char)*struct_size );
		send(connfd, buf, sizeof(char)*struct_size, 0);

		close(connfd);
		free(n_msg);
	} /* end of for */
	free(buf);

} /* end of sends_neighbors_info() */


/* initially send potato to a player, and wait for report when the hops is 0 */
void process_potato(){
	int p_index;
	_msg * msg=NULL;
	int connfd;     /* socket to accept player's report */
	socklen_t addrlen;
	struct sockaddr_storage clientaddr;
	int p_socket;   /* socket to initially send the potato */
	int i, j;
	int * track=NULL; /* hops track */
	int * t_track;

	/* variables to handle large track size */
	int recv_l;
	int to_recv_l;
	int to_recv_block;

	p_index = generate_random();
	while(p_index<0 || p_index>master_engine->player_number){
		p_index = generate_random();
	}
	if(p_index==master_engine->player_number){
		p_index--;   /* player's index is from 0to n-1 */
	}

#if MASTER_DEBUG
	printf("**** send_potato_listen, master will send potato to %d\n", p_index);
#endif


	msg = create_msg(p_index, SEND_P);
	p_socket = find_socket(master_engine->player_list[p_index].host, master_engine->player_list[p_index].port, AF_INET , SOCK_STREAM);
	i=1;
	while( p_socket<0 && i<=MAX_TRY_CONN ){
		sleep(1);
		p_socket = find_socket(master_engine->player_list[p_index].host, master_engine->player_list[p_index].port, AF_INET , SOCK_STREAM);
		i++;
	}
	if (p_socket < 0) {
#if MASTER_DEBUG
		printf("**** send_potato_listen, cannot connect to player(%s, %s) to send potato\n", master_engine->player_list[p_index].host, master_engine->player_list[p_index].port);
#endif

		/* !!!!! should do some cleanup work !!!!! */
		shutdown_nk();
		exit(0);
	}
	send_msg(msg, p_socket);
	free(msg);
	close(p_socket);
	printf("All players present, sending potato to player %d\n", p_index);

	/* listen when the hops is 0 */
	while(1){
		connfd = accept(master_engine->lis_socket, (struct sockaddr *) &clientaddr, &addrlen);
		if (connfd < 0){
#if MASTER_DEBUG
			printf("**** send_potato_listen, invalid connection\n");
#endif
			continue;
		}
		msg = recv_msg(connfd);

		/* if a player reports hops is 0, calculate the space needed to receive the hops-track */
		if(msg->type == REPORT_P){
			/* then prepare to receive hops-track */
			track = (int*)malloc( sizeof(int)*msg->sent_times + 1 );
			memset(track, 0, sizeof(int)*msg->sent_times + 1 );

			/* !!!!  more work needed to handle large track */
			to_recv_l = sizeof(int) * msg->sent_times;
			if (to_recv_l > SEND_BLOCK) {
				to_recv_block = SEND_BLOCK;
				t_track = track;

				while (to_recv_l > 0) {
					recv_l = recv(connfd, t_track, to_recv_block, 0);
					t_track += ( recv_l / (sizeof(int)) );
					to_recv_l -= recv_l;
					if (to_recv_l < SEND_BLOCK)
						to_recv_block = to_recv_l;
				}

			} else {
				recv(connfd, track, sizeof(int)*msg->sent_times, 0);
			}


#if MASTER_DEBUG
			printf("**** send_potato_listen, hops is 0, player:%d\n", msg->sender);
#endif

			/*print out the track*/
			printf("Trace of potato:\n");
			for(j=0; j < msg->sent_times; j++){
				printf("%d", *(track+j) );
				if( j < (msg->sent_times-1) )
					printf(",");
			}

			shutdown_nk(); /* shutdown the ring */

			free(track);
			break;
		}

		free(msg);
		close(connfd);
	} /* end of while */

}  /* process_potato */

int main(int argc, char * argv[]) {
	int i;
	if(argc!=4){
#if MASTER_DEBUG
		printf("**** USAGE: master <port-number> <number-of-players> <hops>\n");
#endif
		exit(0);
	}

	master_engine = (_master_core*) malloc( sizeof(_master_core) );
	if(master_engine==NULL){
#if MASTER_DEBUG
		printf("**** main, memory error1.\n");
#endif
		exit(0);
	}
	memset(master_engine->host, 0, sizeof(char)*(MY_MAXHOST+1) );/**/
	memset(master_engine->port, 0, sizeof(char)*(MY_MAXSERV+1) );

	sprintf(master_engine->port, "%s", argv[1]);
	master_engine->player_number = atol(argv[2]);
	master_engine->hops = atol(argv[3]);
	gethostname( master_engine->host, sizeof(char)*MY_MAXHOST );
	master_engine->player_list = NULL;
	master_engine->lis_socket = -1;
	master_engine->port1 = atoi( argv[1] );

	if( master_engine->player_number<=1 || master_engine->hops<0 ){
#if MASTER_DEBUG
		printf("**** main, number-of-players(%d) should be greater than one and hops(%d) should be non­negative\n.",  master_engine->player_number,  master_engine->hops);
#endif
		exit(0);
	}

#if MASTER_DEBUG
		printf("**** main, port:%s, player_number:%d, hops:%d\n", master_engine->port, master_engine->player_number, master_engine->hops);
#endif

	/* init all player's info */
	master_engine->player_list = (_player_info*)malloc( master_engine->player_number * sizeof(_player_info) );
	if( master_engine->player_list==NULL ){
#if MASTER_DEBUG
		printf("**** main, memory error2.\n");
#endif
		exit(0);
	}
	for(i=0;i<master_engine->player_number;i++){
		master_engine->player_list[i].index = -1;
		memset( master_engine->player_list[i].host, 0, sizeof(char)*(MY_MAXHOST+1) );
		memset( master_engine->player_list[i].port, 0, sizeof(char)*(MY_MAXSERV+1) );
	}

	/* print out required initial info */
	printf("Potato Master on %s\n", master_engine->host);
	printf("Players = %d\n", master_engine->player_number);
	printf("Hops = %d\n", master_engine->hops);

	/* must have master_engine->player_number players connected, before this func could return */
	init_master();

	/* sends each player its left/right neighbors info */
	send_neighbors_info();

	if(master_engine->hops!=0)
		process_potato();
	else
		shutdown_nk();

	close(master_engine->lis_socket);
	exit(0);
}  /* end of main */
