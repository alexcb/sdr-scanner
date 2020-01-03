#include "should_stop.h"

#include <pthread.h>
#include <stddef.h>

int thread_stop_issued = 0;
pthread_mutex_t thread_stop_mutex;
pthread_cond_t thread_stop_cond;

int should_thread_stop( void )
{
	int ret = 0;
	pthread_mutex_lock( &thread_stop_mutex );
	ret = thread_stop_issued;
	pthread_mutex_unlock( &thread_stop_mutex );
	return ret;
}

void stop_thread( void )
{
	pthread_mutex_lock( &thread_stop_mutex );
	thread_stop_issued = 1;
	pthread_mutex_unlock( &thread_stop_mutex );
}

void stop_thread_init( void )
{
	pthread_cond_init( &thread_stop_cond, NULL );
	pthread_mutex_init( &thread_stop_mutex, NULL );
}
