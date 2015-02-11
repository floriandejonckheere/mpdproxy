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
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "config.h"

#define CONFIG "/etc/mpdproxy.conf"
#define MAX_LEN 4096
#define BACKLOG 5

void *th_handler(void*);

static struct option long_options[] = {
	{"config",	required_argument,	NULL, 'c'},
	{0}
};

void die(const char* msg){
	fprintf(stderr, "E: %s\n", msg);

	if(errno) exit(errno);
	else exit(EXIT_FAILURE);
}

void print_usage(char* progname){
	fprintf(stderr, "Usage: %s [ -c | --config=CONFIG ]\n", progname);
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv){
	char *config_f = CONFIG;

	int option_index;
	int arg = getopt_long(argc, argv, "c:", long_options, &option_index);
	if(arg != -1)
		config_f = strdup(optarg);

	// Config file
	config_t config;
	config_init(&config);
	FILE *fp;
	if((fp = fopen(config_f, "r")) == NULL)
		die("cannot open configuration file: no such file or directory\n");

	if(config_read_file(&config, fp) == CONFIG_FAILURE)
		die("error reading configuration file\n");

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
	pthread_t thread_id;

	sock_srv = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_srv < 0)
		die(strerror(errno));

	//~ int optval = 1;
	//~ if(setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0)
		//~ die(strerror(errno));

	//~ bzero((char *) &server, sizeof(server));
	server.sin_family = AF_INET;
	inet_aton(config.host_p, &server.sin_addr);
	server.sin_port = htons(config.port_p);

	if(bind(sock_srv, (struct sockaddr *) &server, sizeof(server)) < 0){
		close(sock_srv);
		die(strerror(errno));
	}

	if(listen(sock_srv, BACKLOG) < 0){
		close(sock_srv);
		die(strerror(errno));
	};

	while((sock_cli = accept(sock_srv, (struct sockaddr *) &client, &sin_size)) ){
		if(pthread_create(&thread_id, NULL, th_handler, (void*) &sock_cli) < 0)
			die(strerror(errno));
	}
	if(sock_cli < 0)
		die(strerror(errno));

	close(sock_srv);

	config_destroy(&config);
	exit(EXIT_SUCCESS);
}

// Thread handler
void *th_handler(void *sock_cli){
	int sock = * (int*) sock_cli;
	char buffer[MAX_LEN];
	int bytes;

	while((bytes = recv(sock, buffer, MAX_LEN, 0)) > 0){
		buffer[bytes] = '\0';
		printf("Received: %s\n", buffer);
	}
	close(sock);
	return NULL;
}
