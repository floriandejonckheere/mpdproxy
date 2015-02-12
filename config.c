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

void config_init(config_t *config){
	memset(config, 0, sizeof(config_t));
	config->host_s = calloc(MAX_LEN, sizeof(char));
	config->pass_s = calloc(MAX_LEN, sizeof(char));
	config->host_p = calloc(MAX_LEN, sizeof(char));
}

void config_destroy(config_t *config){
	free(config->host_s);
	free(config->pass_s);
	free(config->host_p);
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
				strncpy(config->host_s, value, MAX_LEN);
				config->host_s[MAX_LEN - 1] = '\0';
			} else if(strncmp(token, "Port", sizeof("Port")) == 0){
				config->port_s = atoi(value);
			} else if(strncmp(token, "Password", sizeof("Password")) == 0){
				strncpy(config->pass_s, value, MAX_LEN);
				config->pass_s[MAX_LEN - 1] = '\0';
			} else if(strncmp(token, "Listen", sizeof("Listen")) == 0){
				strncpy(config->host_p, value, MAX_LEN);
				config->host_p[MAX_LEN - 1] = '\0';
			} else {
				config->port_p = atoi(value);
			}

			token = strtok(NULL, " ");
		}
	}

	free(line);

	if(strcmp(config->host_p, "") == 0){
		strcpy(config->host_p, "0.0.0.0");
	}
	if(config->port_p == 0)
		config->port_p = 6600;

	return CONFIG_SUCCESS;
}
