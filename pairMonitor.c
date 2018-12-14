#include "pairMonitor.h"

#include <pthread.h>     // For mutex and condtion variables.
#include <stdlib.h>      // For malloc/free/exit
#include <stdio.h>       // For printf (but not using it)
#include <string.h>
#include <math.h>
#include <sys/time.h>

// Record for a pair of threads who have entered.
// We have one of these for every pair of threads in the room.
typedef struct {
  // Names for the two threads who entered.
  char name[ 2 ][ NAME_MAX + 1 ];
  int filled;
  int number_leaving;
} Pair;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;//mutex lock 
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;//condition for enter
pthread_cond_t e = PTHREAD_COND_INITIALIZER;//condition to check capacity
pthread_cond_t leave_cond = PTHREAD_COND_INITIALIZER;//condition for leave
pthread_cond_t done = PTHREAD_COND_INITIALIZER;//condition for terminate

// True if we're still running.
bool running = true;

// Capacity for the room we're entering.
int cap;

int pairsFilled = 0;//number of pairs filled at a certain point in time in the pair list. 


// List of all pairs of threads that have entered.
// An array of Pair structs with a length determined by capacity.
Pair *pairList;

// Time when we first create the monitor.
struct timeval startTime;

void initPairMonitor( int capacity ) {
  // Remember when the program started running.
  gettimeofday( &startTime, NULL );

  // Create and initialize the list of Pair structs.
  cap = capacity;
  pairList = (Pair *) malloc( sizeof( Pair ) * cap );
  for ( int i = 0; i < cap; i++ ) {
    // No threads are part of this pair yet.
    strcpy( pairList[ i ].name[ 0 ], "" );//initialize pair to empty string 
    strcpy( pairList[ i ].name[ 1 ], "" );//initialize pair to empty string 
	pairList[i].filled = 0;//number of places filled in the pair 
	pairList[i].number_leaving = 0;//number of threads leaving from the pair
  }
}

// Return the current execution time in milliseconds.
static long elapsedTime() {
  struct timeval endTime;
  gettimeofday( &endTime, NULL );

  long delta = 1000L * ( endTime.tv_sec - startTime.tv_sec ) +
    (long) round( ( endTime.tv_usec - startTime.tv_usec ) / 1000.0 );

  return delta;
}

//destroys the pair list 
void destroyPairMonitor() {
  free( pairList );//free the pair list
}

//enters the pair list as a pair 
bool enter( const char *name ) {
  pthread_mutex_lock(&lock);//lock 

  // Find the index of an available Pair on the pairList.
  int idx;
  while(running == false) {
	pthread_mutex_unlock(&lock);//if terminated, unlock and return false
	return false;
  }
  while ( pairsFilled == cap) {
	pthread_cond_wait(&e, &lock);//if pair list is full, wait 
  }
  
  for(int i = 0; i < cap; i++) {
	if(pairList[i].filled != 2) { 
	  if(strcmp(pairList[i].name[0], "") == 0) {//if first element in pair is empty 
	    idx = i;//idx will be equal to i
	    strcpy(pairList[i].name[0], name);//copy name of thread in the pair
		pairList[i].filled = 1;//filled becomes 1
		if(pairList[i].filled != 2 && running == true) {//if it is runnning and pair is not totally filled 
		  pthread_cond_wait(&condition, &lock);//wait on the entry condition 
		}
		if(running == false) {
		  pthread_mutex_unlock(&lock);//if terminated, unlock and return false 
		  return false;
		}
		break;//break and unlock and return true if everything goes well 
	  } else if(strcmp(pairList[i].name[1], "") == 0 && pairList[i].number_leaving == 0 ){//if second element of pair is empty 
	    idx = i;//idx becomes i 
		strcpy(pairList[i].name[1], name);//copy name of thread in the pair 
		pairList[i].filled = 2;//pair is totally filled 
		pairsFilled++;//number of pairs filled in the list increments
		pthread_cond_broadcast(&condition);//broadcast to entry condition that pair is made and pair can enter the room 
		//The second thread to enter can report when the two threads enter.
		long delta = elapsedTime();
		printf( "Enter: %s %s (%ld.%03ld)\n", pairList[ idx ].name[ 0 ], pairList[ idx ].name[ 1 ], delta / 1000, delta % 1000 );//print the pair of entering 
		break;//break and unlock and return true if everything goes well 
	  }
	}
  }
  
  pthread_mutex_unlock(&lock);//unlock 
  return true;//return true 
}

//leave 
void leave( const char *name ) {
	
  pthread_mutex_lock(&lock);//lock 
  
  while(running == false) {
	pthread_mutex_unlock(&lock);//if terminated, unlock and return 
	return;
  }
  
  for ( int i = 0; i < cap; i++) {
	if ( strcmp(pairList[i].name[0], name) == 0) {//if name is first element in the pair
	  pairList[i].number_leaving++;//increase number of threads leaving in the pair 
	  if(pairList[i].number_leaving != 2) {
		pthread_cond_wait(&leave_cond, &lock);//if number of threads leaving is not 2, wait till the second one leaves as well. 
	  }
	  if(pairList[i].number_leaving == 2) {//when second one also wants to leave 
		pairsFilled -= 1;//number of pairs filled in the pair list decrements. 
		pthread_cond_broadcast(&leave_cond);//broadcast to the leave condition 
		// The second thread to leave can report when the two threads enter.
		long delta = elapsedTime();
		printf( "Leave: %s %s (%ld.%03ld)\n", pairList[ i ].name[ 0 ], pairList[ i ].name[ 1 ], delta / 1000, delta % 1000 );//print the leaving pair 
		strcpy(pairList[i].name[0], "");//remove name of thread 
		strcpy(pairList[i].name[1], "");//remove name of thread 
	    pairList[i].number_leaving = 0;//number of threads leaving for that pair is reinitialized to 0. 
		pairList[i].filled = 0;//number of threads filled for that pair is reinitialized to 0. 
	    pthread_mutex_unlock(&lock);//unlock 
		break;//break
	  }
	}
	
	if(strcmp(pairList[i].name[1], name) == 0) {//if name is second element in the pair
	  pairList[i].number_leaving++;//increase number of threads leaving in the pair 
	  if(pairList[i].number_leaving != 2) {
		pthread_cond_wait(&leave_cond, &lock);//if number of threads leaving is not 2, wait till the second one leaves as well. 
	  }
	  if(pairList[i].number_leaving == 2) {//when second one also wants to leave 
		pairsFilled -= 1;//number of pairs filled in the pair list decrements. 
		pthread_cond_broadcast(&leave_cond);//broadcast to the leave condition 
		// The second thread to leave can report when the two threads enter.
		long delta = elapsedTime();
		printf( "Leave: %s %s (%ld.%03ld)\n", pairList[ i ].name[ 0 ], pairList[ i ].name[ 1 ], delta / 1000, delta % 1000 );//print the leaving pair 
		strcpy(pairList[i].name[0], "");//remove name of thread 
		strcpy(pairList[i].name[1], "");//remove name of thread 
	    pairList[i].number_leaving = 0;//number of threads leaving for that pair is reinitialized to 0. 
		pairList[i].filled = 0;//number of threads filled for that pair is reinitialized to 0. 
	    pthread_mutex_unlock(&lock);//unlock 
		break;//break
	  }
	}
  }
  
  pthread_cond_broadcast(&e);//broadcast to capacity condition 
  pthread_mutex_unlock(&lock);//unlock 
}

//terminate 
void terminate() {
  pthread_mutex_lock(&lock);//lock
  running = false;//make running false 
  
  while(pairsFilled > 0) {
	pthread_cond_wait(&done, &lock);//while pair list is not empty, wait on termianting condition 
  }
  
  pthread_cond_broadcast(&done);//broadcast terminating condition 
  pthread_cond_broadcast(&condition);//broadcast entry condition 
  pthread_cond_broadcast(&leave_cond);//broadcast leaving condition 
  pthread_cond_broadcast(&e);//broadcast capacity condition 
  pthread_mutex_unlock(&lock);//unlock 
}
