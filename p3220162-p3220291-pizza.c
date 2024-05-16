#include "p3220162-p3220291-pizza.h"

//Formart copied from frontistirio
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#define BILLION 1000000000L;
pthread_mutex_t telOperatorMtx;
pthread_cond_t telOperatorCond;
unsigned int availableTelOperator = N_tel;

pthread_mutex_t printMtx;

pthread_mutex_t totalRevenueMtx;
unsigned int totalRevenue = 0;

unsigned int seed;
const float availablePizzaTypes[] = { P_m ,P_p,P_s };
const char* PizzaNames[]= {"Margarita", "Pepperoni","Special"};
unsigned int weightedProbability(float randomDecimal,const float* probabilityArray, unsigned int size);
void randomlySelectPizzaCountAndType(unsigned int id,unsigned int tSeed,unsigned int totalPizzas,unsigned int* selectedPizzaTypes);
void checkRCAndExitThread(const char* type,int rc,unsigned int id, unsigned int* selectedPizzaTypes);

void* customer(void* x)
{
    struct timespec start,orderEnd;
    clock_gettime(CLOCK_REALTIME, &start);
    unsigned int* selectedPizzaTypes=NULL;
    unsigned int id = *(unsigned int*)x;
    unsigned int tSeed = seed * id;

    //all, except first customer, will call in random time
    if (id != 1)
    {
        int randCallTime = rand_r(&tSeed) % T_orderHigh + T_orderLow;  
        printf("Customer %d will call after %d seconds\n", id, randCallTime);
        sleep(randCallTime);
    }
      
    checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&printMtx), id, selectedPizzaTypes);
    printf("Customer %d is Calling\n", id);
    checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&printMtx), id, selectedPizzaTypes);
   

    checkRCAndExitThread("pthread_mutex_lock", pthread_mutex_lock(&telOperatorMtx), id, selectedPizzaTypes);
    while (availableTelOperator == 0)
    {
        printf("Customer %d didnt find available Operator. Blocked...\n", id);       
        checkRCAndExitThread("pthread_cond_wait",pthread_cond_wait(&telOperatorCond, &telOperatorMtx), id, selectedPizzaTypes);
    }
    printf("Customer %d is Ordering.\n", id);  
    availableTelOperator--;
    checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&telOperatorMtx), id, selectedPizzaTypes);

    int totalPizzas= rand_r(&tSeed) % N_orderHigh + N_orderLow;
    selectedPizzaTypes = (unsigned int*)malloc(totalPizzas * sizeof(int));   
    randomlySelectPizzaCountAndType(id, tSeed, totalPizzas, selectedPizzaTypes);
    
    int randCardProccessingTime = rand_r(&tSeed) % T_paymentHigh + T_paymentLow;
    sleep(randCardProccessingTime); 

    clock_gettime(CLOCK_REALTIME, &orderEnd);
    long inverval=orderEnd.tv_sec-start.tv_sec;
    double randFail = (double)rand_r(&tSeed) / RAND_MAX;
    if(randFail>=P_fail)
    {
        checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&printMtx), id, selectedPizzaTypes);
        printf("Order number %d order has been Placed![%ld] \n", id, inverval);
        checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&printMtx), id, selectedPizzaTypes);
    
        checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&totalRevenueMtx), id, selectedPizzaTypes);
        for(int i=0;i<totalPizzas;i++)
        {
            if(i==0)
            {
                totalRevenue+=C_m;
            }
            else if (i==1)
            {
                totalRevenue+=C_p;
            }
            else 
            { totalRevenue+=C_s; }
        } 
        checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_unlock(&totalRevenueMtx), id, selectedPizzaTypes);
    }
    else
    {
        checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&printMtx), id, selectedPizzaTypes);
        printf("Order number %d order failed![%ld] \n", id, inverval);
        checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&printMtx), id, selectedPizzaTypes);
    }
   
    checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&telOperatorMtx), id, selectedPizzaTypes);
    availableTelOperator++;
    checkRCAndExitThread("pthread_cond_signal",pthread_cond_signal(&telOperatorCond), id, selectedPizzaTypes);
    checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&telOperatorMtx), id, selectedPizzaTypes);

    free(selectedPizzaTypes);
    pthread_exit(NULL);
}

void checkRCAndExitThread(const char* type, int rc, unsigned int id, unsigned int* selectedPizzaTypes)
{
    if (rc != 0) //probably should unlock also?
    {	
       
        printf("ERROR: return code from %s is %d\n",type, rc);
        free(selectedPizzaTypes);
        pthread_exit(id);
	}
}

void randomlySelectPizzaCountAndType(unsigned int id, unsigned int tSeed, unsigned int  totalPizzas,unsigned int* selectedPizzaTypes)
{
    for(int i=0; i<totalPizzas;i++)
    {
        double randDecimal = (double)rand_r(&tSeed) / RAND_MAX;
        selectedPizzaTypes[i] = weightedProbability(randDecimal, &availablePizzaTypes,3);

        checkRCAndExitThread("pthread_mutex_lock",pthread_mutex_lock(&printMtx), id, selectedPizzaTypes);
        printf("Customer %d chose a %s\n",id, PizzaNames[selectedPizzaTypes[i]]);
        checkRCAndExitThread("pthread_mutex_unlock",pthread_mutex_unlock(&printMtx), id, selectedPizzaTypes);        
    }
}

unsigned int weightedProbability(float randDecimal,const float* probabilityArray,unsigned int size)
{
    for(int i=0; i<size; i++)
    {     
        if(randDecimal<probabilityArray[i])
        {
            return i;
        }
        else randDecimal-=probabilityArray[i];
    }
    return-1;
}

int main(int argc, char* argv[])
{
    int rc;
    int N = atoi(argv[1]);
    seed = atoi(argv[2]);  
    pthread_t* threads = malloc(N * sizeof(pthread_t));
    int * ids = (int*)malloc(N * sizeof(int));
    //elegxos an apetyxe i malloc
    if (ids == NULL) {
        printf("ERROR: Malloc failed not enough memory!\n");
        return -1;
    }
    pthread_mutex_init(&telOperatorMtx, NULL);
    pthread_cond_init(&telOperatorCond, NULL);
    pthread_mutex_init(&totalRevenueMtx, NULL);    
    pthread_mutex_init(&printMtx, NULL);

    for (int i = 0; i < N; i++)
    {
        ids[i] = i + 1;
        printf("Main: Thread Creation %d\n", i + 1);
        rc = pthread_create(&threads[i], NULL, customer, &ids[i]);
        if (rc != 0) 
        {
    		printf("ERROR: return code from pthread_create() is %d\n", rc);
       		exit(-1);
	    }
    }
    for (int i = 0; i < N; i++)
    {
        pthread_join(threads[i], NULL);
    }
  
    pthread_mutex_destroy(&telOperatorMtx);
    pthread_cond_destroy(&telOperatorCond);
    pthread_mutex_destroy(&totalRevenueMtx);
    pthread_mutex_destroy(&printMtx);
    free(ids);
    free(threads);
    return 0;
}