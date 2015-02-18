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

#include "config.h"
#include "queue.h"
#include "list.h"

#define CONFIG "/etc/mpdproxy.conf"
#define MAX_LEN 4096
#define BACKLOG 32

#define TRUE 1
#define FALSE 0

static void *th_sock_client(void*);
static void *th_sock_server(void*);
static void *th_cleanup_client(void*);
static void *th_cleanup_server(void*);

typedef struct connection_t {
	int sock_srv;
	int sock_cli;
	pthread_t th_client;
	pthread_t th_server;
	int srv_hup;
} connection_t;

config_t config;

static struct option long_options[] = {
	{"config",	required_argument,	NULL, 'c'},
	{0}
};

static void
die(const char *comp, const char *msg)
{
	fprintf(stderr, "E (%d): %s: %s\n", errno, comp, msg);

	struct queue *q_th, *q_tmp;

	struct queue threads;
	INIT_LIST_HEAD(&threads.list);

	pthread_mutex_lock(&q_mutex);
	list_for_each_entry_safe(q_th, q_tmp, &q.list, list){
		printf("[main] Adding %d\n", (int) q_th->th_id);
		struct queue *q_new = malloc(sizeof(struct queue));
		q_new->th_id = q_th->th_id;
		INIT_LIST_HEAD(&q_new->list);

		list_add(&q_new->list, &threads.list);
		//~ pthread_cancel(q_th->th_id);
		//~ pthread_join(q_th->th_id, NULL);
	}
	pthread_mutex_unlock(&q_mutex);

	list_for_each_entry_safe(q_th, q_tmp, &threads.list, list){
		printf("[main] Cancelling %d\n", (int) q_th->th_id);
		pthread_cancel(q_th->th_id);
		pthread_join(q_th->th_id, NULL);
		list_del(&q_th->list);
		free(q_th);
	}

	queue_destroy();
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

	struct sockaddr_in addr_srv;

	char *config_f = CONFIG;

	int option_index;
	int arg = getopt_long(argc, argv, "c:", long_options, &option_index);
	if(arg != -1)
		config_f = strdup(optarg);

	// Config file
	config_init(&config);
	FILE *fp;
	if((fp = fopen(config_f, "r")) == NULL)
		die("fopen", "cannot open configuration file: no such file or directory\n");

	if(config_read_file(&config, fp) == CONFIG_FAILURE)
		die("config_read_file", "error reading configuration file\n");

	// Server address
	bzero(&addr_srv, sizeof(struct sockaddr));
	addr_srv.sin_family = AF_INET;
	printf("MPD Server: %s:%d\n", config.host_s, config.port_s);
	if(inet_pton(AF_INET, config.host_s, &(addr_srv.sin_addr)) <= 0)
		die("inet_pton", strerror(errno));
	addr_srv.sin_port = htons(config.port_s);

	printf("Listening on %s:%d\n", config.host_p, config.port_p);

	fclose(fp);
	free(config_f);

	/**
	 * Server
	 * 
	 * */
	int sock_srv, sock_cli;
	struct sockaddr_in server, client;
	socklen_t sin_size = sizeof(struct sockaddr_in);

	sock_srv = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_srv < 0)
		die("socket", strerror(errno));

	int optval = 1;
	if(setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0)
		die("setsockopt", strerror(errno));

	bzero((char *) &server, sizeof(server));
	server.sin_family = AF_INET;
	inet_aton(config.host_p, &server.sin_addr);
	server.sin_port = htons(config.port_p);

	if(bind(sock_srv, (struct sockaddr *) &server, sizeof(server)) < 0){
		close(sock_srv);
		die("bind", strerror(errno));
	}

	if(listen(sock_srv, BACKLOG) < 0){
		close(sock_srv);
		die("listen", strerror(errno));
	};

	queue_init();

	while((sock_cli = accept(sock_srv, (struct sockaddr *) &client, &sin_size))){
		connection_t *conn = malloc(sizeof(connection_t));
		conn->sock_cli = sock_cli;
		conn->srv_hup = TRUE;

		if((conn->sock_srv = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("socket", strerror(errno));
		}

		if(connect(conn->sock_srv, (struct sockaddr *) &addr_srv, sizeof(struct sockaddr)) < 0){
			close(conn->sock_srv);
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("connect", strerror(errno));
		}

		if(pthread_create(&(conn->th_client), NULL, &th_sock_client, conn)){
			close(conn->sock_srv);
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_client", strerror(errno));
		}

		if(pthread_create(&(conn->th_server), NULL, &th_sock_server, conn)){
			close(conn->sock_srv);
			free(conn);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_server", strerror(errno));
		}
	}
	if(sock_cli < 0)
		die("accept", strerror(errno));

	/**
	 * Unreachable statements
	 * 
	 * */
	close(sock_srv);

	queue_destroy();
	config_destroy(&config);

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
	int bytes;

	printf("[client] (%d) Connection established\n", (int) pthread_self());

	while((bytes = recv(conn->sock_cli, &buffer, MAX_LEN, 0)) > 0){
		printf("[client] Received %d bytes\n", bytes);
		if((bytes = send(conn->sock_srv, &buffer, bytes, 0)) <= 0)
			break;
		printf("[client] Sent %d bytes to server\n", bytes);
	}
	printf("[client] Connection closed: %s\n", strerror(errno));

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
	int bytes;

	printf("[server] (%d) Connection established\n", (int) pthread_self());

	while((bytes = recv(conn->sock_srv, &buffer, MAX_LEN, 0)) > 0){
		printf("[server] Received %d bytes\n", bytes);
		if((bytes = send(conn->sock_cli, &buffer, bytes, 0)) <= 0)
			break;
		printf("[server] Sent %d bytes to client\n", bytes);
	}
	printf("[server] Connection closed: %s\n", strerror(errno));

	conn->srv_hup = TRUE;

	pthread_cleanup_pop(TRUE);
	pthread_exit(NULL);
}

static void *
th_cleanup_client(void *connection)
{
	printf("[client] Cleaning up\n");
	connection_t *conn = (connection_t*) connection;

	close(conn->sock_cli);
	if(!conn->srv_hup){
		printf("[client] Cancelling %d\n", (int) conn->th_server);
		pthread_cancel(conn->th_server);
		pthread_join(conn->th_server, NULL);
		free(connection);
	}

	queue_rem(pthread_self());
	printf("[client] Bye bye\n");
	return NULL;
	//~ pthread_exit(NULL);
}

static void *
th_cleanup_server(void *connection)
{
	printf("[server] Cleaning up\n");
	connection_t *conn = (connection_t*) connection;

	close(conn->sock_srv);
	if(conn->srv_hup){
		printf("[server] Cancelling %d\n", (int) conn->th_client);
		pthread_cancel(conn->th_client);
		pthread_join(conn->th_client, NULL);
		free(connection);
	}

	queue_rem(pthread_self());

	printf("[server] Bye bye\n");
	return NULL;
	//~ pthread_exit(NULL);
}
