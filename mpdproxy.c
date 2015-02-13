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
#include <sys/types.h>
#include <netinet/in.h>

#include "config.h"

#define CONFIG "/etc/mpdproxy.conf"
#define MAX_LEN 4096
#define BACKLOG 5

void *th_handler(void*);

struct sockaddr_in addr_srv;

static struct option long_options[] = {
	{"config",	required_argument,	NULL, 'c'},
	{0}
};

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
	pthread_t thread_id;

	sock_srv = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_srv < 0)
		die("socket", strerror(errno));

	int optval = 1;
	if(setsockopt(sock_srv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0)
		die("setsockopt", strerror(errno));

	//~ bzero((char *) &server, sizeof(server));
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

	while((sock_cli = accept(sock_srv, (struct sockaddr *) &client, &sin_size)) ){
		if(pthread_create(&thread_id, NULL, th_handler, (void*) &sock_cli) < 0)
			die("pthread_create", strerror(errno));
	}
	if(sock_cli < 0)
		die("accept", strerror(errno));

	close(sock_srv);

	config_destroy(&config);
	exit(EXIT_SUCCESS);
}

// Thread handler
void *th_handler(void *sock){
	char buffer[MAX_LEN];
	int bytes;

	int id = (int) pthread_self();
	printf("[%d] Connected\n", id);

	int sock_srv, sock_cli = * (int*) sock;
	if((sock_srv = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		close(sock_cli);
		die("socket", strerror(errno));
	}

	if(connect(sock_srv, (struct sockaddr *) &addr_srv, sizeof(struct sockaddr)) < 0){
		close(sock_cli);
		close(sock_srv);
		die("connect", strerror(errno));
	}


	// Initial "OK MPD" message
	if((bytes = recv(sock_srv, buffer, MAX_LEN, 0)) < 0){
		close(sock_cli);
		close(sock_srv);
		th_die("recv", strerror(errno));
	}
	buffer[bytes] = '\0';
	if(send(sock_cli, buffer, bytes, 0) < 0){
		close(sock_cli);
		close(sock_srv);
		th_die("send_cli", strerror(errno));
	}

	// Receive from the client
	while((bytes = recv(sock_cli, buffer, MAX_LEN - 1, 0)) > 0){
		buffer[bytes] = '\0';
		printf("[%d] Received %d bytes from client\n", id, bytes);

		// Send to the server
		int bytes_s;
		if((bytes_s = send(sock_srv, buffer, bytes, 0)) < 0){
			close(sock_cli);
			close(sock_srv);
			th_die("send_srv", strerror(errno));
		}
		printf("[%d] Sent %d bytes to server\n", id, bytes_s);

		// End of client command
		if(strstr(&buffer[bytes - 1], "\n") != NULL){
			int bytes_r, bytes_s;
			while((bytes_r = recv(sock_srv, buffer, MAX_LEN - 1, 0)) > 0){
				buffer[bytes_r] = '\0';
				printf("[%d] Received %d bytes from server\n", id, bytes_r);

				// Reply to client
				if((bytes_s = send(sock_cli, buffer, bytes_r, 0)) < 0){
					close(sock_cli);
					close(sock_srv);
					th_die("send_cli", strerror(errno));
				}
				printf("[%d] Sent %d bytes to client\n", id, bytes_s);

				// End of server reply
				if(strstr(buffer, "\nOK") != NULL || strstr(buffer, "\nACK") != NULL)
					break;
			}
			if(bytes_r < 0){
				close(sock_cli);
				close(sock_srv);
				th_die("recv_srv", strerror(errno));
			}
		}
	}
	if(bytes < 0){
		close(sock_cli);
		close(sock_srv);
		th_die("recv_cli", strerror(errno));
	}

	close(sock_cli);
	close(sock_srv);

	printf("[%d] Closed connection\n", id);

	return NULL;
}
