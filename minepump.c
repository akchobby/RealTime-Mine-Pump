#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <semaphore.h>

#include "msg_box.h"
#include "minteger.h"
#include "periodic_task.h"
#include "simu.h"
#include "utils.h"

#define MS_L1 70
#define MS_L2 100

#define Normal 0
#define Alarm1 1
#define Alarm2 2
#define LowLevel 0
#define HighLevel 1

#define P1 96  
#define P2 95  
#define P3 94  
#define P4 91 

/****
 * TO BE DONE:  the explanation of how if all threads have same priority 
 * the msg_box fails
 *
 * Cause found the priority ceiling must be respected when using priority protect 
 * mutex
 * */



/*****************************************************************************/
/* These global variables support communication and synchronization
   between tasks
*/

msg_box mbox_alarm;
sem_t synchro;
m_integer LvlWater, LvlMeth;

/*****************************************************************************/
/* WaterLevelMonitoring_Task is a periodic task, period = 250 ms. At
   each dispatch, it reads the HLS and LLS sensors.
   - If HLS is true, it sends "HighValue" to LvlWater m_integer
   - else, if LLS is false, it sends "LowValue" to LvlWater m_integer
*/
void WaterLevelMonitoring_Body(void) {
  int level = LowLevel;
  //LvlWater->write(level);
 
  printf("Water Measurement Started\n");
  if(ReadHLS()){
	level = HighLevel;
  }else if(!ReadLLS()){
	level = LowLevel;
  }

  MI_write(LvlWater, level);
  printf("Water Measurement Finished\n");
  CHECK_NZ(sem_post(&synchro));
}

/*****************************************************************************/
/* MethaneMonitoring_Task is a periodic task, period = 100 ms. At each
   dispatch, it reads the MS sensor. Depending on the methane level
   (constant MS_L1 and MS_L2), it sends either normal, Alert1 or
   Alert2 to LvlMeth. At the end of the dispatch, it triggers the
   synchronization (semaphore synchro).
*/

void MethaneMonitoring_Body (void) {
  int level = Normal;
  BYTE MS;
  printf("Methane Measurement Started\n");
  MS = ReadMS();
  if (MS >= MS_L2){

	  level = Alarm2;

  }else if(MS >= MS_L1){

	  level = Alarm1;

  }else{
	  level = Normal;
  }
  MI_write(LvlMeth, level);
  printf("Methane Measurement Finished\n");
  CHECK_NZ(sem_post(&synchro));

}

/*****************************************************************************/
/* PumpCtrl_Task is a sporadic task, it is triggered by a synchronization
   semaphore, upon completion of the MethaneMonitoring task. This task
   triggers the alarm logic, and the pump.
   - if the alarm level is different from Normal, it sends the value 1
     to the mbox_alarm message box, otherwise it sends 0;
   - if the alarm level is Alarm2 then the pump is deactivated (cmd =
     0 sent to CmdPump); else, if the water level is high, then the
     pump is activated (cmd = 1 sent to CmdPump), else if the water
     level is low, then the pump is deactivate, otherwise the pump is
     left off.
*/

void *PumpCtrl_Body(void *no_argument) {
  int niveau_eau, niveau_alarme, alarme;
  int cmd=0;
  for (;;) {
	  /*sporadic task requires to sleep till activated*/
	  CHECK_NZ(sem_wait(&synchro));

	  printf("Pump Control Started\n");
	  niveau_eau = MI_read(LvlWater);
	  niveau_alarme = MI_read(LvlMeth);
	  if(niveau_alarme != Alarm2){
		  if(niveau_eau == HighLevel){
			  cmd = 1;
		  }else{
			  cmd =0;
		  }
		  
		  alarme = (niveau_alarme == Alarm1)?1:0;
	  }else{
		  alarme = 1;
		  cmd=0;
	  }
	  CmdPump(cmd);
	  msg_box_send(mbox_alarm, (char*) &alarme);
	  printf("Pump Control Finished\n");

 }

}

/*****************************************************************************/
/* CmdAlarm_Task is a sporadic task, it waits on a message from
   mbox_alarm, and calls CmdAlarm with the value read.
*/

void *CmdAlarm_Body() {
  int value;
  for (;;) {
    printf("CmdAlarm Task Started\n");
    msg_box_receive(mbox_alarm,(char*)&value);
    CmdAlarm(value);
    printf("CmdAlarm Task Finished\n");
  }
}

/*****************************************************************************/
#ifdef RTEMS
void *POSIX_Init() {
#else
int main(void) {
#endif /* RTEMS */

  pthread_t T3,T4;
  printf ("START\n");
  InitSimu(); /* Initialize simulator */

  /* Initialize communication and synchronization primitives */
  mbox_alarm = msg_box_init(sizeof(int));
  sem_init(&synchro,0,0);
  LvlWater = MI_init(90);
  LvlMeth = MI_init(90);

  /* Create task WaterLevelMonitoring_Task */
  struct timespec period_water;
  period_water.tv_nsec = 250 * 1000 * 1000;
  period_water.tv_sec = 0;

  create_periodic_task(period_water, &WaterLevelMonitoring_Body, P3);
  printf("Created Water measurement task\n");

  /* Create task MethaneMonitoring_Task */
  struct timespec period_methane;
  period_methane.tv_nsec = 100 * 1000 * 1000;
  period_methane.tv_sec = 0;

  create_periodic_task(period_methane, &MethaneMonitoring_Body, P2);
  printf("Created methane measurement task\n");

  pthread_attr_t pump_attr, alarm_attr;
  struct sched_param pump_param, alarm_param;

 // CHECK_NZ(pthread_attr_getschedparam(&my_attr, &my_param));


  /* Create task PumpCtrl_Task --------  */
  
  CHECK_NZ(pthread_attr_init(&pump_attr));
  pump_param.sched_priority = P1;
  CHECK_NZ(pthread_attr_setinheritsched(&pump_attr, PTHREAD_EXPLICIT_SCHED));
  CHECK_NZ(pthread_attr_setschedpolicy(&pump_attr, SCHED_FIFO));
  CHECK_NZ(pthread_attr_setschedparam(&pump_attr, &pump_param));

  CHECK_NZ(pthread_create(&T3, &pump_attr , PumpCtrl_Body, NULL));
  printf("Created pump control task\n");

  /* Create task CmdAlarm_Task */
  
  CHECK_NZ(pthread_attr_init(&alarm_attr));
  alarm_param.sched_priority = P1;
  CHECK_NZ(pthread_attr_setinheritsched(&alarm_attr, PTHREAD_EXPLICIT_SCHED));
  CHECK_NZ(pthread_attr_setschedpolicy(&alarm_attr, SCHED_FIFO));
  CHECK_NZ(pthread_attr_setschedparam(&alarm_attr, &alarm_param));

  CHECK_NZ(pthread_create(&T4, &alarm_attr, CmdAlarm_Body, NULL));
  printf("Created alarm task\n");

  pthread_join(T3,0);
  pthread_join(T4,0);

#ifndef RTEMS
  return 0;
#else
  return NULL;
#endif
}

#ifdef RTEMS
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS

#define CONFIGURE_POSIX_INIT_THREAD_TABLE
#define CONFIGURE_INIT

#include <rtems/confdefs.h>
#endif /* RTEMS */
