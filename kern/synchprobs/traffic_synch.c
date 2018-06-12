#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

static struct lock *intersectionLock;
static struct cv *intersectionCV;
struct array *waitingVehicles;
volatile int numVehicles = 0;

typedef struct Vehicles
{
  Direction origin;
  Direction destination;
} Vehicle;

bool right_turn(Vehicle *v);
bool check_constraints(Vehicle *incomingV);

bool
right_turn(Vehicle *v) {
  KASSERT(v != NULL);
  if (((v->origin == west) && (v->destination == south)) ||
      ((v->origin == south) && (v->destination == east)) ||
      ((v->origin == east) && (v->destination == north)) ||
      ((v->origin == north) && (v->destination == west))) {
    return true;
  } else {
    return false;
  }
}

bool
check_constraints(Vehicle *incomingV) {
  /* compare newly-added vehicle to each other vehicles in in the intersection */
  for(unsigned int i=0; i<array_num(waitingVehicles); i++) {
    Vehicle *ithWaitingV = array_get(waitingVehicles,i);
    if (ithWaitingV->origin == incomingV->origin) continue;
    /* no conflict if vehicles go in opposite directions */
    if ((ithWaitingV->origin == incomingV->destination) &&
        (ithWaitingV->destination == incomingV->origin)) continue;
    /* no conflict if one makes a right turn and 
       the other has a different destination */
    if ((right_turn(ithWaitingV) || right_turn(incomingV)) &&
  (incomingV->destination != ithWaitingV->destination)) continue;
    // constrints failed
    cv_wait(intersectionCV,intersectionLock);
    return false;
  }

  KASSERT(lock_do_i_hold(intersectionLock));

  array_add(waitingVehicles,incomingV,NULL);
  numVehicles++;
  return true;
}



/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  /*intersectionSem = sem_create("intersectionSem",1);
  if (intersectionSem == NULL) {
    panic("could not create intersection semaphore");
  }*/
  
  intersectionLock = lock_create("intersectionLock");
  if (intersectionLock == NULL) {
    panic("could not create intersection lock");
  }

  intersectionCV = cv_create("intersectionCV");
  if (intersectionCV == NULL) {
    panic("could not create intersection condition variable");
  }

  waitingVehicles = array_create();
  array_init(waitingVehicles);
  if (waitingVehicles == NULL) {
    panic("could not create intersection array");
  }

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  /*KASSERT(intersectionSem != NULL);
  sem_destroy(intersectionSem);*/

  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);
  KASSERT(waitingVehicles != NULL);

  lock_destroy(intersectionLock);
  cv_destroy(intersectionCV);
  array_destroy(waitingVehicles);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  // (void)destination; /* avoid compiler complaint about unused parameter */
  /* KASSERT(intersectionSem != NULL);
  P(intersectionSem); */

  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);
  KASSERT(waitingVehicles != NULL);

  Vehicle *incomingV = kmalloc(sizeof(struct Vehicles));
  incomingV->origin = origin;
  incomingV->destination = destination;

  lock_acquire(intersectionLock);

  while(!check_constraints(incomingV)) {
    // nothing done :P
    numVehicles+=0;
  }
  
  lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  // (void)origin;  /* avoid compiler complaint about unused parameter */
  // (void)destination; /* avoid compiler complaint about unused parameter */
  /* KASSERT(intersectionSem != NULL);
  V(intersectionSem); */

  KASSERT(intersectionLock != NULL);
  KASSERT(intersectionCV != NULL);
  KASSERT(waitingVehicles != NULL);

  lock_acquire(intersectionLock);

  for(unsigned int i=0; i<array_num(waitingVehicles); i++) {
    Vehicle *ithWaitingV = array_get(waitingVehicles,i);
    if((ithWaitingV->origin == origin) && (ithWaitingV->destination == destination)) {
      array_remove(waitingVehicles,i);
      numVehicles--;
      cv_broadcast(intersectionCV,intersectionLock);
      break;
    }
  }

  lock_release(intersectionLock);
}
