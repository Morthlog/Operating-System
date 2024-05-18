#include "p3220162-p3220291-pizza.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

pthread_t *threads = NULL;
unsigned int *ids = NULL;
int mainThreadId;
unsigned int seed;

pthread_mutex_t telOperatorMtx;
pthread_cond_t telOperatorCond;
unsigned int availableTelOperator = N_tel;
unsigned int totalOrders = 0;

pthread_mutex_t printMtx;
pthread_mutex_t totalRevenueMtx;
unsigned int totalRevenue = 0;
unsigned int pizzaSellings[3]= {0};
unsigned int totalSucOrders = 0; 
unsigned int totalTimeOrdering = 0;
unsigned int maxTimeOrdering = 0;//same mutex for all 5

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
        threadSafePrintf(id, selectedPizzaTypes, "Customer %d didn't find available Operator. Blocked...\n", id);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&telOperatorCond, &telOperatorMtx));
    }
    threadSafePrintf(id, selectedPizzaTypes, "Customer %d is Ordering.\n", id);
    availableTelOperator--;
    totalOrders += 1;
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
    long interval = orderEnd.tv_sec - start.tv_sec;

    double randFail = (double)rand_r(&tSeed) / RAND_MAX;
    if (randFail >= P_fail)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d has been Placed![%ld minute(s)] \n", id, interval);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&totalRevenueMtx));
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
        totalTimeOrdering += interval;
        if (interval > maxTimeOrdering)
        {
        	maxTimeOrdering = interval;
        }
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_unlock(&totalRevenueMtx));
    }
    else
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order number %d order failed![%ld minute(s)] \n", id, interval);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    	availableTelOperator++;
    	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&telOperatorCond));
    	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

    	free(selectedPizzaTypes);
    	pthread_exit(NULL);
    }

    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx));
    availableTelOperator++;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&telOperatorCond));
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&telOperatorMtx));

	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&cookMtx));
    while (availableCook < 0)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order %d didn't find any preparer. Waiting...\n", id);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&cookCond, &cookMtx));
    }
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is being prepared.\n", id);
    availableCook -= 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&cookMtx));
	sleep(totalPizzas * T_prep);

	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&ovenMtx));
    while (availableOven < totalPizzas)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order %d didn't find enough ovens. Waiting...\n", id);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&ovenCond, &ovenMtx));
    }
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is in Oven.\n", id);
    availableOven -= totalPizzas;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&ovenMtx));
    
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&cookMtx));
    availableCook += 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&cookCond));
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&cookMtx));
    
	sleep(10);
	clock_gettime(CLOCK_REALTIME, &start);

	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&delivererMtx));
    while (availableDeliverer < 0)
    {
        threadSafePrintf(id, selectedPizzaTypes, "Order %d can't get a deliverer. Waiting...\n", id);
        checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_wait", pthread_cond_wait(&delivererCond, &delivererMtx));
    }
    threadSafePrintf(id, selectedPizzaTypes, "Order %d is being packed.\n", id);
    availableDeliverer -= 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&delivererMtx));

	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&ovenMtx));
	availableOven += totalPizzas;
	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_broadcast", pthread_cond_broadcast(&ovenCond));
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&ovenMtx));

	sleep(T_pack * totalPizzas);

	threadSafePrintf(id, selectedPizzaTypes, "Order %d is being delivered.\n", id);
	int randDeliveringTime = rand_r(&tSeed) % (T_delHigh - T_delLow + 1) + T_delLow;
	sleep(randDeliveringTime);

	clock_gettime(CLOCK_REALTIME, &orderEnd);
    interval = orderEnd.tv_sec - start.tv_sec;

	threadSafePrintf(id, selectedPizzaTypes, "Order %d was given to customer.[%ld minute(s)]\n", id, interval);
	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&totalCoolingMtx));
	totalTimeCooling += interval;
	if (maxTimeCooling < interval)
	{
		maxTimeCooling = interval;
	}
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&totalCoolingMtx));
    
    sleep(randDeliveringTime);
	threadSafePrintf(id, selectedPizzaTypes, "Delivery boy from order %d has returned.\n", id);
	checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_lock", pthread_mutex_lock(&delivererMtx));
    availableDeliverer += 1;
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_cond_signal", pthread_cond_signal(&delivererCond));
    checkRCAndExitThread(id, selectedPizzaTypes, "pthread_mutex_unlock", pthread_mutex_unlock(&delivererMtx));

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
        pthread_exit(&id); // unlocks a mutex automatically? https://linux.die.net/man/3/pthread_cleanup_push
        					// cast to void was giving warning
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
    if (argc != 3)
    {
        printf("Arguments should be 3: <out> <number of threads> <seed>");
        return -1;
    }
    
    char* p;
    errno = 0; 
    long lN = strtol(argv[1], &p, 10);
    if (*p != '\0' ||  errno != 0) 
    {
        return -1;
    }
    long lseed = strtol(argv[2], &p, 10);
    if (*p != '\0' ||  errno != 0) 
    {
        return -1;
    }
	if (lN < INT_MIN || lN > INT_MAX || lseed < 0 || lseed > UINT_MAX) 
	{
        return -1;
    }
    int N = (int) lN;
    seed = (uint) lseed;
    
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
    
    printf("\nTotal Revenue: %d\n", totalRevenue);
	printf("Pizzas sold\nmargarita: %d\npeperoni: %d\nspecial: %d\n", pizzaSellings[0], pizzaSellings[1], pizzaSellings[2]);
	if (totalSucOrders>0 )
	{
		printf("There were %d succesfull orders and %d failed orders\n", totalSucOrders, totalOrders - totalSucOrders);
		printf("Average ordering time: %d minute(s)\nLongest ordering time: %d minute(s)\n", totalTimeOrdering/totalSucOrders, maxTimeOrdering);
		printf("Average cooling time: %d minute(s)\nLongest cooling time: %d minute(s)\n", totalTimeCooling/totalSucOrders, maxTimeCooling);
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
