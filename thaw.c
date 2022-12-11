#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// for shared memory
#include <sys/ipc.h> 
#include <sys/shm.h> 

long *tmdat;

int main(int argc, char **argv)
{
    // shared memory for make sure ctrlr don't freeze
    key_t key2 = ftok("/tmp/shm",81);
    int shmid2  = shmget(key2, 2048*sizeof(long), 0666 | IPC_CREAT);
    tmdat = (long*)shmat(shmid2, (void*)0, 0);
    
    time_t now = time(NULL);
    
    if (now > tmdat[2047]+20 && tmdat[2046]) 
    {   char cmdbuf[256];
        sprintf(cmdbuf, "sudo kill %d", tmdat[2046]);
        system(cmdbuf);
        
        tmdat[2047] = now + 50;
    }
    
    return 0;
}








