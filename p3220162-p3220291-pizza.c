#include "p3220162-p3220291-pizza.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
// #define EXTRA_DEBUG
// static pthread_key_t* keys;
static int N;
static int *threadStatus; //-1 thread not created, 0 threadCreated, 1 threadDestroyed

pthread_t *threads = NULL;
unsigned int *ids = NULL;
int mainThreadId;
unsigned int seed;

pthread_mutex_t telOperatorMtx;
pthread_cond_t telOperatorCond;
unsigned int availableTelOperator = N_tel;
unsigned int totalOrders = 0;

pthread_mutex_t waitPreviousCustomerCallMtx;
pthread_cond_t waitPreviousCustomerCond;
int waitForPreviousCustomerCall = 0;

pthread_mutex_t printMtx;
pthread_mutex_t totalRevenueMtx;
unsigned int totalRevenue = 0;
unsigned int pizzaSellings[3] = {0};
unsigned int totalSucOrders = 0;
unsigned int totalTimeOrdering = 0;
unsigned int maxTimeOrdering = 0; // same mutex for all 5

pthread_mutex_t cookMtx;
pthread_cond_t cookCond;
unsigned int availableCook = N_cook;

pthread_mutex_t ovenMtx;
pthread_cond_t ovenCond;
unsigned int availableOven = N_oven;

pthread_mutex_t delivererMtx;
pthread_cond_t delivererCond;
unsigned int availableDeliverer = N_deliverer;

pthread_mutex_t totalCoolingMtx;
unsigned int totalTimeCooling = 0;
unsigned int maxTimeCooling = 0;

const float availablePizzaTypes[] = {P_m, P_p, P_s};
const char *PizzaNames[] = {"Margarita", "Pepperoni", "Special"};

unsigned int weightedProbability(float randomDecimal, const float *probabilityArray, unsigned int size);
void randomlySelectPizzaCountAndType(unsigned int id, unsigned int tSeed, unsigned int totalPizzas, unsigned int *selectedPizzaTypes);
void checkRCAndExitThread(unsigned int id, unsigned int *selectedPizzaTypes, const char *type, int rc);
void checkRCAndExitProcess(const char *type, int rc);
void threadSafePrintf(unsigned int id, unsigned int *selectedPizzaTypes, const char *format, ...);
void freeMainResources();
void cancelThreads();
static void destructor(void *args);
static void cleanupUnlockMutex(void *p);
typedef struct destructorArgs
{
    unsigned int id;
    unsigned int *memoryToFree;
} DESTRUCTOR_ARGS;


void *customer(void *x)
{
    DESTRUCTOR_ARGS destructorArgs;
    struct timespec threadStartTime, callTime, bakeFinishTime, currentTime;
    clock_gettime(CLOCK_REALTIME, &threadStartTime);
    unsigned int *selectedPizzaTypes = NULL;
    unsigned int id = *(unsigned int *)x;
    unsigned int tSeed = seed * id;
    long interval = 0;
    long timeSinceStart = 0;
    destructorArgs.id = id;

/*====Start=======The first customer calls at time 0, and each subsequent customer calls after a random integer interval======*/ 
    if (id != 1)
    {
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&waitPreviousCustomerCallMtx));
        pthread_cleanup_push(cleanupUnlockMutex, (void *)&waitPreviousCustomerCallMtx);
        while (waitForPreviousCustomerCall == 1)
        {
            #ifdef EXTRA_DEBUG
            threadSafePrintf(id, selectedPizzaTypes, "Customer %d waiting for the previous customer to call. Blocked...\n", id);
            #endif

            checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&waitPreviousCustomerCond, &waitPreviousCustomerCallMtx));
        }
        waitForPreviousCustomerCall = 1;       
        pthread_cleanup_pop(0);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&waitPreviousCustomerCallMtx));

        int randCallTime = rand_r(&tSeed) % T_orderHigh + T_orderLow;
        
        #ifdef EXTRA_DEBUG
        clock_gettime(CLOCK_REALTIME, &currentTime);
        timeSinceStart = currentTime.tv_sec - threadStartTime.tv_sec;      
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d will call after %d minutes. [Time:%ld minute(s)]\n", id, randCallTime, timeSinceStart);
        #endif

        sleep(randCallTime);

        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&waitPreviousCustomerCallMtx));
        pthread_cleanup_push(cleanupUnlockMutex, (void *)&waitPreviousCustomerCallMtx);
        waitForPreviousCustomerCall = 0;
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&waitPreviousCustomerCond));
        pthread_cleanup_pop(0);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&waitPreviousCustomerCallMtx));   
    }
    #ifdef EXTRA_DEBUG
    clock_gettime(CLOCK_REALTIME, &currentTime);
    timeSinceStart = currentTime.tv_sec - threadStartTime.tv_sec;  
    threadSafePrintf(id, selectedPizzaTypes, "Customer %d is Calling [Time:%ld minute(s)]\n", id, timeSinceStart);
    #endif
    
    clock_gettime(CLOCK_REALTIME, &callTime);
 //====END=======The first customer calls at time 0, and each subsequent customer calls after a random integer interval======

// =====START===When all phone operators are busy, the customer waits for the next available phone operator==================
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&telOperatorMtx);
    while (availableTelOperator == 0)
    {
        #ifdef EXTRA_DEBUG
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d didn't find available Operator. Blocked...\n", id);
        #endif
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&telOperatorCond, &telOperatorMtx));
    }
    #ifdef EXTRA_DEBUG
    threadSafePrintf(id, selectedPizzaTypes, "Customer %d is Ordering.\n", id);
    #endif

    availableTelOperator--;
    totalOrders += 1;
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));
// ====END===When all phone operators are busy, the customer waits for the next available phone operator==================


// ===START===When a customer connects to an operator, they select a random integer number of pizzas and Type======================
    int totalPizzas = rand_r(&tSeed) % N_orderHigh + N_orderLow;
    selectedPizzaTypes = (unsigned int *)malloc(totalPizzas * sizeof(int));

    // pthread_setspecific(keys[id], selectedPizzaTypes);
    if (selectedPizzaTypes == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        pthread_exit(x);
    }

    destructorArgs.memoryToFree = selectedPizzaTypes;
    pthread_cleanup_push(destructor, &destructorArgs);
    randomlySelectPizzaCountAndType(id, tSeed, totalPizzas, selectedPizzaTypes);
// ===END===When a customer connects to an operator, they select a random integer number of pizzas and Type====


// ===START===The operator needs a random number of minutes to charge the customer's credit card=====
    int randCardProccessingTime = rand_r(&tSeed) % T_paymentHigh + T_paymentLow;
    sleep(randCardProccessingTime);

    clock_gettime(CLOCK_REALTIME, &currentTime);
    long timeSinceStart = currentTime.tv_sec - threadStartTime.tv_sec;
    
// ===END===The operator needs a random number of minutes to charge the customer's credit card=====


/* ==============With probability Pfail the charge fails and the order is cancelled, otherwise the order is registered,=================================================
                 the store's revenue is increased by Cm, Cp or Cs euros per pizza, depending on the pizzas in the order*/
    double randFail = (double)rand_r(&tSeed) / RAND_MAX;
    if (randFail >= P_fail)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d has been Placed![Time: %ld minute(s)] \n", id, timeSinceStart);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&totalRevenueMtx));
        pthread_cleanup_push(cleanupUnlockMutex, (void *)&totalRevenueMtx);
        totalSucOrders += 1;
        for (int i = 0; i < totalPizzas; i++)
        {
            if (selectedPizzaTypes[i] == 0)
            {
                totalRevenue += C_m;
                pizzaSellings[0] += 1;
            }
            else if (selectedPizzaTypes[i] == 1)
            {
                totalRevenue += C_p;
                pizzaSellings[1] += 1;
            }
            else
            {
                totalRevenue += C_s;
                pizzaSellings[2] += 1;
            }
        }
        
        pthread_cleanup_pop(0);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_unlock(&totalRevenueMtx));
    }
    else
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d order failed![Time: %ld minute(s)] \n", id, timeSinceStart);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
        pthread_cleanup_push(cleanupUnlockMutex, (void *)&telOperatorMtx);
        availableTelOperator++;
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&telOperatorCond));
        pthread_cleanup_pop(0);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

        pthread_exit(NULL);
    }
/* ====END=========With probability Pfail the charge fails and the order is cancelled, otherwise the order is registered,=================================================
                 the store's revenue is increased by Cm, Cp or Cs euros per pizza, depending on the pizzas in the order*/


    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&telOperatorMtx);
    availableTelOperator++;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&telOperatorCond));
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&cookMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&cookMtx);
    while (availableCook < 0)
    {
        #ifdef EXTRA_DEBUG
        threadSafePrintf(id, selectedPizzaTypes, "Order %d didn't find any preparer. Waiting...\n", id);
        #endif
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&cookCond, &cookMtx));
    }
    #ifdef EXTRA_DEBUG
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is being prepared.\n", id);
    #endif

    availableCook -= 1;
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&cookMtx));
    sleep(totalPizzas * T_prep);

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&ovenMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&ovenMtx);
    while (availableOven < totalPizzas)
    {
        #ifdef EXTRA_DEBUG
        threadSafePrintf(id, selectedPizzaTypes, "Order %d didn't find enough ovens. Waiting...\n", id);
        #endif

        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&ovenCond, &ovenMtx));
    }
    #ifdef EXTRA_DEBUG
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is in Oven.\n", id);
    #endif
    availableOven -= totalPizzas;
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&ovenMtx));

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&cookMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&cookMtx);
    availableCook += 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&cookCond));
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&cookMtx));

    sleep(T_bake);
    clock_gettime(CLOCK_REALTIME, &bakeFinishTime);

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&delivererMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&delivererMtx);
    while (availableDeliverer < 0)
    {
        #ifdef EXTRA_DEBUG
        threadSafePrintf(id, selectedPizzaTypes, "Order %d can't get a deliverer. Waiting...\n", id);
        #endif

        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&delivererCond, &delivererMtx));
    }
    #ifdef EXTRA_DEBUG
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is being packed.\n", id);
    #endif

    availableDeliverer -= 1;
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&delivererMtx));

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&ovenMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&ovenMtx);
    availableOven += totalPizzas;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_broadcast", pthread_cond_broadcast(&ovenCond));
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&ovenMtx));

    sleep(T_pack * totalPizzas);
    clock_gettime(CLOCK_REALTIME, &currentTime);
    int XcallToPack = currentTime.tv_sec - callTime.tv_sec;
    timeSinceStart = currentTime.tv_sec - threadStartTime.tv_sec;
    threadSafePrintf(id, selectedPizzaTypes, "Order %d was ready in %ld minute(s).[Time: %ld minute(s)].\n", id, XcallToPack, timeSinceStart);

    int randDeliveringTime = rand_r(&tSeed) % (T_delHigh - T_delLow + 1) + T_delLow;
    sleep(randDeliveringTime);

    clock_gettime(CLOCK_REALTIME, &currentTime);
    timeSinceStart = currentTime.tv_sec - threadStartTime.tv_sec;
    interval = currentTime.tv_sec - bakeFinishTime.tv_sec;
    int YcallToDelivery = currentTime.tv_sec - callTime.tv_sec;
    
    threadSafePrintf(id, selectedPizzaTypes, "Order %d was delivered in %ld minute(s).[Time: %ld minute(s)]\n", id, YcallToDelivery, timeSinceStart);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&totalCoolingMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&totalCoolingMtx);
    totalTimeCooling += interval;
    if (maxTimeCooling < interval)
    {
        maxTimeCooling = interval;
    }

    totalTimeOrdering += YcallToDelivery;
    if (YcallToDelivery > maxTimeOrdering)
    {
        maxTimeOrdering = YcallToDelivery;
    }
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&totalCoolingMtx));

    sleep(randDeliveringTime);
    #ifdef EXTRA_DEBUG
    threadSafePrintf(id, selectedPizzaTypes, "Delivery boy from order %d has returned.\n", id);
    #endif

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&delivererMtx));
    pthread_cleanup_push(cleanupUnlockMutex, (void *)&delivererMtx);
    availableDeliverer += 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&delivererCond));
    pthread_cleanup_pop(0);
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&delivererMtx));

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
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

        #ifdef EXTRA_DEBUG
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d chose a %s\n", id, PizzaNames[selectedPizzaTypes[i]]);
        #endif
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

//===================Checks, process and thread handling==========================
void checkRCAndExitProcess(const char *type, int rc)
{
    if (rc != 0)
    {
        pthread_mutex_lock(&printMtx);
        printf("ERROR in Main: return code from %s is %d\n", type, rc);
        pthread_mutex_unlock(&printMtx);
        cancelThreads();
        exit(-1);
    }
}

void cancelThreads()
{
    #ifdef EXTRA_DEBUG
    printf("======= MAIN Cancels threads ======= \n");
    #endif

    void *status = NULL;
    for (int i = 0; i < N; i++)
    {   
        #ifdef EXTRA_DEBUG
        printf("Thread id %d status %d\n",i+1, threadStatus[i]);
        #endif

        if (threadStatus[i] == 0) // if is created
        {
            pthread_cancel(threads[i]);
        }
    }

    for (int i = 0; i < N; i++)
    {
        if (threadStatus[i] == 0)
        {
            checkRCAndExitProcess("pthread_join", pthread_join(threads[i], &status));

            #ifdef EXTRA_DEBUG
            if (status == PTHREAD_CANCELED)
            {
                printf("main(): thread %d was canceled\n", ids[i]);
            }
            else
            {
                printf("main(): thread %d wasn't canceled)\n", ids[i]);
            } // maybe it has already finished
            #endif
        }
    }

    freeMainResources();
}

void freeMainResources()
{
    #ifdef EXTRA_DEBUG
    printf("Freeing main resources\n");
    #endif

    free(threadStatus);
    free(threads);
    free(ids);
}

void checkRCAndExitThread(unsigned int id, unsigned int *memoryToFree, const char *type, int rc)
{
    if (rc != 0)
    {
        pthread_mutex_lock(&printMtx);
        printf("ERROR: return code from %s is %d\n", type, rc);
        pthread_mutex_unlock(&printMtx);

        pthread_exit(&id); // Runs cleanup_pop and frees memory https://linux.die.net/man/3/pthread_cleanup_push
                           // cast to void was giving warning
    }
}

static void destructor(void *args)
{
    DESTRUCTOR_ARGS *destructorArgs = (DESTRUCTOR_ARGS *)args;

    #ifdef EXTRA_DEBUG
    printf("Destructor of id %d is executing. ", destructorArgs->id);
    #endif

    if (destructorArgs->memoryToFree != NULL)
    {
        #ifdef EXTRA_DEBUG
        printf("FREEING IT'S MEMORY -> ");
        #endif

        free(destructorArgs->memoryToFree);

        #ifdef EXTRA_DEBUG
        printf("OK\n");
        #endif
    }
    threadStatus[destructorArgs->id - 1] = 1;
}

static void cleanupUnlockMutex(void *p)
{
    pthread_mutex_unlock(p);
}


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Arguments should be 3: <out> <number of threads> <seed>");
        return -1;
    }

    char *p;
    errno = 0;
    long lN = strtol(argv[1], &p, 10);
    if (*p != '\0' || errno != 0)
    {
        return -1;
    }

    long lseed = strtol(argv[2], &p, 10);
    if (*p != '\0' || errno != 0)
    {
        return -1;
    }
    if (lN < INT_MIN || lN > INT_MAX || lseed < 0 || lseed > UINT_MAX)
    {
        return -1;
    }

    N = (int)lN;
    seed = (uint)lseed;

    threads = malloc(N * sizeof(pthread_t));
    mainThreadId = pthread_self();

    if (threads == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        return -1;
    }
    ids = (unsigned int *)malloc(N * sizeof(int));
    // elegxos an apetyxe i malloc
    if (ids == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        free(threads);
        return -1;
    }

    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&printMtx, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&telOperatorMtx, NULL));
    checkRCAndExitProcess("pthread_cond_init", pthread_cond_init(&telOperatorCond, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&totalRevenueMtx, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&cookMtx, NULL));
    checkRCAndExitProcess("pthread_cond_init", pthread_cond_init(&cookCond, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&ovenMtx, NULL));
    checkRCAndExitProcess("pthread_cond_init", pthread_cond_init(&ovenCond, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&delivererMtx, NULL));
    checkRCAndExitProcess("pthread_cond_init", pthread_cond_init(&delivererCond, NULL));
    checkRCAndExitProcess("pthread_mutex_init", pthread_mutex_init(&totalCoolingMtx, NULL));

    // keys = malloc (N*sizeof(pthread_key_t));
    threadStatus = malloc(N * sizeof(int));
    if (threadStatus == NULL)
    {
        printf("ERROR: Malloc failed not enough memory!\n");
        free(threads);
        free(ids);
        return -1;
    }

    for (int i = 0; i < N; i++)
    {
        threadStatus[i] = -1; // initialize to -1, meaning it's not created yet
    }
    for (int i = 0; i < N; i++)
    {
        ids[i] = i + 1;

        #ifdef EXTRA_DEBUG
        threadSafePrintf(mainThreadId, NULL, "Main: Thread Creation %d\n", i + 1);
        #endif

        // pthread_key_create(&keys[i], destructor);
        checkRCAndExitProcess("pthread_create", pthread_create(&threads[i], NULL, customer, &ids[i]));
        threadStatus[i] = 0;
        // checkRCAndExitProcess("Test cancellation request ",1);// Test point
    }

    // ================testing cancel S===========
    //sleep(3); // sleep to test cancel
    //checkRCAndExitProcess("Test cancellation request ", 1);
    // ================testing cancel E===========
    for (int i = 0; i < N; i++)
    {
        checkRCAndExitProcess("pthread_join", pthread_join(threads[i], NULL));
    }

    printf("\nTotal Revenue: %d\n", totalRevenue);
    printf("Pizzas sold\nmargarita: %d\npeperoni: %d\nspecial: %d\n", pizzaSellings[0], pizzaSellings[1], pizzaSellings[2]);
    if (totalSucOrders > 0)
    {
        printf("There were %d succesfull orders and %d failed orders\n", totalSucOrders, totalOrders - totalSucOrders);
        printf("Average ordering time: %d minute(s)\nLongest ordering time: %d minute(s)\n", totalTimeOrdering / totalSucOrders, maxTimeOrdering);
        printf("Average cooling time: %d minute(s)\nLongest cooling time: %d minute(s)\n", totalTimeCooling / totalSucOrders, maxTimeCooling);
    }
    else
    {
        printf("There were no succesfull orders\n");
    }

    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&printMtx));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&telOperatorMtx));
    checkRCAndExitProcess("pthread_cond_destroy", pthread_cond_destroy(&telOperatorCond));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&totalRevenueMtx));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&cookMtx));
    checkRCAndExitProcess("pthread_cond_destroy", pthread_cond_destroy(&cookCond));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&ovenMtx));
    checkRCAndExitProcess("pthread_cond_destroy", pthread_cond_destroy(&ovenCond));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&delivererMtx));
    checkRCAndExitProcess("pthread_cond_destroy", pthread_cond_destroy(&delivererCond));
    checkRCAndExitProcess("pthread_mutex_destroy", pthread_mutex_destroy(&totalCoolingMtx));

    freeMainResources();
    return 0;
}
