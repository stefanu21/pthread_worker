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

struct pthread_worker_main_obj_t
{
	enum pthread_worker_status_t service_status;
	unsigned char nr_worker_threads; 
	pthread_t thread_id;
	pthread_cond_t *cond_var;
	pthread_mutex_t *cond_mutex;
	struct dllist cond_list;
 	struct pthread_worker_worker_obj_t *worker_obj;
	void *(*locked_worker_callback)(struct pthread_worker_worker_obj_t *, struct dllist *);
	void *(*unlocked_worker_callback)(struct pthread_worker_worker_obj_t *, struct dllist *);
	int (*cond_list_insert_callback)(struct pthread_worker_main_obj_t *, struct dllist *);
	void (*cond_list_destroy_callback)(struct pthread_worker_main_obj_t *, struct dllist *);
};

struct pthread_worker_worker_obj_t
{
	pthread_cond_t *cond_var;
	pthread_mutex_t *cond_mutex;
	pthread_mutex_t obj_mutex;	
	pthread_t thread_id;
	struct dllist *cond_list;
	enum pthread_worker_status_t *service_status;
	void *(*locked_worker_callback)(struct pthread_worker_worker_obj_t *, struct dllist *);
	void *(*unlocked_worker_callback)(struct pthread_worker_worker_obj_t *, struct dllist *);
};

static int pthread_worker_mutex_trylock(pthread_mutex_t *mutex, time_t retry_us, void *user_data, int (*mutex_busy_callback)(void *))
{
	int rc = 0;

	if(!mutex || !mutex_busy_callback)
	{
		logg_err("parameter error");
		return -1;
	}

	do{
		if((rc = pthread_mutex_trylock(mutex)) != 0)
		{
			if(rc == EBUSY)
			{
				if((rc = mutex_busy_callback(user_data)) != 0)
					return rc;
			} else
			{
				logg_err("unexpected mutex lock error %s", strerror(rc));
				return -1;
			}
		} else
		{
			return 0;
		}

		usleep(retry_us);
	} while(1);
}

static int pthread_worker_mutex_unlock(pthread_mutex_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

static int callback_worker_signal_lock(void *arg)
{
	struct pthread_worker_worker_obj_t *p_worker_obj = (struct pthread_worker_worker_obj_t *)arg;

	if(*(p_worker_obj->service_status) != PTHREAD_WORKER_STATUS_RUNNING)
	{
		logg_err("service stopped");
		return -1;
	}
	else
		return 0;
	return 0;
}

/*
static int callback_worker_startup(void *arg)
{
	struct pthread_worker_worker_obj_t *p_worker_obj = (struct pthread_worker_worker_obj_t *)arg;

	if(*(p_worker_obj->service_status) == PTHREAD_WORKER_STATUS_STOPPED)
	{
		logg_err("service stopped");
		return -1;
	}
	else
		return 0;
}
*/
static void *pthread_worker_callback(void *arg)
{
	struct pthread_worker_worker_obj_t *p_worker_obj = (struct pthread_worker_worker_obj_t *)arg;
	pthread_mutex_t *cond_mutex = p_worker_obj->cond_mutex;
	pthread_cond_t *cond_var = p_worker_obj->cond_var;

	while((*(p_worker_obj->service_status) == PTHREAD_WORKER_STATUS_RUNNING) || (*(p_worker_obj->service_status) == PTHREAD_WORKER_STATUS_STOPPED))
	{
		if(*(p_worker_obj->service_status) == PTHREAD_WORKER_STATUS_STOPPED)
		{
			usleep(100*1000);
			continue;
		}
		
		if(pthread_worker_mutex_trylock(cond_mutex, 500*1000, arg, callback_worker_signal_lock) != 0)
		{
			continue;
		}

		while((dllist_empty(p_worker_obj->cond_list) == 1) && (*(p_worker_obj->service_status) == PTHREAD_WORKER_STATUS_RUNNING))
		{
			struct timespec now;
			int rc = 0;

			clock_gettime(CLOCK_REALTIME, &now);
			now.tv_sec += 1;

			if((rc = pthread_cond_timedwait(cond_var, cond_mutex, &now)) != 0)
			{
				if(rc == ETIMEDOUT)
				{
					logg(LOG_DEBUG, "cond wait timeout");

				}else
				{				
					logg_err("error wait for cond");
					pthread_worker_mutex_unlock(cond_mutex);
					*(p_worker_obj->service_status) = PTHREAD_WORKER_STATUS_DESTROYED;
					return NULL;
				}
			}
		}

		if(*(p_worker_obj->service_status) != PTHREAD_WORKER_STATUS_RUNNING)
		{
			pthread_worker_mutex_unlock(cond_mutex);
			continue;
		}

		if(p_worker_obj->locked_worker_callback)
			p_worker_obj->locked_worker_callback(p_worker_obj, p_worker_obj->cond_list);

		pthread_worker_mutex_unlock(cond_mutex);

		if(p_worker_obj->unlocked_worker_callback)
			p_worker_obj->unlocked_worker_callback(p_worker_obj, p_worker_obj->cond_list);
	}
	return NULL;
}

static int callback_mutex_trylock(void *arg)
{
	struct pthread_worker_main_obj_t *p_main_obj = (struct pthread_worker_main_obj_t *)arg;

	if(p_main_obj->service_status != PTHREAD_WORKER_STATUS_RUNNING)
		return -1;
	
	logg(LOG_DEBUG,"wait for mutex");
	return 0;
}

static void *pthread_main_callback(void *arg)
{
	struct pthread_worker_main_obj_t *p_main_obj = (struct pthread_worker_main_obj_t *)arg;
	
	while((p_main_obj->service_status == PTHREAD_WORKER_STATUS_RUNNING) || (p_main_obj->service_status == PTHREAD_WORKER_STATUS_STOPPED)){
		if(p_main_obj->service_status == PTHREAD_WORKER_STATUS_STOPPED)
		{
			usleep(100 * 1000);
			continue;
		}

		// read data send signal to worker
		if(pthread_worker_mutex_trylock(p_main_obj->cond_mutex, 100*1000, p_main_obj, callback_mutex_trylock) != 0)
		{	
			continue;
		}
		if(p_main_obj->cond_list_insert_callback(p_main_obj, &p_main_obj->cond_list) == 1)
		{
			pthread_cond_signal(p_main_obj->cond_var);
		}
		pthread_mutex_unlock(p_main_obj->cond_mutex);
		usleep(100 * 1000);
	}
	return NULL;
}

struct pthread_worker_main_obj_t *pthread_worker_init(unsigned char nr_worker_threads, struct pthread_worker_callbacks_t *callbacks)
{
	struct pthread_worker_main_obj_t *p_main_obj;	
	void *ret = NULL;
	int i;

	if(nr_worker_threads == 0)
	{
		logg_err("parameter error");
		return NULL;
	}

	p_main_obj = calloc(1, sizeof(*p_main_obj));

	if(!p_main_obj)
		return NULL;

	p_main_obj->service_status = PTHREAD_WORKER_STATUS_STOPPED;
	p_main_obj->nr_worker_threads = nr_worker_threads;
	p_main_obj->cond_var = calloc(1, sizeof(*(p_main_obj->cond_var)));
	p_main_obj->cond_mutex = calloc(1, sizeof(*(p_main_obj->cond_mutex)));
	dllist_init(&p_main_obj->cond_list);

	p_main_obj->locked_worker_callback = callbacks->locked_worker_callback;
	p_main_obj->unlocked_worker_callback = callbacks->unlocked_worker_callback;
	p_main_obj->cond_list_insert_callback = callbacks->cond_list_insert_callback;
	p_main_obj->cond_list_destroy_callback = callbacks->cond_list_destroy_callback;

	if(!(p_main_obj->cond_var) || (pthread_cond_init(p_main_obj->cond_var, NULL) != 0))
	{
		logg_err("error init condition variable");
		goto error_free_all;
	}

	if(!(p_main_obj->cond_mutex) || (pthread_mutex_init(p_main_obj->cond_mutex, NULL) != 0))
	{
		logg_err("error init mutex");
		goto error_destroy_cond;
	}

	p_main_obj->worker_obj = calloc(nr_worker_threads, sizeof(*p_main_obj->worker_obj));

	if(!p_main_obj->worker_obj)
		goto error_destroy_mutex;


	if(pthread_create(&p_main_obj->thread_id, NULL, &pthread_main_callback, (void *)p_main_obj) != 0)
	{
		logg_err("can't create main callback");
		goto error_free_worker_obj;
	}

	for(i = 0; i < nr_worker_threads; i++)
	{
		p_main_obj->worker_obj[i].cond_var = p_main_obj->cond_var;
		p_main_obj->worker_obj[i].cond_mutex = p_main_obj->cond_mutex;

		p_main_obj->worker_obj[i].service_status = &p_main_obj->service_status;
		p_main_obj->worker_obj[i].locked_worker_callback = p_main_obj->locked_worker_callback;
		p_main_obj->worker_obj[i].unlocked_worker_callback = p_main_obj->unlocked_worker_callback;
		p_main_obj->worker_obj[i].cond_list = &p_main_obj->cond_list; 

		if(pthread_mutex_init(&p_main_obj->worker_obj[i].obj_mutex, NULL) != 0)
		{	
			int u;
			void *ret = NULL;	
			logg_err("error init worker mutex");
			p_main_obj->service_status = PTHREAD_WORKER_STATUS_DESTROYED; 
			for(u = 0; u < i; u++)
			{	
				pthread_mutex_destroy(&p_main_obj->worker_obj[u].obj_mutex);
				pthread_join(p_main_obj->worker_obj[u].thread_id, &ret);
			}
			goto error_join_main_thread;
		}

		if(pthread_create(&p_main_obj->worker_obj[i].thread_id, NULL, &pthread_worker_callback, (void *)&p_main_obj->worker_obj[i]) != 0)
		{
			int u = 0;
			logg_err("error create worker thread");
			p_main_obj->service_status = PTHREAD_WORKER_STATUS_DESTROYED;
			pthread_mutex_destroy(&p_main_obj->worker_obj[i].obj_mutex);

			for(u = 0; u < i; u++)
			{	
				pthread_mutex_destroy(&p_main_obj->worker_obj[u].obj_mutex);
				pthread_join(p_main_obj->worker_obj[u].thread_id, &ret);
			}
			goto error_join_main_thread;
		}
	}

	return p_main_obj;

error_join_main_thread:
	pthread_join(p_main_obj->thread_id, &ret);
error_free_worker_obj:
	free(p_main_obj->worker_obj);
error_destroy_mutex:
	pthread_mutex_destroy(p_main_obj->cond_mutex);
error_destroy_cond:
	pthread_cond_destroy(p_main_obj->cond_var);
error_free_all:
	free(p_main_obj->cond_var);
	free(p_main_obj->cond_mutex);
	free(p_main_obj);
	return NULL;
}

void pthread_worker_destroy(struct pthread_worker_main_obj_t *p_main_obj)
{
	void *ret = NULL;
	p_main_obj->service_status = PTHREAD_WORKER_STATUS_DESTROYED;
	
	pthread_join(p_main_obj->thread_id, &ret);

	for(int i = 0; i < p_main_obj->nr_worker_threads; i++)
		pthread_join(p_main_obj->worker_obj[i].thread_id, &ret);

	if((dllist_empty(&p_main_obj->cond_list) != 1) && (p_main_obj->cond_list_destroy_callback != NULL))
		p_main_obj->cond_list_destroy_callback(p_main_obj, &p_main_obj->cond_list);	
	pthread_mutex_destroy(p_main_obj->cond_mutex);
	pthread_cond_destroy(p_main_obj->cond_var);
	
	free(p_main_obj->cond_var);
	free(p_main_obj->cond_mutex);
	free(p_main_obj->worker_obj);
	free(p_main_obj);
}

int pthread_worker_start(struct pthread_worker_main_obj_t *p_main_obj)
{
	if(!p_main_obj)
	{
		logg_err("parameter error");
		return -1;
	}

	p_main_obj->service_status = PTHREAD_WORKER_STATUS_RUNNING;

	return 0;
}

void pthread_worker_stop(struct pthread_worker_main_obj_t *p_main_obj)
{
	if(!p_main_obj)
		return;

	p_main_obj->service_status = PTHREAD_WORKER_STATUS_STOPPED;

	return;
}

