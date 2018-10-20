#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>

typedef struct {
	int best_rank;
	int id_best_rank;
	sem_t sem;
	int n;
} Rank;

typedef struct {
	int id;
	int rank;
} Params;

Rank r;

void funcSignal(int signo){
	if(signo == SIGUSR1){
		printf("segnaleee\n");
	}
}

static void * t_func (void *arg);

int main(int argc, char *argv[]) {

	int N, i, j, ret_code, index;
	Params *p;
	pthread_t *th;
	int *temp;

	if(argc != 2) {
		fprintf(stderr, "Missing parameters. Usage: program nThread\n");
		exit(1);
	}

	N = atoi(argv[1]);
	p = (Params *)malloc(N * sizeof(Params));
	th = malloc(N * sizeof(pthread_t));

	r.id_best_rank = -1;
	r.best_rank = N + 1; 
	sem_init(&r.sem, 0, 1);
	r.n = 0;

	temp = malloc( N * sizeof(int));
	for(i = 0; i < N; i++)
		temp[i] = i+1;

	/*Create threads */
	for (i = 0; i < N; i++) {

		p[i].id = i;
		if(i == N - 1) {
			index = 0;
		} else {
			index = rand()%(N-i-1);
		}		
		p[i].rank = temp[index];

		temp[index] = temp[N - i - 1];
		temp[N - i - 1] = p[i].rank;

		ret_code = pthread_create(&th[i], NULL, t_func, (void *) &p[i]);
		if(ret_code != 0) {
			fprintf(stderr, "create failed %d\n", ret_code);
			exit(1);
		}

	}

	/* Wait rand time between 2 and 5 sec */
//	sleep(rand()%4 + 2);
 
	for(i = 0; i < N - 3; i++) {
		if(i == N - 4) {
			index = 0;
		} else {
			index = rand()%(N-4);
		}		
		j = temp[index];

		temp[index] = temp[N - i - 4];
		temp[N - i - 4] = p[i].rank;
		printf("%d %d\n", i, j);

		pthread_kill(th[j-1], SIGUSR1);
	}

	/* Wait for threads */
	for(i = 0; i < N; i++){
		ret_code = pthread_join(th[i], NULL);
		if(ret_code != 0) {
			fprintf(stderr, "create failed %d\n", ret_code);
			exit(1);
		}
	}

	free(th);
	free(p);
	sem_destroy(&r.sem);
	return 0;
	
}

static void * t_func (void *arg){

	Params *p = (Params*)arg;

	//pthread_detach(pthread_self());
	signal(SIGUSR1, funcSignal);

	
	pause();
	sem_wait(&r.sem);
	
	if(p->rank < r.best_rank) {
		r.best_rank = p->rank;
		r.id_best_rank = p->id;
	}
	
	printf("My id: %d, my rank: %d. Best id: %d, best rank: %d.\n", p->id, p->rank, r.id_best_rank, r.best_rank);

	sem_post(&r.sem);

}















