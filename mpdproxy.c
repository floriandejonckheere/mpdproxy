/*
 * mpdproxy.c - MPD proxy server
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "config.h"

#define CONFIG "/etc/mpdproxy.conf"

static struct option long_options[] = {
	{"config",	required_argument,	NULL, 'c'},
	{0}
};

void die(const char* msg){
	fprintf(stderr, "E: %s\n", msg);
	exit(EXIT_FAILURE);
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

	config_t config;
	config_init(&config);
	FILE *fp;
	if((fp = fopen(config_f, "r")) == NULL)
		die("cannot open configuration file: no such file or directory\n");

	if(config_read_file(&config, fp) == CONFIG_FAILURE)
		die("error reading configuration file\n");

	printf("Listening on %s:%d\n", config.host_p, config.port_p);

	fclose(fp);
	config_destroy(&config);
	free(config_f);

	exit(EXIT_SUCCESS);
}
