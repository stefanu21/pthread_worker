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


struct dummy_list_t 
{
	char *text;
	struct dllist link;
};

 void *unlocked_worker_callback(int id, struct dllist *list, void *custom)
{
	return NULL;
}

void *locked_worker_callback(int id, struct dllist *list, void *custom)
{
	struct dummy_list_t *dummy = container_of(list->prev, struct dummy_list_t, link);

	dllist_remove(list->prev);

	logg_err("(%d) call %s", id, dummy->text);
	free(dummy->text);
	free(dummy);
	return NULL;
}

int locked_main_callback(struct dllist *list, void *custom)
{	
	static int i = 1;

	if(!i--)
	{	
		int u = 0;
		for (u = 0; u < 5; u++)
		{
			struct dummy_list_t *dummy = calloc(1, sizeof(*dummy));
			if(!dummy)
			{
				logg_err("calloc error");
				return 0;
			}
			if(asprintf(&dummy->text, "hello %d", rand()) < 0)
			{
				logg_err("asprintf error");
				free(dummy);
				return 0;
			} 

			logg_err("signal %s", dummy->text);
			dllist_insert(list, &dummy->link);
		}
		i = 1;
		return 1;
	}

	logg_err("check list");
	return 0;
}

int unlocked_main_callback(void *custom)
{
	sleep(1);
	return 0;
}

void cond_list_destroy_callback(struct dllist *list, void *custom)
{
	struct dummy_list_t *dummy, *tmp;
	logg_err("call, size %d", dllist_length(list));

	dllist_for_each_safe(dummy, tmp, list, link)
	{
		dllist_remove(&dummy->link);
		free(dummy->text);
		free(dummy);
	}
}

void custom_destroy_callback(void *custom)
{
	logg_err("call");
}

int main(int argc, char *argv[])
{
	struct pthread_worker_main_obj_t *p_upload_worker_obj;
	int i = 3;
	struct pthread_worker_callbacks_t callbacks = { .locked_worker_callback = locked_worker_callback,
		.unlocked_worker_callback = unlocked_worker_callback,
		.locked_main_callback = locked_main_callback,
		.unlocked_main_callback = unlocked_main_callback,
		.cond_list_destroy_callback = cond_list_destroy_callback,
		.custom_destroy_callback = custom_destroy_callback,
	};
	log_open("demo");

	p_upload_worker_obj = pthread_worker_init(5, &callbacks, NULL);

	if(!p_upload_worker_obj)
	{
		logg_err("error alloc user_data");
		exit(EXIT_FAILURE);
	}

	pthread_worker_start(p_upload_worker_obj);

	while(i)
	{
		sleep(2);
		i--;
	}

	pthread_worker_stop(p_upload_worker_obj);
	sleep(5);
	pthread_worker_start(p_upload_worker_obj);
	sleep(5);
	pthread_worker_destroy(p_upload_worker_obj);
	exit(EXIT_SUCCESS);
}
