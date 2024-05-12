#include "p3220162-p3220291-pizza.h"

//Formart copied from frontistirio

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
//#define N 10
pthread_mutex_t lock;
pthread_cond_t cond;
int resources = 2; //diathesimoi poroi
int id[N];
int main(int argc, char* argv[]) 
{
    int rc;
    int N = (int) argv[1];
    pthread_t* threads = malloc(N *sizeof(pthread_t));
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    for (int i = 0; i < N; i++) 
    {
        id[i] = i+1;
        printf("Main: dhmioyrgia nhmatos %d\n", i+1);
        rc = pthread_create(&threads[i], NULL, order, &id[i]);
    }
    for (int i = 0; i < N; i++) 
    {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    return 0;
}

void *order(void *x)
{
    int id = *(int *)x;
    int rc;
    printf("H paraggelia %d xekinhse\n",id);
    rc = pthread_mutex_lock(&lock);
    while (resources == 0) 
    {
        printf("H paraggelia %d den brike diathesimo poro.
        Blocked...\n", id);
        rc = pthread_cond_wait(&cond, &lock);
    }
    printf("H paraggelia %d eksipiretitai.\n", id);
    resources--;
    rc = pthread_mutex_unlock(&lock);
    sleep(5); //kane kapoia douleia me ton poro
    rc = pthread_mutex_lock(&lock);
    printf("H paraggelia %d eksipiretithike epitixos! \n", id);
    resources++;
    rc = pthread_cond_signal(&cond);
    rc = pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
}