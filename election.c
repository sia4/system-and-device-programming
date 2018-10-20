#include <stdio.h>
#include "pthread.h"
#include "semaphore.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

#define N 10

typedef struct{
  int rank;
  long int id;
  int num_votes;
  pthread_mutex_t mutex;
} Best;
Best *best;

sem_t *barrier;

int max_random (int max);
 
static void *
process (void *arg){
  int *j = (int *) arg;
  int rank, i;
  long int id;
  
  rank = *j;
  id = pthread_self ();
  pthread_detach (pthread_self ());
  pthread_mutex_lock (&best->mutex);
  if (rank > best->rank) {
    best->rank = rank;
    best->id = id;
  }
  best->num_votes++;
  if (best->num_votes < N){
    pthread_mutex_unlock (&best->mutex);
    sem_wait (barrier);             /* wait for all to vote */
  }
  else { // best->num_votes == N
    pthread_mutex_unlock (&best->mutex);
    for (i = 0; i < N - 1; i++)
      sem_post (barrier);           /* release all waiting */
  }
  printf ("thread id = %ld rank = %d -- leader id = %ld  rank = %d\n", id, rank, best->id, best->rank);
}

int
main (int argc, char **argv){
  pthread_t th;
  void *retval;
  int i, j, k, *pi, r[N];
  struct timeval tv;

  best = (Best *) malloc (sizeof (Best));
  best->rank = 0;
  best->num_votes = 0;
  
  pthread_mutex_init (&best->mutex, NULL);
  barrier = (sem_t *) malloc (sizeof (sem_t));
  sem_init (barrier, 0, 0);
  
  gettimeofday(&tv, NULL);
  srand (tv.tv_usec * getpid ());
  
  for (i = 0; i < N; i++)
    r[i] = i;
  k = N;
  for (i = 0; i < N; i++) {
    pi = (int *) malloc (sizeof (int));
    j = max_random (k);
    *pi = r[j] + 1;
    r[j] = r[--k];  
    pthread_create (&th, NULL, process, pi);
  }
  pthread_exit (0);
} 

int
max_random (int max)
{
  return (int) (random () % max);
}
