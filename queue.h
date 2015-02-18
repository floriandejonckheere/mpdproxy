/*
 * queue.h - thread queue
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#include <pthread.h>

#include "list.h"

#ifndef QUEUE_H
#define QUEUE_H

struct queue {
	struct list_head list;
	pthread_t th_id;
};

struct queue q;
pthread_mutex_t q_mutex;

void queue_print();
void queue_init();
void queue_destroy();
void queue_ins(pthread_t th_id);
void queue_rem(pthread_t th_id);

#endif
