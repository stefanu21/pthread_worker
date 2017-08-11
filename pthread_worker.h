#ifndef _PTHREAD_WORKER_H
#define _PTHREAD_WORKER_H

#include "dllist.h"

enum pthread_worker_status_t
{
        PTHREAD_WORKER_STATUS_RUNNING,
        PTHREAD_WORKER_STATUS_STOPPED,
	PTHREAD_WORKER_STATUS_DESTROYED,
};

struct pthread_worker_main_obj_t;
struct pthread_worker_worker_obj_t;

struct pthread_worker_callbacks_t
{
	void *(*locked_worker_callback)(int index, struct dllist *, void *);
	void *(*unlocked_worker_callback)(int index, struct dllist *, void *);
	int (*locked_main_callback)(struct dllist *, void *);
	int (*unlocked_main_callback)(void *);
	void (*cond_list_destroy_callback)(struct dllist *, void *);
	void (*custom_destroy_callback)(void *);
};


void pthread_worker_stop(struct pthread_worker_main_obj_t *p_main_obj);
int pthread_worker_start(struct pthread_worker_main_obj_t *p_main_obj);
void pthread_worker_destroy(struct pthread_worker_main_obj_t *p_main_obj);
struct pthread_worker_main_obj_t *pthread_worker_init(unsigned char nr_of_workers, struct pthread_worker_callbacks_t *callbacks, void *custom);

#endif /*_PTHREAD_WORKER_H*/

