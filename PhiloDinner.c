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

/*
 * Defina FORK para que execute a versao que utiliza processos pesados(fork) da implementacao do jantar dos filosofos
 * Defina THREAD para que execute a versao que utiliza processos leves(threads) da implementacao do jantar dos filosofos
 */

//const char* names[5] = {"Platao","Bacon","Nietzsche","Kant","Aristoteles"};
void start_data_usage();
#define FORK
//#define THREAD

#ifdef FORK
#define N 5
#define LEFT (i+(N-1))%N
#define RIGHT (i+1)%N
#define THINKING 0
#define HUNGRY 1
#define EATING 2

struct shared_data { // porcao da memoria responsavel por determinar acessos, ela deve ser compartilhada entre os processosa traves da mmap(..)
    sem_t spoon; // regiao critica/mutex
    sem_t phil[N]; // mutex de cada filosofo
    int state[N]; // estado de cada filosofo
};

struct shared_data *shared; // memoria a ser compartilhada

void initialize_shared(); // inicia a memoria compartilhada
void finalize_shared();   // finaliza a memoria compartilhada

void philosopher(int id); // funcao do filosofo recebe o "ID" dele
void test(int id); // teste para saber se ele pode ou nao comer
void take_spoon(int id); // tenta pegar o garfo caso nao tenha ninguem comendo ao seu lado
void put_spoon(int id); // deixa os garfos na mesa depois que terminou de comer
void thinking(int id);
void eating(int id);
void get_current_cpu_usage();
void get_current_memory_usage();
struct rusage r_usage;
long double a[4], b[4], loadavg;
FILE* fp;


int main(void)
{
	clock_t t; 
    t = clock(); 
    
	printf("Starting Forks...\n");
	printf("CPU/Memory info before creating shared memory space: ");
	get_current_cpu_usage();
	printf("\n");
	getrusage(RUSAGE_CHILDREN,&r_usage);
	printf("Maximum resident set size: %ld KB\n",r_usage.ru_maxrss);
	printf("Shared memory size: %ld KB\n",sizeof(*shared));
	printf("Unshared memory size: %ld KB\n",r_usage.ru_maxrss);
	printf("Voluntary contex switches: %ld \n",r_usage.ru_nvcsw);
	printf("Involuntary contex switches: %ld \n",r_usage.ru_nivcsw);
	printf("\n");
    
    int i;
    pid_t pid, pids[N]; // process ids
    initialize_shared();
    for(i=0;i<N;++i)
    {
        pid = fork();
        if(pid==0)
        {
            // child
            philosopher(i);
            _exit(0);
        }
        else if(pid>0)
        {
            // parent
            pids[i] = pid;
            printf("pids[%d]=%d\n",i,pids[i]);
        }
        else
        {
            perror("fork");
            _exit(0);
        }
    }
    // wait for child processes to end
    for(i=0;i<N;++i) waitpid(pids[i],NULL,0);
	t = clock() - t; 
	printf("CPU/Memory info after all forks are done but memory still set...");
	get_current_cpu_usage();
	printf("\n");
	getrusage(RUSAGE_CHILDREN,&r_usage);
	printf("Maximum resident set size: %ld KB\n",r_usage.ru_maxrss);
	printf("Shared memory size: %ld KB\n",r_usage.ru_maxrss);
	printf("Unshared memory size: %ld KB\n",r_usage.ru_ixrss);
	printf("Voluntary contex switches: %ld \n",r_usage.ru_nvcsw);
	printf("Involuntary contex switches: %ld \n",r_usage.ru_nivcsw);
	printf("\n");
	
    finalize_shared();
	
	printf("CPU/Memory info after all forks are done and memory unset...");
	get_current_cpu_usage();
	printf("\n");
	getrusage(RUSAGE_CHILDREN,&r_usage);
	printf("Maximum resident set size: %ld KB\n",r_usage.ru_maxrss);
	printf("Shared memory size: %ld KB\n",r_usage.ru_maxrss);
	printf("Unshared memory size: %ld KB\n",r_usage.ru_ixrss);
	printf("Voluntary contex switches: %ld \n",r_usage.ru_nvcsw);
	printf("Involuntary contex switches: %ld \n",r_usage.ru_nivcsw);
	printf("\n");
	printf("CPU time used: %d \n",r_usage.ru_stime.tv_sec);
    double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
	printf("Took %f seconds to execute \n", time_taken); 
    return 0;
}



void philosopher(int i) // abstracao do filosofo
{
    for(int x=0;x<10;x++)
    {
        thinking(i);
        take_spoon(i);
        eating(i);
        put_spoon(i);
        sleep(1);
    }
}

void thinking(int i){

    printf("philosopher %d is thinking...",i+1);
    sleep(1);

}

void eating(int i){

    printf("philosopher %d is eating...",i+1);
	get_current_cpu_usage();
	printf("\n");
	get_current_memory_usage();
    sleep(2);

}


void take_spoon(int i)
{
    sem_wait(&shared->spoon);
    shared->state[i] = HUNGRY;
    printf("philosopher %d is hungry\n",i+1);
    test(i);
    sem_post(&shared->spoon);
    sem_wait(&shared->phil[i]);
}

void put_spoon(int i)
{
    sem_wait(&shared->spoon);
    shared->state[i] = THINKING;
    printf("philosopher %d puts down spoon %d and %d hin\n",i+1,LEFT+1,i+1);
    printf("philosopher %d thinks\n",i+1);
    test(LEFT);
    test(RIGHT);
    sem_post(&shared->spoon);
}

void test(int i) // teste commum do jantar dos filosofos
{
    if( shared->state[i] == HUNGRY && shared->state[LEFT] != EATING && shared->state[RIGHT] != EATING) // se os garfos a esquerda e a direita de i nao estao sendo usados e eu estou com fome
    {
        shared->state[i] = EATING; // muda estado apra comendo
        printf("philosopher %d takes spoon %d and %d\n",i+1,LEFT+1,i+1);
        printf("philosopher  %d eats\n",i+1);
        sem_post(&shared->phil[i]);
    }
}


void initialize_shared() // cria memora compartilhada entre processos pesados
{
    int i;
    int prot=(PROT_READ|PROT_WRITE); // representa que ele pode ser ora lido ora escrito
    int flags=(MAP_SHARED|MAP_ANONYMOUS); // representa que eh uma memoria compartilhada anonima
    /*
     * Vantagens de se ter memoria compartilhada anonima
     *  ela e devolvida ao sistema apos o uso, logo nao existe leaks
     *  o tamanho delas eh dinamico, o acesso a elas pode mudar a qualquer momento
     *  sao alocadas de maneira distinta ao heap global do processo
     *  Desvantagens
     *  como o tamanho nao e limitado, pode ser necessario muitos page framos do sistema para representa-la
     *  criar e retornar valores geram um overhead maior
     */
    shared=mmap(0,sizeof(*shared),prot,flags,-1,0); // funcao para criar a memoria compartilhada
    /*
     *
     * no nosso caso, passamos o tamanho do conteudo do vetor de memoria estatico no nosso programa inicial
     * apartir dele os processos subsequentes, os filosofos, vao buscar nessa memoria suas respesctivas permissoes de acesso
     *
     */
    memset(shared,'\0',sizeof(*shared));
    sem_init(&shared->spoon,1,1);
    for(i=0;i<N;++i) sem_init(&shared->phil[i],1,1);
}

void finalize_shared()
{
    int i;
    for(i=0;i<N;++i) sem_destroy(&shared->phil[i]); // destroi os semaforos
    munmap(shared, sizeof(*shared)); // destroi a memoria compartilhada
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
