#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include "log.h"
#include "pthread_worker.h"

 void *locked_worker_callback(struct pthread_worker_worker_obj_t *obj, struct dllist *list)
{
	logg_err("call");
	return NULL;
}

void *unlocked_worker_callback(struct pthread_worker_worker_obj_t *obj, struct dllist *list)
{
	logg_err("call");
	return NULL;
}

int cond_list_insert_callback(struct pthread_worker_main_obj_t *obj, struct dllist *list)
{	
	logg_err("check list");
	return 0;
}

void cond_list_destroy_callback(struct pthread_worker_main_obj_t *obj, struct dllist *list)
{
	logg_err("call");
}

int main(int argc, char *argv[])
{
	struct pthread_worker_main_obj_t *p_upload_worker_obj;
	int i = 10;
	struct pthread_worker_callbacks_t callbacks = { .locked_worker_callback = locked_worker_callback,
							.unlocked_worker_callback = unlocked_worker_callback,
							.cond_list_insert_callback = cond_list_insert_callback,
							.cond_list_destroy_callback = cond_list_destroy_callback,
						      };
	log_open("demo");

	p_upload_worker_obj = pthread_worker_init(5, &callbacks);

	if(!p_upload_worker_obj)
	{
		logg_err("error alloc user_data");
		exit(EXIT_FAILURE);
	}
	
	sleep(5);
	pthread_worker_start(p_upload_worker_obj);

	while(i)
	{
		sleep(2);
		i--;
	}

	pthread_worker_stop(p_upload_worker_obj);

	sleep(5);
	pthread_worker_destroy(p_upload_worker_obj);
	exit(EXIT_SUCCESS);
}
