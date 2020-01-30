#include <sys/types.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "threadpool.h"

#include <pthread.h>

 void usageError();
 work_t* getFirstElement(threadpool* pool);
 void addElementToQueue(threadpool* pool, work_t* work);
void freeData(threadpool* pool);
void lock(threadpool* pool);
void unlock(threadpool* pool);
//char* getDate();

/*todo - 
* 1. exit only at main
* 2. lock mutex befor every wait - signal unlock wait
* 3. pat attention to unlock after every lock even when we return*/


/*int main(int argc, char const *argv[])
{
    if(atoi(argv[1]) == 0 || atoi(argv[2]) == 0 || argc > 3)
    {
       usageError();
       exit(1);
    }
    int arr[100000];
    for(int i = 0 ; i < 100000 ; i++)
        arr[i] = i;
    threadpool* t = create_threadpool(atoi(argv[1]));
    if(t == NULL)
    {
        printf("error trying to allocate\n");
        exit(1);
    }
    int i ;
    for( i = 0 ; i < atoi(argv[2]) ; i++)
        dispatch(t,f1,(void*)&arr[i]);
   // printPool(t);
    destroy_threadpool(t);
    
    return 0;

}*/

void freeData(threadpool* pool)
{

    pthread_cond_destroy(&pool->q_empty);
    pthread_cond_destroy(&pool->q_not_empty);
    pthread_mutex_destroy(&pool->qlock);
    free(pool->threads);
    free(pool);

    
}

threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
    {
        usageError();
        return NULL;
    }

    threadpool* t = (threadpool*)calloc(sizeof(threadpool), sizeof(threadpool) );
    if(t == NULL)
    {
        printf("error trying to allocate pool\n");
        return NULL;
    }
    
    t->num_threads = num_threads_in_pool;
    t->qsize = 0;
    t->qhead = NULL;
    t->qtail = NULL;
    t->shutdown = 0;
    t->dont_accept = 0;
    pthread_mutex_init(&t->qlock, NULL);
    pthread_cond_init(&t->q_not_empty, NULL);
    pthread_cond_init(&t->q_empty, NULL);
    t->shutdown = 0;       
    t->dont_accept = 0; 

    t->threads =(pthread_t*)calloc(sizeof(pthread_t)*num_threads_in_pool,sizeof(pthread_t));
    if(t->threads == NULL)
    {
        printf("error trying to allocate threads\n");
        pthread_cond_destroy(&t->q_empty);
        pthread_cond_destroy(&t->q_not_empty);
        pthread_mutex_destroy(&t->qlock);  
        free(t);
        return NULL;///fix - return null
    }

    for(int i = 0; i < num_threads_in_pool; i++)
    {
        if(pthread_create(&t->threads[i], NULL, do_work, (void*)t) != 0){
            perror("pthread_create\n");
            freeData(t);
            return NULL;
        }
    }
    return t;


}

 void usageError()
 {
     printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
 }

 void* do_work(void* p)
 {
    threadpool *t = (threadpool*)p;

    while(1)
    {        

       // printf("do_work()- After lock\n");
        lock(t);
        //if destroy process has begun
        if(t->shutdown == 1){
           // printf("exiting thread from 1\n");
            unlock(t);
            return NULL;
        }

  

        //wait for job
        if(t->qsize == 0){
            pthread_cond_wait(&t->q_not_empty,&t->qlock);
        }
        
        //if destruction woke me up
        if(t->shutdown == 1){
            unlock(t);
            return NULL;
        }

        work_t* qhead = getFirstElement(t);

        if(qhead == NULL)
        {
            unlock(t);
            continue;
        }
    

        if(t->qsize == 0 && t->dont_accept == 1){
//            unlock(t);
            if(pthread_cond_signal(&t->q_empty) != 0 )
            {
                perror("pthread_cond_signal\n");
                freeData(t);
                exit(1);
            }
        }
        //printPool(t);
        unlock(t);

        qhead->routine(qhead->arg);
        free(qhead);


    }
 }

 work_t* getFirstElement(threadpool* pool)
 {

    work_t* temp =  pool->qhead;
    if(pool->qhead != NULL)
        pool->qhead = (pool->qhead)->next;

    pool->qsize--;

    return temp;
 }

 void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
 {
     if(from_me->dont_accept != 1)
     {
        lock(from_me);
        
        work_t* work = (work_t*)calloc(sizeof(work_t),sizeof(work_t));
        if(work == NULL)
        {
            unlock(from_me);
            printf("error tryig to allocate work_t at dispatch\n");
            freeData(from_me);
            exit(1);
        }
        //printf("arg1 = %d\n",*(int*)arg);
        work->arg = arg;
       // printf("arg2 = %d\n",*(int*)work->arg);
        work->routine = dispatch_to_here;
        work->next = NULL;
        
        //printPool(from_me);
        addElementToQueue(from_me, work);
       // printf("arg3 = %d\n",*(int*)work->arg);
       // printPool(from_me);

        if(pthread_cond_signal(&from_me->q_not_empty) != 0 )
        {
            perror("pthread_cond_signal\n");
            freeData(from_me);
            exit(1);
        }

       
        unlock(from_me);
     }
 }

 void addElementToQueue(threadpool* pool, work_t* work)
 {
     
    if(pool-> qhead == NULL) 
    {
        pool-> qhead = work;
        pool-> qtail = work;
   }
    else 
    {
        pool-> qtail-> next = work; 
        pool-> qtail = (pool-> qtail)-> next;  
   }

    pool->qsize++;

 }

void destroy_threadpool(threadpool* destroyme)
{
    printf("in destroy\n");

    lock(destroyme);

    //dont add anymore jobs
    destroyme->dont_accept=1;

    //wait for all jobs to end
    if(destroyme->qsize > 0 )
    {
        if(pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock) != 0)
        {
            perror("pthread_cond_wait\n");
            freeData(destroyme);
            exit(1);
        }

    }


    printf("destroy finished waiting for queue to become empty\n");

    destroyme->shutdown =1;

    //wake threads waiting in do_work()
    if(pthread_cond_broadcast(&destroyme->q_not_empty) != 0 )
    {
        perror("pthread_cond_broadcast\n");
        freeData(destroyme);
        exit(1);
    }


    printf("destroy woke every one up\n");

    unlock(destroyme);

   // pthread_cond_signal(&destroyme->q_not_empty);
    for(int i=0; i<destroyme->num_threads; i++) {
       if(pthread_join(destroyme->threads[i], NULL) != 0)
       {
            perror("pthread_create\n");
            freeData(destroyme);
            exit(1);
       }
    }

    printf("end of destroy\n");

    
    freeData(destroyme);


}

void lock(threadpool* pool)
{
    if(pthread_mutex_lock(&pool->qlock) != 0)
    {
        perror("mutex\n");
        freeData(pool);
        exit(1);
    }
}

void unlock(threadpool* pool)
{
    if(pthread_mutex_unlock(&pool->qlock) != 0)
    {
        perror("mutex\n");
        freeData(pool);
        exit(1);
    }
}

