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
#include <bits/socket.h>

#ifndef PLAYER_DEBUG
#define PLAYER_DEBUG 0
#endif

/* #define BASE_PORT 1025 every player's port is BASE_PORT+index */
/* #define MAX_Q_NUM 20 */
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

typedef struct _player_core_st{
	/* this player's own host/port info */
	char host[MY_MAXHOST+1];
	char port[MY_MAXSERV+1];

	/* master's host/port info */
	char master_host[MY_MAXHOST+1];
	char master_port[MY_MAXSERV+1];

	/* neighbor's host/port info */
	int left_index;
	char left_host[MY_MAXHOST+1];
	char left_port[MY_MAXSERV+1];
	int right_index;
	char right_host[MY_MAXHOST+1];
	char right_port[MY_MAXSERV+1];

	int index;

	int lis_socket;  /* init in get_neighbors_info() */

	int base_port; /* base_port is master's port, every player's port is BASE_PORT+index+1 */
}_player_core;

typedef struct _neighbor_msg_st{
	int exit;
	int left_index;
	char left_host[MY_MAXHOST+1];
	char left_port[MY_MAXSERV+1];
	int right_index;
	char right_host[MY_MAXHOST+1];
	char right_port[MY_MAXSERV+1];
}_neighbor_msg;

static _player_core * player_engine;

int generate_random(){
	int value;

	value = rand();
	return value%2;
}

void send_msg(_msg * msg, int sock){
	int struct_size;
	char * buf;

	if(msg==NULL ){
#if PLAYER_DEBUG
		printf("**** send_msg, msg is invalid\n");
#endif
		return;
	}
	if(sock<0){
#if PLAYER_DEBUG
		printf("**** send_msg, socket is invalid\n");
#endif
		return;
	}

	struct_size = sizeof( _msg );
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );

	if(buf==NULL){
#if PLAYER_DEBUG
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
#if PLAYER_DEBUG
		printf("**** recv_msg, msg or socket is invalid\n");
#endif
		return NULL;
	}
	struct_size = sizeof( _msg );
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );
	msg = (_msg*)malloc( sizeof(_msg) );

	if(buf==NULL || msg==NULL){
#if PLAYER_DEBUG
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

/*
int find_socket(const char *hostname, const char *service, int family, int socktype){
	int s, rc, len, port;
	struct hostent *hp;
	struct sockaddr_in sin;

	rc=-1;
	hp = gethostbyname( hostname );
	if (hp == NULL) {
#if PLAYER_DEBUG
		printf("**** find_socket, %s: host not found\n", hostname);
#endif

		return rc;
	}
	port = atoi( service );

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
#if PLAYER_DEBUG
		printf("**** find_socket, cannot create socket\n", hostname);
#endif

		return rc;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);

	rc = connect(s, (struct sockaddr *) &sin, sizeof(sin));
	if (rc < 0) {
#if PLAYER_DEBUG
		printf("**** find_socket, cannot connect\n", hostname);
#endif

		return rc;
	}

	return rc;
} */


int find_socket(const char *hostname, const char *service, int family, int socktype) {

    struct addrinfo hints, *res, *ressave;
    int n, sockfd;

#if PLAYER_DEBUG
		printf("**** find_socket, to find socket for(%s, %s)\n", hostname, service );
#endif

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = family;
    hints.ai_socktype = socktype;

    n = getaddrinfo(hostname, service, &hints, &res);

    if (n <0) {
#if PLAYER_DEBUG
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
#if PLAYER_DEBUG
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
#if PLAYER_DEBUG
		printf("**** setup_listener(%s, %s), socket error:: could not open socket\n", hostname, service);
#endif
		return -1;
	}

	listen(sockfd, SOMAXCONN);
	freeaddrinfo(ressave);
	return sockfd;
}  /* end of setup_listener */

/* connect to master for the first time, to get its index */
void init_player(){
	int connfd;
    struct sockaddr_in my_sock_addr;
    socklen_t my_sock_len;
    int index;
    int i;

	srand( time(NULL) );  /* also generate random seed */

	i=1;
	connfd = find_socket(player_engine->master_host, player_engine->master_port, AF_INET, SOCK_STREAM);
	while( connfd<0 && i<=MAX_TRY_CONN ){
		sleep(1);
		connfd = find_socket(player_engine->master_host, player_engine->master_port, AF_INET, SOCK_STREAM);
		i++;
	}

	if ( connfd < 0) {
#if PLAYER_DEBUG
		printf("**** init_player, cannot connect to master\n");
#endif
		exit(0);
	}

	my_sock_len = sizeof(my_sock_addr);
	getsockname(connfd, &my_sock_addr, &my_sock_len);
	/* converts an address into a host name and a service name */
	getnameinfo((struct sockaddr *) &my_sock_addr, my_sock_len,
			player_engine->host, sizeof(char)*MY_MAXHOST, player_engine->port,
			sizeof(char)*MY_MAXSERV, NI_NUMERICHOST);
#if PLAYER_DEBUG
		printf("**** init_player, this player use host&port(%s, %s) to initially connect to master\n", player_engine->host, player_engine->port);
#endif


	recv(connfd, &index, sizeof(int), 0 );
	player_engine->index = index;
	sprintf(player_engine->port, "%d", (player_engine->base_port + 1 + player_engine->index) );

#if PLAYER_DEBUG
	printf("**** init_player, this player(%d) will use port(%s) to accept connections\n", player_engine->index, player_engine->port);
#endif

	/* print required info */
	printf("Connected as player %d\n", player_engine->index);
	close(connfd);
}

/* get neighbors info from master */
void get_neighbors_info(){
	socklen_t addrlen;
	struct sockaddr_storage clientaddr;
	int connfd;
	_neighbor_msg * n_msg=NULL;
	char * buf=NULL;
	int struct_size;

	/* setup listener socket for this player */
	player_engine->lis_socket = setup_listener(NULL, player_engine->port,
			AF_INET, SOCK_STREAM);
	if (player_engine->lis_socket < 0) {
#if PLAYER_DEBUG
		printf("**** init_player, cannot setup listener for player\n");
#endif
		exit(0);
	}

	connfd = accept(player_engine->lis_socket, (struct sockaddr *) &clientaddr, &addrlen);
	if (connfd < 0){
#if PLAYER_DEBUG
		printf("**** get_neighbors_info, invalid connect\n");
#endif
		exit(0);
	}

	struct_size = sizeof(_neighbor_msg);
	buf = (char*)malloc( sizeof(char)*(struct_size+1) );
	n_msg = (_neighbor_msg*)malloc( struct_size+1 );
	if(buf==NULL || n_msg==NULL){
#if PLAYER_DEBUG
		printf("**** get_neighbors_info, memory error1.\n");
#endif
		exit(0);
	}

	memset(buf, 0, sizeof(char)*(struct_size+1) );
	memset(n_msg, 0, struct_size+1);
	recv(connfd, buf, sizeof(char)*struct_size, 0);
	memcpy(n_msg, buf, sizeof(char)*struct_size );


	sprintf(player_engine->left_host, "%s", n_msg->left_host);
	sprintf(player_engine->left_port, "%s", n_msg->left_port);
	sprintf(player_engine->right_host, "%s", n_msg->right_host);
	sprintf(player_engine->right_port, "%s", n_msg->right_port);
	player_engine->left_index = n_msg->left_index;
	player_engine->right_index = n_msg->right_index;

#if PLAYER_DEBUG
		printf("**** get_neighbors_info, %d's left neighbor:%d, %s, %s\n", player_engine->index, player_engine->left_index,
				player_engine->left_host, player_engine->left_port);
		printf("**** get_neighbors_info, %d's right neighbor:%d, %s, %s\n", player_engine->index, player_engine->right_index,
				player_engine->right_host, player_engine->right_port);
#endif

	free(buf);
	free(n_msg);
}

/*void inspect(int * arr, int size){
	int i;
	for(i=0;i<size;i++){
		if( arr[i]!=0 && arr[i]!=1 )
			printf("---------> abnormal element(%d), index:%d\n", *(arr+i), i);
	}
}*/

void listen_msg(){
	socklen_t addrlen;
	struct sockaddr_storage clientaddr;
	int connfd;
	_msg * msg=NULL;
	int neighbor;
	int * old_track=NULL;
	int * new_track=NULL;
	int send_socket;
	int * t_track;

	/* variables to handle large track size */
	int recv_l;
	int to_recv_l;
	int to_recv_block;
	int sent_l;
	int to_send_l;
	int to_send_block;

	while(1){
		connfd = accept(player_engine->lis_socket, (struct sockaddr *) &clientaddr, &addrlen);
		if (connfd < 0){
#if PLAYER_DEBUG
			printf("**** listen_msg, invalid connection\n");
#endif
			continue;
		}
		msg = recv_msg(connfd);
		if(msg==NULL)
			continue;

		/* if this player receives a potato */
		if(msg->type==SEND_P){

			/* if hops is 0(the master sends a 0-hops to this player), report to the master
			if(msg->hops==0 ){
#if PLAYER_DEBUG
				printf("**** listen_msg, master sends hops-0 to %d\n", player_engine->index);
#endif
				msg->receiver=-1;
				msg->sender=player_engine->index;
				msg->type=REPORT_P;
				send_socket = find_socket(player_engine->master_host, player_engine->master_port, AF_INET , SOCK_STREAM);
				send_msg(msg,  send_socket);
				close(send_socket);
				continue;
			} */

#if PLAYER_DEBUG
			printf("**** listen_msg, %d, when receive potato from(%d), sent_time:%d, hops:%d\n", player_engine->index, msg->sender, msg->sent_times, msg->hops);
#endif
			msg->sent_times++;
			msg->hops--;

			/* will receive/create the track, and send it to a neighbor/master */
			if (msg->sender != -1) { /* if not the master initially sends the potato, then recv the actual hops-track, and update it */
				old_track = (int*) malloc( sizeof(int) * (msg->sent_times - 1) + 1 );
				memset(old_track, 0, sizeof(int) * (msg->sent_times - 1) +1 );

				/* !!!!  more work needed to handle large track */
				to_recv_l = sizeof(int) * (msg->sent_times - 1);
				if(to_recv_l>SEND_BLOCK){
					to_recv_block = SEND_BLOCK;
					t_track = old_track;

					while(to_recv_l>0){
						recv_l = recv(connfd, t_track, to_recv_block, 0);
						t_track+=( recv_l / (sizeof(int)) );
						to_recv_l-=recv_l;
						if( to_recv_l<SEND_BLOCK)
							to_recv_block = to_recv_l;
					}

#if PLAYER_DEBUG
					printf("**** listen_msg, after received large data, final element:%d.\n", *(old_track+msg->sent_times - 2) );
#endif
				}else{
					recv(connfd, old_track, sizeof(int) * (msg->sent_times - 1), 0);
				}

				/* copy to new track */
				new_track = (int*) malloc( sizeof(int) * msg->sent_times + 1 );
				memset( new_track, 0, sizeof(int) * msg->sent_times + 1 );
				memcpy(new_track, old_track, sizeof(int) * (msg->sent_times - 1) );
				*(new_track+msg->sent_times - 1) = player_engine->index;  /* append this player's index */
				free(old_track);
#if PLAYER_DEBUG
				printf("**** Receive potato from the %d, append self index:%d\n", msg->sender, *(new_track+msg->sent_times - 1) );
#endif
			} else { /* else(this player is the first one who receives the potato), append first element to the track */
				new_track = (int*) malloc(sizeof(int) + 1);
				memset(new_track, 0, sizeof(int) + 1);
				*(new_track) = player_engine->index;  /* append this player's index */
#if PLAYER_DEBUG
				printf("**** Initially receive potato from the master, append self index:%d\n", player_engine->index);
#endif
			}

			msg->sender=player_engine->index;
			/* send msg/track to one of its neighbors/master */
			if (msg->hops != 0) {
				neighbor = generate_random();
				if (neighbor == 0) { /* 0: send to left */
					send_socket = find_socket(player_engine->left_host,
							player_engine->left_port, AF_INET, SOCK_STREAM);
					msg->receiver = player_engine->left_index;
#if PLAYER_DEBUG
					/* printf("**** Next potato receiver(%s, %s)\n", player_engine->left_host, player_engine->left_port); */
#endif
				} else { /* 1: send to right */
					send_socket = find_socket(player_engine->right_host,
							player_engine->right_port, AF_INET, SOCK_STREAM);
					msg->receiver = player_engine->right_index;
#if PLAYER_DEBUG
					/* printf("**** Next potato receiver(%s, %s)\n", player_engine->right_host, player_engine->right_port); */
#endif
				}
				msg->type = SEND_P;

				printf("Sending potato to %d\n", msg->receiver);

			} else {
				send_socket = find_socket(player_engine->master_host,
						player_engine->master_port, AF_INET, SOCK_STREAM);
				msg->type = REPORT_P;

				printf("I'm it\n");
			}
			send_msg(msg, send_socket); /* send notification msg */

			/* !!!!  more work needed to handle large track */
			to_send_l = sizeof(int) * (msg->sent_times);
			if(to_send_l>SEND_BLOCK){
				to_send_block = SEND_BLOCK;
				t_track = new_track;

				while( to_send_l>0 ){
					/* printf("---->test2, to_send_block:%d, to_send_l:%d\n", to_send_block, to_send_l); */
					sent_l = send(send_socket, t_track, to_send_block, 0);
					/* printf("---->test3, sent_l:%d\n", sent_l); */
					t_track+=( sent_l / (sizeof(int)) );
					/* printf("---->test4, sent_l / (sizeof(int)):%d\n", sent_l / (sizeof(int)) ); */
					to_send_l-=sent_l;
					if( to_send_l<SEND_BLOCK)
						to_send_block = to_send_l;
				}

#if PLAYER_DEBUG
				printf("**** listen_msg, after sent large data, final element:%d.\n", *(new_track+msg->sent_times - 1) );
#endif
			}else{
				send(send_socket, new_track, sizeof(int) * msg->sent_times, 0); /* send new track */
			}

			close(send_socket);
			/* end if SEND_P */
		}else if(msg->type==EXIT){  /* if master requires to shutdown */
#if PLAYER_DEBUG
			printf("**** listen_msg, master requires to shutdown.\n");
#endif
			close(connfd);
			close(player_engine->lis_socket);
			exit(0);
		}

		close(connfd);
		free(msg);
	}/* end while*/

}  /* end of listen_msg */

int main(int argc, char * argv[]) {
	if( argc!=3 ){
#if PLAYER_DEBUG
		printf("**** USAGE: player <master-machine-name> <port-number>\n");
#endif
		exit(0);
	}

	player_engine = (_player_core*)malloc( sizeof(_player_core) );
	if(player_engine==NULL){
#if PLAYER_DEBUG
		printf("**** main, memory error1.\n");
#endif
		exit(0);
	}

	player_engine->index=-1;
	player_engine->left_index=-1;
	player_engine->right_index=-1;
	player_engine->lis_socket=-1;
	memset( player_engine->host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( player_engine->port, 0, sizeof(char)*(MY_MAXSERV+1) );
	memset( player_engine->master_host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( player_engine->master_port, 0, sizeof(char)*(MY_MAXSERV+1) );
	memset( player_engine->left_host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( player_engine->left_port, 0, sizeof(char)*(MY_MAXSERV+1) );
	memset( player_engine->right_host, 0, sizeof(char)*(MY_MAXHOST+1) );
	memset( player_engine->right_port, 0, sizeof(char)*(MY_MAXSERV+1) );

	sprintf(player_engine->master_host, "%s", argv[1]);
	sprintf(player_engine->master_port, "%s", argv[2]);

	player_engine->base_port= atoi(argv[2]);


#if PLAYER_DEBUG
	printf("**** main, to connect to master(%s,%s).\n", player_engine->master_host, player_engine->master_port);
#endif

	/* connect to master and get its own index, also setup its own listener */
	init_player();

	/* listen to master, get neighbors' info */
	get_neighbors_info();

	/* listen for possible potato and master's shutdown msg */
	listen_msg();

}  /* end of main */
