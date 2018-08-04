/* Maria Ausilia Napoli Spatafora
X81000442*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <string.h>

#define BUF_SIZE 1024
#define S_READ 0
#define S_WRITE 1
#define TYPE_EXIT 0
#define TYPE_REGFILE 1
#define TYPE_DIR 2

typedef struct{
	char text[BUF_SIZE];
	unsigned type; // 0:exit_process, 1:file, 2:dir
} shmData;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
	return semop(sem_des, operazioni, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
	return semop(sem_des, operazioni, 1);
}

void Reader(int dsSem, int dsShm, char *dir){
	shmData *p; //for shared memory
	if((p = (shmData*)shmat(dsShm, NULL, 0)) == (shmData*)-1){
		perror("shmat_R");
		exit(1);
	}

	struct stat info;
	DIR *current;
	if((current = opendir(dir)) == NULL){
		perror("opendir");
		exit(1);
	}
	struct dirent *pointerDir;
	while((pointerDir = readdir(current)) != NULL){
		char file[BUF_SIZE];
		strncpy(file, dir, BUF_SIZE);
		strncat(file, "/", BUF_SIZE);
		strncat(file, pointerDir->d_name, BUF_SIZE);
		//printf("file:%s\n", file);
		if((lstat(file, &info)) == -1){
			perror("lstat");
			exit(1);
		}
		if(S_ISDIR(info.st_mode)){
			if((strncmp(pointerDir->d_name, ".", BUF_SIZE)) == 0 || (strncmp(pointerDir->d_name, "..", BUF_SIZE)) == 0) continue;
			//dir consumer
			//critical_region 1:
			//printf("Reader - WAIT\n");
			if((WAIT(dsSem, S_WRITE)) == -1){
				perror("WAIT_S_WRITE");
				exit(1);
			}
			p->type = TYPE_DIR;
			strncpy(p->text, file, BUF_SIZE);
			//printf("Reader - SIGNAL\n");
			if((SIGNAL(dsSem, S_READ)) == -1){
				perror("SIGNAL_S_READ");
				exit(1);
			}
		}
		else if(S_ISREG(info.st_mode)){
			//file consumer
			//printf("Reader - WAIT\n");
			if((WAIT(dsSem, S_WRITE)) == -1){
				perror("WAIT_S_WRITE");
				exit(1);
			}
			p->type = TYPE_REGFILE;
			strncpy(p->text, file, BUF_SIZE);
			//printf("Reader - SIGNAL\n");
			if((SIGNAL(dsSem, S_READ)) == -1){
				perror("SIGNAL_S_READ");
				exit(1);
			}
		}
	}
	closedir(current);

	//synchro-msg
	if((WAIT(dsSem, S_WRITE)) == -1){
		perror("WAIT_S_WRITE");
		exit(1);
	}
	p->type = TYPE_EXIT;
	//printf("Reader - SIGNAL\n");
	if((SIGNAL(dsSem, S_READ)) == -1){
		perror("SIGNAL_S_READ");
		exit(1);
	}

	//printf("Reader exit\n");
	exit(0);
}

void DirConsumer(int dsSem, int dsShm, int nReader){
	int count = 0;
	shmData *p; //for shared memory
	if((p = (shmData*)shmat(dsShm, NULL, 0)) == (shmData*)-1){
		perror("shmat_D");
		exit(1);
	}
	while(1){
		//printf("DirConsumer - WAIT\n");
		if((WAIT(dsSem, S_READ)) == -1){
			perror("WAIT_S_READ");
			exit(1);
		}
		if(p->type == TYPE_DIR){
			//printf("DirConsumer - SIGNAL\n");
			printf("%s [directory]\n", p->text);
			if((SIGNAL(dsSem, S_WRITE)) == -1){
				perror("WAIT_S_WRITE");
				exit(1);
			}
		}
		else if(p->type == TYPE_EXIT){
			count++;
			if((SIGNAL(dsSem, S_READ)) == -1){ //to allow other consumer to read the flag
				perror("SIGNAL_S_READ_D_exit");
				exit(1);
			}
			//printf("DirConsumer - SIGNAL\n");
			if((SIGNAL(dsSem, S_WRITE)) == -1){
				perror("WAIT_S_WRITE");
				exit(1);
			}
			if(count == nReader) break;
		}
		else{
			if((SIGNAL(dsSem, S_READ)) == -1){ //to allow other consumer to read the data
				perror("SIGNAL_S_READ_D");
				exit(1);
			}
		}
		usleep(5000);
	}
	//printf("DirConsumer exit\n");
	exit(0);
}

void FileConsumer(int dsSem, int dsShm, int nReader){
	int count = 0;
	shmData *p; //for shared memory
	if((p = (shmData*)shmat(dsShm, NULL, 0)) == (shmData*)-1){
		perror("shmat_F");
		exit(1);
	}
	struct stat info;
	while(1){
		//printf("FileConsumer - WAIT\n");
		if((WAIT(dsSem, S_READ)) == -1){
			perror("WAIT_S_READ");
			exit(1);
		}
		if(p->type == TYPE_REGFILE){
			if((lstat(p->text, &info)) == -1){
				perror("lstat_F");
				exit(1);
			}
			printf("%s [file di %ld bytes]\n", p->text, info.st_size);
			//printf("FileConsumer - SIGNAL\n");
			if((SIGNAL(dsSem, S_WRITE)) == -1){
				perror("SIGNAL_S_WRITE");
				exit(1);
			}
		}
		else if(p->type == TYPE_EXIT){
			count++;
			if((SIGNAL(dsSem, S_READ)) == -1){
				perror("SIGNAL_S_READ_D_exit");
				exit(1);
			}
			//printf("FileConsumer - SIGNAL\n");
			if((SIGNAL(dsSem, S_WRITE)) == -1){
				perror("SIGNAL_S_WRITE");
				exit(1);
			}
			if(count == nReader) break;
		}
		else{
			if((SIGNAL(dsSem, S_READ)) == -1){
				perror("SIGNAL_S_READ_F");
				exit(1);
			}
		}
		usleep(5000);
	}
	//printf("FileConsumer exit\n");
	exit(0);
}

int main(int argc, char **argv){

	int dsSem, dsShm;
	
	if(argc < 2){
		printf("Usage <directory> [<directory>...]: %s\n", argv[0]);
		exit(0);
	}
	struct stat info;
	int j = 1;
	while(j < argc){
		if((lstat(argv[j], &info)) == -1){
			perror("lstat");
			exit(1);
		}
		if(!S_ISDIR(info.st_mode)){
			printf("A parameter isn't a dir!\n");
			exit(0);
		}
		j++;
	}

	if((dsShm = shmget(IPC_PRIVATE, sizeof(shmData), IPC_CREAT | IPC_EXCL | 0600)) == -1){
		perror("shmget");
		exit(1);
	}
	if((dsSem = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600)) == -1){
		perror("semget");
		exit(1);
	}
	if((semctl(dsSem, S_READ, SETVAL, 0)) == -1){
		perror("SETVAL_S_READ");
		exit(1);
	}
	if((semctl(dsSem, S_WRITE, SETVAL, 1)) == -1){
		perror("SETVAL_S_WRITE");
		exit(1);
	}

	pid_t pidFather = getpid();

	if((fork() != 0)){
		if((fork() == 0)){ //son 2
			//printf("I'm Dir-Consumer\n");
			DirConsumer(dsSem, dsShm, argc-1);
		}
	}
	else{ //son 1
		//printf("I'm File-Consumer\n");
		FileConsumer(dsSem, dsShm, argc-1);
	}

	int i = 1;
	while(i<argc && getpid() == pidFather){
		if((fork() == 0)){ //son n
			//printf("I'm Reader_%d\n", i);
			Reader(dsSem, dsShm, argv[i]);
		}
		else i++;
	}

	if(getpid() == pidFather){
		wait(NULL);
		wait(NULL);
		i = 1;
		while(i < argc){
			i++;
			wait(NULL);
		}

		//printf("I'm Father\n");

		if((shmctl(dsShm, IPC_RMID, NULL)) == -1){
			perror("shmctl");
			exit(1);
		}
		if((semctl(dsSem, 0, IPC_RMID, NULL)) == -1){
			perror("semctl");
			exit(1);
		}

		//printf("Father exit\n");
	}
}