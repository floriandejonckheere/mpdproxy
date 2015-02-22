/*
 * config.h - configuration file parser
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_SUCCESS 0
#define CONFIG_FAILURE 1

typedef struct config_t {
	char *host_srv;
	char *port_srv;

	char *host_prx;
	char *port_prx;
} config_t;

void config_init(config_t *config);
void config_destroy(config_t *config);

int config_read_file(config_t *config, FILE *fp);

#endif
