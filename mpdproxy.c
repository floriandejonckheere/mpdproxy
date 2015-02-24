/*
 * mpdproxy.c - MPD proxy server
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "config.h"
#include "queue.h"
#include "list.h"

#define CONFIG "/etc/mpdproxy.conf"
#define MAX_LEN 4096

#define TRUE 1
#define FALSE 0

static void *th_sock_client(void*);
static void *th_sock_server(void*);
static void *th_cleanup_client(void*);
static void *th_cleanup_server(void*);

typedef struct connection_t {
	int sock_prx;
	int sock_cli;
	pthread_t th_client;
	pthread_t th_server;
	int srv_hup;
} connection_t;

config_t config;
struct addrinfo *addr_prx;

FILE *errstr;

static struct option long_options[] = {
	{"config",	required_argument,	NULL,	'c'},
	{"log",		required_argument,	NULL,	'l'},
	{"ipv4",	no_argument,		NULL,	'4'},
	{"ipv6",	no_argument,		NULL,	'6'},
	{0, 0, 0, 0}
};

static void
print(const char *comp, const char *msg)
{
	fprintf(errstr, "[%s]: %s (%d)\n", comp, msg, errno);
	fflush(errstr);
}

static void
die(const char *comp, const char *msg)
{
	print(comp, msg);
	fclose(errstr);

	if(addr_prx) freeaddrinfo(addr_prx);

	if(q != NULL){
		struct queue *q_th, *q_tmp;

		struct queue threads;
		INIT_LIST_HEAD(&threads.list);

		pthread_mutex_lock(&q_mutex);
		list_for_each_entry_safe(q_th, q_tmp, &q->list, list){
			struct queue *q_new = malloc(sizeof(struct queue));
			q_new->th_id = q_th->th_id;
			INIT_LIST_HEAD(&q_new->list);

			list_add(&q_new->list, &threads.list);
		}
		pthread_mutex_unlock(&q_mutex);

		list_for_each_entry_safe(q_th, q_tmp, &threads.list, list){
			pthread_cancel(q_th->th_id);
			pthread_join(q_th->th_id, NULL);
			list_del(&q_th->list);
			free(q_th);
		}

		queue_destroy();
	}
	config_destroy(&config);

	pthread_exit(&errno);
}

static void
sig_handler(int sig)
{
	die(strsignal(sig), "Caught signal, exiting");
}

int
main(int argc, char** argv)
{
	// Signal handler
	signal(SIGINT, sig_handler);

	char *config_f = NULL;
	int ipv4 = FALSE;
	int ipv6 = FALSE;

	int opt_idx, c;

	errstr = stderr;

	while((c = getopt_long(argc, argv, "c:l:46", long_options, &opt_idx)) != -1){
		switch(c){
			case 0:
				if(long_options[opt_idx].flag != 0)
					break;
				fprintf(stderr, "option %s", long_options[opt_idx].name);
				if(optarg)
					fprintf(stderr, " with arg %s", optarg);
				fprintf(stderr, "\n");
				break;
			case 'c':
				config_f = strdup(optarg);
				break;
			case 'l':
				if((errstr = fopen(optarg, "a")) == NULL){
					errstr = stderr;
					die("open_log", strerror(errno));
				}
				break;
			case '4':
				ipv4 = TRUE;
				break;
			case '6':
				ipv6 = TRUE;
				break;
			default:
				abort();
		}
	}

	if(!config_f)
		config_f = strdup(CONFIG);

	if(!ipv4 && !ipv6)
		ipv4 = ipv6 = TRUE;

	/**
	 * Configuration
	 * 
	 * */
	config_init(&config);
	FILE *fp;
	if((fp = fopen(config_f, "r")) == NULL)
		die("fopen", "cannot open configuration file: no such file or directory");

	if(config_read_file(&config, fp) == CONFIG_FAILURE)
		die("config_read_file", "error reading configuration file");

	fclose(fp);
	free(config_f);

	/**
	 * Networking
	 * 
	 * */
	
	int sock_srv, sock_cli, err, port;
	struct addrinfo hints, *addr_srv, *p;

	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;

	if(ipv4 && !ipv6){
		hints.ai_family = AF_INET;
	} else if(!ipv4 && ipv6){
		hints.ai_family = AF_INET6;
	} else hints.ai_family = AF_UNSPEC;

	// Proxy
	if((err = getaddrinfo(config.host_prx, config.port_prx, &hints, &addr_prx)))
		die("getaddr_proxy", gai_strerror(err));

	hints.ai_flags = AI_PASSIVE;

	// Server
	if((err = getaddrinfo(config.host_srv, config.port_srv, &hints, &addr_srv)))
		die("getaddrinfo", gai_strerror(err));

	for(p = addr_srv; p != NULL; p = p->ai_next){
		if((sock_srv = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		int optval = 1;
		if(setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0)
			die("setsockopt", strerror(errno));

		if(bind(sock_srv, p->ai_addr, p->ai_addrlen) == -1){
			close(sock_srv);
			continue;
		}

		break;
	}

	if(p == NULL){
		freeaddrinfo(addr_srv);
		die("bind_srv", strerror(errno));
	}

	if(ipv4){
		inet_ntop(AF_INET, &((struct sockaddr_in*) p->ai_addr)->sin_addr, s, sizeof(s));
		port = htons(((struct sockaddr_in*) p->ai_addr)->sin_port);
		fprintf(errstr, "[main] Listening on %s:%d\n", s, port);
	}
	if(ipv6){
		inet_ntop(AF_INET6, &((struct sockaddr_in6*) p->ai_addr)->sin6_addr, s, sizeof(s));
		port = htons(((struct sockaddr_in6*) p->ai_addr)->sin6_port);
		fprintf(errstr, "[main] Listening on %s:%d\n", s, port);
	}

	freeaddrinfo(addr_srv);

	if(listen(sock_srv, SOMAXCONN) < 0){
		close(sock_srv);
		die("listen", strerror(errno));
	};

	queue_init();

	while((sock_cli = accept(sock_srv, NULL, NULL))){
		connection_t *conn = malloc(sizeof(connection_t));
		conn->sock_cli = sock_cli;
		conn->srv_hup = TRUE;

		for(p = addr_prx; p != NULL; p = p->ai_next){
			if((conn->sock_prx = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
				continue;

			if(connect(conn->sock_prx, p->ai_addr, p->ai_addrlen)){
				close(conn->sock_prx);
				continue;
			}

			break;
		}

		if(p == NULL){
			print("bind_prx", strerror(errno));
			close(sock_cli);
			free(conn);
			continue;
		}

		if(p->ai_family == AF_INET){
			inet_ntop(AF_INET, &((struct sockaddr_in*) p->ai_addr)->sin_addr, s, sizeof(s));
			port = htons(((struct sockaddr_in*) p->ai_addr)->sin_port);
		} else {
			inet_ntop(AF_INET6, &((struct sockaddr_in6*) p->ai_addr)->sin6_addr, s, sizeof(s));
			port = htons(((struct sockaddr_in6*) p->ai_addr)->sin6_port);
		}
		fprintf(errstr, "[main] Proxying requests to %s:%d\n", s, port);

		if(pthread_create(&(conn->th_client), NULL, &th_sock_client, conn)){
			close(conn->sock_prx);
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_client", strerror(errno));
		}

		if(pthread_create(&(conn->th_server), NULL, &th_sock_server, conn)){
			close(conn->sock_prx);
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_server", strerror(errno));
		}
	}
	if(sock_cli < 0)
		die("accept", strerror(errno));

	pthread_exit(EXIT_SUCCESS);
}

/**
 * Threads
 * 
 * */
static void *
th_sock_client(void *connection)
{
	queue_ins(pthread_self());

	connection_t *conn = (connection_t*) connection;

	pthread_cleanup_push(&th_cleanup_client, connection);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	char buffer[MAX_LEN];
	ssize_t bytes;

	while((bytes = recv(conn->sock_cli, &buffer, MAX_LEN, 0)) > 0){
		if((bytes = send(conn->sock_prx, &buffer, (size_t) bytes, 0)) <= 0)
			break;
	}

	conn->srv_hup = FALSE;

	pthread_cleanup_pop(TRUE);
	pthread_exit(NULL);
}

static void *
th_sock_server(void *connection)
{
	queue_ins(pthread_self());

	connection_t *conn = (connection_t*) connection;

	pthread_cleanup_push(&th_cleanup_server, connection);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	char buffer[MAX_LEN];
	ssize_t bytes;

	while((bytes = recv(conn->sock_prx, &buffer, MAX_LEN, 0)) > 0){
		if((bytes = send(conn->sock_cli, &buffer, (size_t) bytes, 0)) <= 0)
			break;
	}

	conn->srv_hup = TRUE;

	pthread_cleanup_pop(TRUE);
	pthread_exit(NULL);
}

static void *
th_cleanup_client(void *connection)
{
	connection_t *conn = (connection_t*) connection;

	close(conn->sock_cli);
	if(!conn->srv_hup){
		pthread_cancel(conn->th_server);
		pthread_join(conn->th_server, NULL);
		free(connection);
	}

	queue_rem(pthread_self());

	// Call pthread_detach to let pthread reuse (and ultimately free) alloc'ed resources
	pthread_detach(pthread_self());
	return NULL;
}

static void *
th_cleanup_server(void *connection)
{
	connection_t *conn = (connection_t*) connection;

	close(conn->sock_prx);
	if(conn->srv_hup){
		pthread_cancel(conn->th_client);
		pthread_join(conn->th_client, NULL);
		free(connection);
	}

	queue_rem(pthread_self());

	// Call pthread_detach to let pthread reuse (and ultimately free) alloc'ed resources
	pthread_detach(pthread_self());
	return NULL;
}
