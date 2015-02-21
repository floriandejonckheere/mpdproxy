/*
 * queue.c - thread queue
 * 
 * Florian Dejonckheere <florian@floriandejonckheere.be>
 * 
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <search.h>
#include <pthread.h>

#include "queue.h"
#include "list.h"

struct queue q;
pthread_mutex_t mutex;

static void
die(const char *comp, const char *msg)
{
	fprintf(stderr, "E (%d): %s: %s\n", errno, comp, msg);

	if(errno) exit(errno);
	else exit(EXIT_FAILURE);
}

void
queue_init()
{
	if(pthread_mutex_init(&q_mutex, NULL))
		die("pthread_mutex_init", strerror(errno));

	INIT_LIST_HEAD(&q.list);
}

void
queue_destroy()
{
	struct queue *q_th, *q_tmp;

	pthread_mutex_lock(&q_mutex);
	list_for_each_entry_safe(q_th, q_tmp, &q.list, list){
		list_del(&q_th->list);
		free(q_th);
	}
	pthread_mutex_unlock(&q_mutex);

	if(pthread_mutex_destroy(&q_mutex))
		die("pthread_mutex_destroy", strerror(errno));
}

void
queue_ins(pthread_t th_id)
{
	struct queue *q_new = malloc(sizeof(struct queue));
	q_new->th_id = th_id;
	INIT_LIST_HEAD(&q_new->list);

	pthread_mutex_lock(&q_mutex);
	list_add(&q_new->list, &q.list);
	pthread_mutex_unlock(&q_mutex);
}

void
queue_rem(pthread_t th_id)
{
	struct queue *q_th, *q_tmp;

	pthread_mutex_lock(&q_mutex);
	list_for_each_entry_safe(q_th, q_tmp, &q.list, list){
		if(pthread_equal(q_th->th_id, th_id)){
			list_del(&q_th->list);
			free(q_th);
		}
	}
	pthread_mutex_unlock(&q_mutex);
}
