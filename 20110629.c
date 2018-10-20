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

char *dirname;
char *dirtmp;
DIR *dp;

static void * t_func (void *arg);

pthread_mutex_t mutex;

int main(int argc, char *argv[]) {

	int nThread, i, retcode;
	pthread_t *th;
	struct stat stat_buf;

	/* Check and save command line arguments*/
	if(argc != 3) {
		fprintf(stderr, "Missing parameters. Usage: program nThread dirName\n");
		exit(1);
	}

	nThread = atoi(argv[1]);

	if (stat(argv[2], &stat_buf) < 0) {
		fprintf(stderr, "Stat error 1.\n");
		exit(1);
	}

    if ((stat_buf.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "%s is not a directory.\n", argv[2]);
		exit(1);
	}

	dirname = malloc(strlen(argv[2]) * sizeof(char));
	strcpy(dirname, argv[2]);

	dirtmp = malloc((strlen(argv[2]) + 4) * sizeof(char));
	strcpy(dirtmp, argv[2]);
	strcat(dirtmp, "/tmp");

	printf("Creating temp dir.\n");
	/* Create tmp dir*/
	mkdir(dirtmp, 0777);
	printf("Temp dir created.\n");

	/* Init mutex */
	pthread_mutex_init(&mutex, NULL);
	
	printf("Mutex initialized.\n");

	/* Oper dir */
	  if ((dp = opendir(dirname)) == NULL){
	    fprintf(stderr, "Error opendir %s\n", dirname);
	    exit(1);
	  }

	printf("Dir opened.\n");
	/* Create threads */

	th = malloc(nThread * sizeof(pthread_t));	
	for(i = 0; i < nThread; i++) {
		
		printf("Creating thread.\n");
		retcode = pthread_create (&th[i], NULL, t_func, NULL);
	  	if (retcode != 0) {
	    	fprintf (stderr, "create failed %d\n", retcode);
			exit(1);
		}

	}

	for(i = 0; i < nThread; i++) {
		
		pthread_join(th[i], NULL);	
	
	}
	
	/* Close dir */
	closedir(dp);

	/* Destroy and free all*/
	pthread_mutex_destroy(&mutex);	
	free(th);

	pthread_exit(NULL);
}

static void * t_func (void *arg) {

	char filename [_POSIX_PATH_MAX + 1];
	char file [_POSIX_PATH_MAX + 1];
	char file1 [_POSIX_PATH_MAX + 1];
	char file2 [_POSIX_PATH_MAX + 1];
	char fileoutTMP [_POSIX_PATH_MAX + 1];
	char fileoutDIR [_POSIX_PATH_MAX + 1];
	int count, fd1, fd2, fdout, i;
	struct stat stat_buf;
	struct dirent *dirp;
	off_t len1, len2;
	char *paddr1, *paddr2; 
		
	printf("Inside t_func\n");
	while(1) {

	/* In the critical section read two files */
	pthread_mutex_lock(&mutex);
	
	printf("Inside lock.\n");
	count = 0;
	while(count < 2) {
		dirp = readdir(dp);
		if(dirp == NULL) { //no more file in the directory
			pthread_mutex_unlock(&mutex);
			pthread_exit(0);
		}
		sprintf(filename,"%s/%s", dirname,dirp->d_name);
		if (lstat(filename, &stat_buf) < 0)	{ //non segue softlinks
			fprintf(stderr, "Stat error.\n");
			exit(1);
		}

		if ((stat_buf.st_mode & S_IFMT) == S_IFREG) {
			if(count == 0) { //first file found				
				strcpy(file, dirp->d_name);
				len1 = stat_buf.st_size;
				printf("st_size %jd\n", stat_buf.st_size);
			}
			else { //second file found
				strcpy(file2, filename);
				sprintf(fileoutTMP, "%s/%s%s", dirtmp, file, dirp->d_name);
				sprintf(fileoutDIR, "%s/%s%s", dirname, file, dirp->d_name);
				sprintf(file1, "%s/%s", dirname, file);
				len2 = stat_buf.st_size;

			}
			count++;
		}
	
	}

	/* Map the two files and delete them from directory */
	if((fd1 = open(file1, O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file %s.\n", file1);
		exit(1);
	} 
	if((fd2 = open(file2, O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file %s.\n", file2);
		close(fd1);
		exit(1);
	}

	printf("mapping: file1: %s file2: %s len1: %ld, len2: %ld\n", file1, file2, len1, len2);
	paddr1 = mmap(0, len1, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd1, 0);

	paddr2 = mmap(0, len2, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd2, 0);

	if(paddr1 == (caddr_t) -1 || paddr2 == (caddr_t) -1) {
		fprintf(stderr, "Error mapping input files\n");
		exit(1);
	}
	close(fd1);
	close(fd2);
	remove(file1);
	remove(file2);

	pthread_mutex_unlock(&mutex);

	/* open output file */

	if ((fdout = open (fileoutTMP, O_RDWR | O_CREAT | O_TRUNC, 0777)) < 0) {
		fprintf(stderr, "Error opening file.\n");
		close(fd1);
		close(fd2);
		exit(1);
	}

	/*copy files */
	for(i = 0; i < len1; i++){
		write(fdout, &paddr1[i], sizeof(char));
		printf("%c", paddr1[i]);
}
	for(i = 0; i < len2; i++)
		write(fdout, &paddr2[i], sizeof(char));

	link(fileoutTMP, fileoutDIR);
	remove(fileoutTMP);
	if (munmap (paddr1, len1) < 0) {
		fprintf(stderr, "Error unmapping.\n");
		exit(1);
	}

	if (munmap (paddr2, len2) < 0) {
		fprintf(stderr, "Error unmapping.\n");
		exit(1);
	}

	close(fdout);
	
	}
	
}








