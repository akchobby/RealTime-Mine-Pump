#include <stdlib.h>
#include <pthread.h>

#include "minteger.h"
#include "utils.h"

/*****************************************************************************/
m_integer MI_init(int priority) {
  m_integer m;
  m = (m_integer) malloc(sizeof(struct s_m_integer));
  (*m).value = 0;
  /* Configure mutex, using a concurrency control policy */

  /* Attribute initialize */
  pthread_mutexattr_t my_attr;
  CHECK_NZ(pthread_mutexattr_init(&my_attr));

  /* setting the right protocol: here always protocol where the mutex currently holding the 
   * mutex inherits the higher priority , if a thread of highr priority tries to access the mutex,
   * by using protect we ensure that this process gets the highest priority of all threads that would 
   * have to access the mutex.
   * explaination: https://www.ibm.com/docs/en/aix/7.1?topic=programming-synchronization-scheduling
   *
   * Interesting to know is that when using the Prio_protect type you must ensure the scheduler is running as 
   * FIFO ,  if pthread the sched header can be used , if not while running the executable one can use the
   * sudo chrt --fifo 1 <executable> 
   * https://stackoverflow.com/questions/61655903/linux-pthread-mutex-lock-only-works-in-the-second-try
   * */
  
  CHECK_NZ(pthread_mutexattr_setprotocol(&my_attr, PTHREAD_PRIO_INHERIT));
  CHECK_NZ(pthread_mutexattr_setprioceiling(&my_attr, priority));

  /* initialize mutex with right attribiutes*/
  CHECK_NZ(pthread_mutex_init(&(m->mutex), &my_attr));
  return m;
}

/*****************************************************************************/
void MI_write(m_integer m, int v) {
	CHECK_NZ(pthread_mutex_lock(&((*m).mutex)));
	(*m).value = v;
	CHECK_NZ(pthread_mutex_unlock(&(m->mutex)));
}

/*****************************************************************************/
int MI_read(m_integer m) {
  int v;
  CHECK_NZ(pthread_mutex_lock(&((*m).mutex)));
  v=(*m).value;
  CHECK_NZ(pthread_mutex_unlock(&(m->mutex)));
  return v;
}

/*****************************************************************************/
#ifdef __TEST__
int main (void) {
  m_integer my_minteger = MI_init (99);
  MI_write (my_minteger, 42);
  printf ("Read %d\n", MI_read (my_minteger));
  return 0;
}
#endif /* __TEST__ */
