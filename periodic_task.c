#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

#include "periodic_task.h"
#include "utils.h"

/*****************************************************************************/
void *periodic_task_body (void *parameters);

void *periodic_task_body (void *parameters) {
  struct timespec trigger;                      /* Stores next dispatch time */
  struct timespec period;                              /* Period of the task */
  sem_t timer;
  task_parameters *my_parameters = (task_parameters *) parameters;

  period = (*my_parameters).period;
  
  /*Initializing the semaphore named timer, https://man7.org/linux/man-pages/man3/sem_init.3.html */
  CHECK_NZ(sem_init(&timer, 0, 0));

  /*Getting the start  clock time,
   * Here we can use either REALTIME or MONOTONIC, there is an advantage 
   * in using the latter as it will not be effected by time sync protocol
   * jumping like ntp  https://stackoverflow.com/questions/3523442/difference-between-clock-realtime-and-clock-monotonic 
   * But due to the implementation of clock monotonic , it can't be used with sem_timedwait
   * https://stackoverflow.com/questions/40698438/sem-timedwait-with-clock-monotonic-raw-clock-monotonic */
  CHECK_NZ(clock_gettime(CLOCK_REALTIME, &trigger));

  /*Infinite loop*/
  for(;;){

	  /*the job */
	  my_parameters->job();

	  /*next trigger time of job*/
	  add_timespec(&trigger, &trigger, &period);

	  /* semaphore wait based on time, if semaphore is initialised at 0 value, 
	   * the decrement can't happen on wait hence the thread is blocked till timeout time.
	   * https://man7.org/linux/man-pages/man3/sem_wait.3.html*/
	  sem_timedwait(&timer, &trigger);
  }

}

/*****************************************************************************/
void create_periodic_task (struct timespec period, void (*job) (void), int priority ) {
  pthread_t tid;
  task_parameters *parameters = malloc (sizeof (task_parameters));
  
  parameters->period = period;
  parameters->job = job;


  /* Below are some important steps to schedule*/
	
  pthread_attr_t my_attr;
  struct sched_param my_param;
  
  /* get the pointers to the attributes and parameters of the current thread
   * This step can be replaced/neglected */
  CHECK_NZ(pthread_attr_init(&my_attr));
  CHECK_NZ(pthread_attr_getschedparam(&my_attr, &my_param));
  
  /*change the priority, based on user input*/
  my_param.sched_priority = priority;

  /* Here we use the explicit scheduler so tht we can change the policy else the thread created 
   * will inherit frrom the parent thread by default https://man7.org/linux/man-pages/man3/pthread_setschedparam.3.html */
  CHECK_NZ(pthread_attr_setinheritsched(&my_attr, PTHREAD_EXPLICIT_SCHED));

  /*In the stndard linux kerenel, non-realtime scheduling policies are used. For our case Im using a realtime 
   * policy: the FIFO scheduling policy for tasks of same priority */
  CHECK_NZ(pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO));
  
  /* Passing the  params to attributes*/
  CHECK_NZ(pthread_attr_setschedparam(&my_attr, &my_param));
  
  /*creating the periodic task*/
  CHECK_NZ(pthread_create(&tid, &my_attr, &periodic_task_body, parameters));

}
/*****************************************************************************/

/*This test needs to be run as sudo to allow thread priotiy and policy changes*/
#ifdef __TEST__
void dummy (void) {
  printf ("o< \n");
}

int main (void) {
  struct timespec period;
  period.tv_nsec = 500 * 1000 * 1000;
  period.tv_sec  = 0 ;

  create_periodic_task (period, dummy, 90);
  printf ("Task created\n");
  while (1);
  return 0;
}
#endif /* __TEST__ */
