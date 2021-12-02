/**
 * msg_box.c: implement a basic message box of size 1, with message
 *            overwriting
*/

#include <stdlib.h>
#include <string.h>
#include "assert.h"

#include "msg_box.h"
#include "utils.h"

/*****************************************************************************/
msg_box msg_box_init(const unsigned elt_size) {
    msg_box mbox;

    /* Allocate message box */
    if (!(mbox=(msg_box)malloc(sizeof(struct s_msg_box))))
      return 0;

    /* Allocate buffer */
    if (!((*mbox).buf=(char *)malloc(elt_size)))
      return 0;

    (*mbox).empty = true;
    (*mbox).elt_size=elt_size;
    /* Create mutex and cond. var. */
    /* Attribute initialize */
    pthread_mutexattr_t my_attr;
    CHECK_NZ(pthread_mutexattr_init(&my_attr));
    /* setting the right protocol: here always protocol where the mutex currently holding the 
     * mutex inherits the higher priority , if a thread of highr priority tries to access the mutex,
     * by using protect we ensure that this process gets the highest priority of all threads that would 
     * * have to access the mutex.
     * * explaination: https://www.ibm.com/docs/en/aix/7.1?topic=programming-synchronization-scheduling
     * *
     * Interesting to know is that when using the Prio_protect type you must ensure the scheduler is running as 
     * FIFO ,  if pthread the sched header can be used , if not while running the executable one can use the
     * sudo chrt --fifo 1 <executable> _____  https://man7.org/linux/man-pages/man1/chrt.1.html 
     * https://stackoverflow.com/questions/61655903/linux-pthread-mutex-lock-only-works-in-the-second-try
     * */
    
    CHECK_NZ(pthread_mutexattr_setprotocol(&my_attr, PTHREAD_PRIO_PROTECT));
    CHECK_NZ(pthread_mutexattr_setprioceiling(&my_attr, 96));
    
    /* initialize mutex with right attribiutes*/
    CHECK_NZ(pthread_mutex_init(&(mbox->mutex), &my_attr));

    /* Initialize conditional var attributes
     * Check the available pthread cond variable options here: 
     * https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032q/index.html
     * */

    pthread_condattr_t cattr;
    CHECK_NZ  (pthread_condattr_init(&cattr));
    CHECK_NZ(pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED));

    /* Initializing the variable with attribute*/
    CHECK_NZ(pthread_cond_init(&(mbox->not_empty), &cattr));

    return mbox;
}

/*****************************************************************************/
int msg_box_receive(msg_box mbox, char *buf) {


  CHECK_NZ(pthread_mutex_lock(&(mbox->mutex)));	
  /* Wait until the message box has a message */
  while(mbox->empty){
	  CHECK_NZ(pthread_cond_wait(&(mbox->not_empty),&(mbox->mutex)));

  }
  /* Copy the message */
  memcpy(buf,(*mbox).buf,(*mbox).elt_size);
  (*mbox).empty = true;
  CHECK_NZ(pthread_mutex_unlock(&(mbox->mutex)));

  return (*mbox).elt_size;
}

/*****************************************************************************/
int msg_box_send(msg_box mbox, const char *buf) {

  /* Copy the message */
  CHECK_NZ(pthread_mutex_lock(&(mbox->mutex)));
  memcpy((*mbox).buf,buf,(*mbox).elt_size);
  (*mbox).empty=false;
  CHECK_NZ(pthread_cond_broadcast(&(mbox->not_empty)));
  CHECK_NZ(pthread_mutex_unlock(&(mbox->mutex)));

  return (*mbox).elt_size;
}

/*****************************************************************************/
void msg_box_delete(msg_box mbox) {
  pthread_cond_destroy(&(*mbox).not_empty);
  pthread_mutex_lock(&(*mbox).mutex);
  free((*mbox).buf);
  pthread_mutex_unlock(&(*mbox).mutex);
  pthread_mutex_destroy(&(*mbox).mutex);
  free(mbox);
}

/*****************************************************************************/
#ifdef __TEST__
int main (void) {
  char c = 'a', d;
  msg_box mbox = msg_box_init (sizeof (char));
  int e = msg_box_send (mbox, &c);
  printf ("msg_box_send: %d\n", e);
  e = msg_box_receive (mbox, &d);
  printf ("Read %c\n", d);
  return 0;
}
#endif /* __TEST__ */
