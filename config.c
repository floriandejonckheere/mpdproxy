/*
 * config.c - configuration file parser
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define MAX_LEN 255

void config_init(config_t *config)
{
	memset(config, 0, sizeof(config_t));
	config->host_srv = calloc(MAX_LEN, sizeof(char));
	config->port_srv = calloc(MAX_LEN, sizeof(char));
	config->host_prx = calloc(MAX_LEN, sizeof(char));
	config->port_prx = calloc(MAX_LEN, sizeof(char));
}

void config_destroy(config_t *config)
{
	free(config->host_srv);
	free(config->port_srv);
	free(config->host_prx);
	free(config->port_prx);
}

int config_read_file(config_t *config, FILE *fp){
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while((read = getline(&line, &len, fp)) != -1){
		// Remove trailing newline
		if(read > 0)
			line[read - 1] = '\0';

		char *token = strtok(line, " ");
		while(token && strncmp(token, "#", 1) != 0 && strncmp(token, "\n", 1) != 0){
			char *value = strtok(NULL, " ");

			if(strncmp(token, "Host", sizeof("Host")) == 0){
				strncpy(config->host_prx, value, MAX_LEN);
				config->host_prx[MAX_LEN - 1] = '\0';
			} else if(strncmp(token, "Port", sizeof("Port")) == 0){
				strncpy(config->port_prx, value, MAX_LEN);
				config->port_prx[MAX_LEN - 1] = '\0';
			} else if(strncmp(token, "Listen", sizeof("Listen")) == 0){
				strncpy(config->host_srv, value, MAX_LEN);
				config->host_srv[MAX_LEN - 1] = '\0';
			} else if(strncmp(token, "ProxyPort", sizeof("ProxyPort")) == 0){
				strncpy(config->port_srv, value, MAX_LEN);
				config->port_srv[MAX_LEN - 1] = '\0';
			} else {
				fprintf(stderr, "[config] Unknown key: %s\n", token);
			}

			token = strtok(NULL, " ");
		}
	}

	free(line);

	if(strcmp(config->host_prx, "") == 0){
		strcpy(config->host_prx, "0.0.0.0");
	}
	if(config->port_prx == 0)
		config->port_prx = "6600";

	return CONFIG_SUCCESS;
}
