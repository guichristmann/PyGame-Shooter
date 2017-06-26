#include <time.h>
#include <stdio.h>
#include <pthread.h>

int RELOADING = 0;

struct Shot{
    int active;
};

struct Shot shot;

void * reloadTimer(void * args){
    int msec = 0, trigger = 3 * 1000; // 3 seconds
    
    clock_t start = clock();
    while (msec < trigger){
        clock_t diff = clock() - start;
        msec = diff * 1000 / CLOCKS_PER_SEC;
    }

    printf("Time taken: %d seconds, %d milliseconds\n", msec/1000, msec%1000);

    shot.active = 1;

    pthread_exit(NULL);
}

int main(){
    pthread_t reloadThread;

    shot.active = 0;
    while(1){
        if (shot.active == 0 && RELOADING == 0){ // creates thread to reload the shot
            RELOADING = 1;
            pthread_create(&reloadThread, NULL, reloadTimer, NULL);
        }
        
    }

    return 0;
}
