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

#define CONFIG "/etc/mpdproxy.conf"
#define MAX_LEN 4096
#define BACKLOG 32

void *th_sock_client(void*);
void *th_sock_server(void*);

typedef struct connection_t {
	int sock_srv;
	int sock_cli;
} connection_t;

config_t config;

static struct option long_options[] = {
	{"config",	required_argument,	NULL, 'c'},
	{0}
};

void sig_handler(int sig){
	printf("Caught signal %d, exiting\n", sig);
	config_destroy(&config);
	exit(sig);
}

void die(const char *comp, const char *msg){
	fprintf(stderr, "E (%d): %s: %s\n", errno, comp, msg);

	if(errno) exit(errno);
	else exit(EXIT_FAILURE);
}

void th_die(const char *comp, const char *msg){
	fprintf(stderr, "E (%d): %s: %s\n", errno, comp, msg);

	if(errno) pthread_exit(&errno);
	else pthread_exit(NULL);
}

int main(int argc, char** argv){
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
	pthread_t th_client, th_server;

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

	while((sock_cli = accept(sock_srv, (struct sockaddr *) &client, &sin_size))){
		connection_t conn;
		conn.sock_cli = sock_cli;

		if((conn.sock_srv = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			close(sock_cli);
			close(sock_srv);
			die("socket", strerror(errno));
		}

		if(connect(conn.sock_srv, (struct sockaddr *) &addr_srv, sizeof(struct sockaddr)) < 0){
			close(conn.sock_srv);
			close(sock_cli);
			close(sock_srv);
			die("connect", strerror(errno));
		}
		
		if(pthread_create(&th_client, NULL, th_sock_client, (void*) &conn) < 0){
			close(conn.sock_srv);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_client", strerror(errno));
		}

		if(pthread_create(&th_server, NULL, th_sock_server, (void*) &conn) < 0){
			close(conn.sock_srv);
			close(sock_cli);
			close(sock_srv);
			die("pthread_create_server", strerror(errno));
		}
	}
	if(sock_cli < 0)
		die("accept", strerror(errno));

	close(sock_srv);

	config_destroy(&config);
	exit(EXIT_SUCCESS);
}

/**
 * Threads
 * 
 * */
void *th_sock_client(void *connection){
	connection_t conn = * (connection_t*) connection;

	char buffer[MAX_LEN];
	int bytes;

	while((bytes = recv(conn.sock_cli, &buffer, MAX_LEN, 0)) > 0){
		if((bytes = send(conn.sock_srv, &buffer, bytes, 0)) < 0)
			break;
	}
	close(conn.sock_cli);

	pthread_exit(NULL);
}

void *th_sock_server(void *connection){
	connection_t conn = * (connection_t*) connection;

	char buffer[MAX_LEN];
	int bytes;

	while((bytes = recv(conn.sock_srv, &buffer, MAX_LEN, 0)) > 0){
		if((bytes = send(conn.sock_cli, &buffer, bytes, 0)) < 0)
			break;
	}
	close(conn.sock_cli);

	pthread_exit(NULL);
}
