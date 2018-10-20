#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>

#define MAX_LINE 256

typedef struct _params {
	int id;
	int class;
	sem_t sem;
	int is_waiting;
}Params;

int nThread;
Params *params;
int pipe_array[2];

sem_t sem_global;

static void * t_func (void *arg);

int main(int argc, char *argv[]){

	pthread_t *th;
	int i, j, ret_code, found;
	int len;
	char line[MAX_LINE + 1];

	if(argc != 2) {
		fprintf(stderr, "Missing parameters. Usage: program nThread\n");
		exit(1);
	}
	nThread = atoi(argv[1]);
	th = malloc(nThread * sizeof(pthread_t));
	params = (Params*)malloc (nThread * sizeof(struct _params));
	sem_init(&sem_global, 0, 0);

	if(pipe(pipe_array) < 0){
		fprintf(stderr, "Error creating pipe.\n");
		exit(1);
	}

	for(i = 0; i < nThread; i++){

		sem_init(&params[i].sem, 0, 0);
		params[i].id = i;
		params[i].class = rand()%3 + 1; 

		params[i].is_waiting = 0;

		ret_code = pthread_create(&th[i], NULL, t_func, (void *) &params[i]);
		if(ret_code != 0) {
			fprintf(stderr, "create failed %d\n", ret_code);
			exit(1);
		}
	}	

	/*Sblock the first thread */
	found = 0;
//	while(!found) {	
	sem_wait(&sem_global);
		for(j = 1; j <= 3 && !found; j++){ 
			for(i = 0; i < nThread && !found; i++) {

				if(params[i].is_waiting && params[i].class == j) {
					sem_post(&params[i].sem);
					found = 1;
				}
			}
		}
//	}

	/* Read from pipe */
	while(1) {
		if ( (len = read(pipe_array[0], line, MAX_LINE)) <= 0) {
			fprintf(stderr, "Error reading from pipe\n");
			exit(1);
		}
		line[len] = '\0';
		printf("%s\n", line); //printf on a file
		fflush(stdout); 
	}

	/* Wait for threads and close all*/
	for(i = 0; i < nThread; i++){
		ret_code = pthread_join(th[i], NULL);
		if(ret_code != 0) {
			fprintf(stderr, "create failed %d\n", ret_code);
			exit(1);
		}

	}
	close(pipe_array[0]);
	close(pipe_array[1]);		

	free(th);
	free(params);
	return 0;
}

static void * t_func (void *arg){

	Params *p = (Params*)arg;
	char line[MAX_LINE + 1];
	int len, n, i, j, found;

	while(1) {
	
		sleep(rand()%3); //sleep(rand()%8 + 2);
		sprintf(line, "<%d, C%d, 0>", p->id, p->class);

		len = strlen(line);	
		if((n = write(pipe_array[1], line, len)) != len) {
			fprintf(stderr, "Error writing on pipe\n");
			exit(1);
		}

		p->is_waiting = 1;
		sem_post(&sem_global);
		sem_wait(&p->sem);
		p->is_waiting = 0;

		sprintf(line, "<%d, C%d, 1>", p->id, p->class);

		len = strlen(line);	
		if((n = write(pipe_array[1], line, len)) != len) {
			fprintf(stderr, "Error writing on pipe\n");
			exit(1);
		}

		found = 0;
		//while(found == 0) {
		sem_wait(&sem_global);	
			for(j = 1; j <= 3 && !found; j++){ 
				for(i = 0; i < nThread && !found; i++) {

					if(params[i].is_waiting && params[i].class == j) {
						sem_post(&params[i].sem);
						found = 1;
					}
				}
			}
		//}

	}

	return 0;
}
