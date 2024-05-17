#include "p3220162-p3220291-pizza.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

pthread_t *threads = NULL;
unsigned int *ids = NULL;
int mainThreadId;
unsigned int seed;

pthread_mutex_t telOperatorMtx;
pthread_cond_t telOperatorCond;
unsigned int availableTelOperator = N_tel;
pthread_mutex_t printMtx;
pthread_mutex_t totalRevenueMtx;
unsigned int totalRevenue = 0;

const float availablePizzaTypes[] = {P_m, P_p, P_s};
const char *PizzaNames[] = {"Margarita", "Pepperoni", "Special"};

unsigned int weightedProbability(float randomDecimal, const float *probabilityArray, unsigned int size);
void randomlySelectPizzaCountAndType(unsigned int id, unsigned int tSeed, unsigned int totalPizzas, unsigned int *selectedPizzaTypes);
void checkRCAndExitThread(unsigned int id, unsigned int *selectedPizzaTypes, const char *type, int rc);
void checkRCAndExitProcess(const char *type, int rc);
void threadSafePrintf(unsigned int id, unsigned int *selectedPizzaTypes, const char *format, ...);
void freeMainResources();

void *customer(void *x)
{
    struct timespec start, orderEnd;
    clock_gettime(CLOCK_REALTIME, &start);
    unsigned int *selectedPizzaTypes = NULL;
    unsigned int id = *(unsigned int *)x;
    unsigned int tSeed = seed * id;

    // all, except first customer, will call in random time
    if (id != 1)
    {
        int randCallTime = rand_r(&tSeed) % T_orderHigh + T_orderLow;
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d will call after %d seconds\n", id, randCallTime);
        sleep(randCallTime);
    }

    threadSafePrintf(id, selectedPizzaTypes, "Customer %d is Calling\n", id);

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    while (availableTelOperator == 0)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d didnt find available Operator. Blocked...\n", id);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&telOperatorCond, &telOperatorMtx));
    }
    threadSafePrintf(id, selectedPizzaTypes, "Customer %d is Ordering.\n", id);
    availableTelOperator--;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

    int totalPizzas = rand_r(&tSeed) % N_orderHigh + N_orderLow;
    selectedPizzaTypes = (unsigned int *)malloc(totalPizzas * sizeof(int));
    
    if (selectedPizzaTypes == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        pthread_exit(x);
    }

    randomlySelectPizzaCountAndType(id, tSeed, totalPizzas, selectedPizzaTypes);

    int randCardProccessingTime = rand_r(&tSeed) % T_paymentHigh + T_paymentLow;
    sleep(randCardProccessingTime);

    clock_gettime(CLOCK_REALTIME, &orderEnd);
    long inverval = orderEnd.tv_sec - start.tv_sec;

    double randFail = (double)rand_r(&tSeed) / RAND_MAX;
    if (randFail >= P_fail)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d order has been Placed![%ld] \n", id, inverval);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&totalRevenueMtx));
        for (int i = 0; i < totalPizzas; i++)
        {
            if (i == 0)
            {
                totalRevenue += C_m;
            }
            else if (i == 1)
            {
                totalRevenue += C_p;
            }
            else
            {
                totalRevenue += C_s;
            }
        }
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_unlock(&totalRevenueMtx));
    }
    else
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d order failed![%ld] \n", id, inverval);
    }

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    availableTelOperator++;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&telOperatorCond));
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

    free(selectedPizzaTypes);
    pthread_exit(NULL);
}

void checkRCAndExitThread(unsigned int id, unsigned int *memoryToFree, const char *type, int rc)
{
    if (rc != 0)
    {
        pthread_mutex_lock(&printMtx);
        printf("ERROR: return code from %s is %d\n", type, rc);
        pthread_mutex_unlock(&printMtx);
        if(memoryToFree!=NULL)
        {
              free(memoryToFree);
        }  
        pthread_exit((void *)id); // unlocks a mutex automatically? https://linux.die.net/man/3/pthread_cleanup_push
    }
}

void threadSafePrintf(unsigned int id, unsigned int *memoryToFree, const char *format, ...)
{
    if (id == mainThreadId)
    {
        checkRCAndExitProcess("pthread_mutex_lock", pthread_mutex_lock(&printMtx));
    }
    else
    {
        checkRCAndExitThread(id, memoryToFree, "pthread_mutex_lock", pthread_mutex_lock(&printMtx));
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (id == mainThreadId)
    {
        checkRCAndExitProcess("pthread_mutex_unlock", pthread_mutex_unlock(&printMtx));
    }
    else
    {
        checkRCAndExitThread(id, memoryToFree, "pthread_mutex_unlock", pthread_mutex_unlock(&printMtx));
    }
}

void randomlySelectPizzaCountAndType(unsigned int id, unsigned int tSeed, unsigned int totalPizzas, unsigned int *selectedPizzaTypes)
{
    for (int i = 0; i < totalPizzas; i++)
    {
        double randDecimal = (double)rand_r(&tSeed) / RAND_MAX;
        selectedPizzaTypes[i] = weightedProbability(randDecimal, availablePizzaTypes, 3);
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d chose a %s\n", id, PizzaNames[selectedPizzaTypes[i]]);
    }
}

unsigned int weightedProbability(float randDecimal, const float *probabilityArray, unsigned int size)
{
    for (int i = 0; i < size; i++)
    {
        if (randDecimal < probabilityArray[i])
        {
            return i;
        }
        else
        {
            randDecimal -= probabilityArray[i];
        }
    }
    return -1;
}

void checkRCAndExitProcess(const char *type, int rc)
{
    if (rc != 0)
    {
        pthread_mutex_lock(&printMtx);
        printf("ERROR in Main: return code from %s is %d\n", type, rc);
        pthread_mutex_unlock(&printMtx);
        freeMainResources();
        exit(-1);
    }
}

void freeMainResources()
{
    free(threads);
    free(ids);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Arguments should be 3: <out> <number of threads> <seed>");
        return -1;
    }

    int N = atoi(argv[1]);
    seed = atoi(argv[2]);
    threads = malloc(N * sizeof(pthread_t));
    mainThreadId = pthread_self();
    if (threads == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        return -1;
    }
    unsigned int *ids = (unsigned int *)malloc(N * sizeof(int));
    // elegxos an apetyxe i malloc
    if (ids == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        free(threads);
        return -1;
    }

    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&telOperatorMtx, NULL));
    checkRCAndExitProcess("pthread_cond_init", pthread_cond_init(&telOperatorCond, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&totalRevenueMtx, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&printMtx, NULL));

    for (int i = 0; i < N; i++)
    {
        ids[i] = i + 1;
        threadSafePrintf(mainThreadId, NULL, "Main: Thread Creation %d\n", i + 1);
        checkRCAndExitProcess("pthread_create", pthread_create(&threads[i], NULL, customer, &ids[i]));
    }
    for (int i = 0; i < N; i++)
    {
        checkRCAndExitProcess("pthread_join", pthread_join(threads[i], NULL));
    }

    checkRCAndExitProcess("thread_mutex_destroy", pthread_mutex_destroy(&telOperatorMtx));
    checkRCAndExitProcess("thread_cond_destroy", pthread_cond_destroy(&telOperatorCond));
    checkRCAndExitProcess("thread_mutex_destroy", pthread_mutex_destroy(&totalRevenueMtx));
    checkRCAndExitProcess("thread_mutex_destroy", pthread_mutex_destroy(&printMtx));

    freeMainResources();
    return 0;
}