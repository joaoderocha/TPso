#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "string.h"
#include "sys/times.h"

#define THREAD

#ifdef THREAD

#define N 5
#define PENSAR 0
#define FOME 1
#define COMER 2
#define ESQUERDA (nfilosofo+4)%N //agarrar garfo
//da esquerda
#define DIREITA (nfilosofo+1)%N  //agarrar garfo
//da direita
void *filosofo(void *num);
void take_spoon(int);
void put_spoon(int);
void testar(int);
void eating(int );
void thinking(int i);
void get_current_cpu_usage();
void get_current_memory_usage();
struct rusage r_usage;
long double a[4], b[4], loadavg;
FILE* fp; 
sem_t mutex;
sem_t S[N]; //inicializacao do semáforo
int estado[N];
int nfilosofo[N]={0,1,2,3,4};

void *filosofo(void *num)
{
    for(int x=0;x<10;x++)
    {
        int *i = num;
        thinking(*i);
        take_spoon(*i);
        eating(*i);
        put_spoon(*i);
    }
}

void thinking(int i){

    printf("philosopher %d is thinking in the cpu usage...\n",i+1);
    sleep(1);

}

void eating(int i){

    printf("philosopher %d is eating...\n",i+1);
	get_current_cpu_usage();
	printf("\n");
	get_current_memory_usage();
    sleep(2);

}

void take_spoon(int nfilosofo)
{
    sem_wait(&mutex);
    estado[nfilosofo] = FOME;
    printf("Filosofo %d tem fome.\n", nfilosofo+1);
    //+1 para imprimir filosofo 1 e nao filosofo 0
    testar(nfilosofo);
    sem_post(&mutex);
    sem_wait(&S[nfilosofo]);
    sleep(1);
}

void put_spoon(int nfilosofo)
{
    sem_wait(&mutex);
    estado[nfilosofo]=PENSAR;
    printf("Filosofo %d deixou os garfos %d e %d.\n", nfilosofo+1, ESQUERDA+1, nfilosofo+1);
    printf("Filosofo %d esta a pensar.\n", nfilosofo+1);
    testar(ESQUERDA);
    testar(DIREITA);
    sem_post(&mutex);
}

void testar(int nfilosofo)
{
    if(estado[nfilosofo]==FOME && estado[ESQUERDA]
                                  !=COMER && estado[DIREITA]!=COMER)
    {
        estado[nfilosofo]=COMER;
        sleep(2);
        printf("Filosofo %d agarrou os garfos %d e %d.\n", nfilosofo+1, ESQUERDA+1, nfilosofo+1);
        printf("Filosofo %d esta a comer.\n", nfilosofo+1);
        sem_post(&S[nfilosofo]);
    }
}

int main() {
	int i;	
	clock_t t; 
    t = clock(); 
    
	printf("Starting Threads...\n");
	get_current_cpu_usage();
	printf("\n");
	get_current_memory_usage();
	printf("\n");
    pthread_t thread_id[N]; //identificadores das
    //threads
    sem_init(&mutex,0,1);
    for(i=0;i<N;i++)
        sem_init(&S[i],0,0);
    for(i=0;i<N;i++)
    {
        pthread_create(&thread_id[i], NULL, filosofo, &nfilosofo[i]);
        //criar as threads
        printf("Filosofo %d esta a pensar.\n",i+1);
    }
    for(i=0;i<N;i++)
        pthread_join(thread_id[i],NULL); //para
    //fazer a junção das threads
	
	t = clock() - t; 
    double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
	printf("Took %f seconds to execute \n", time_taken); 
	
    return(0);
}

void get_current_cpu_usage(){
	fp = fopen( "/proc/self/status", "r" );
	fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
    fclose(fp);
	sleep(1);
	fp = fopen("/proc/stat","r");
    fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
    fclose(fp);
    loadavg = ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
	loadavg *= 10;
    printf("The current CPU utilization is : %Lf%%",loadavg);
}

void get_current_memory_usage(){
	
	getrusage(RUSAGE_SELF,&r_usage);
	printf("Maximum resident set size: %ld KB\n",r_usage.ru_maxrss);
	printf("Shared memory size: %ld KB\n",r_usage.ru_maxrss);
	printf("Unshared memory size: %ld KB\n",r_usage.ru_ixrss);
	printf("Voluntary contex switches: %ld \n",r_usage.ru_nvcsw);
	printf("Involuntary contex switches: %ld \n",r_usage.ru_nivcsw);
	
}
#endif
