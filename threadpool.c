#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

/**
 * creates threadpool
 * @param num_threads_in_pool
 * @return
 */
threadpool* create_threadpool(int num_threads_in_pool){
    if(num_threads_in_pool>MAXT_IN_POOL){
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        return NULL;
    }
    else if (num_threads_in_pool==0){
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        return NULL;
    }
    else if(num_threads_in_pool<0){
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        return NULL;
    }
    else{
        threadpool *tPool=(threadpool*)malloc(sizeof(threadpool));
        if (tPool==NULL){
            perror("error: malloc\n");
            exit(EXIT_FAILURE);
        }
        tPool->num_threads=num_threads_in_pool;
        tPool->qsize=0;
        tPool->threads=(pthread_t*) malloc(sizeof (pthread_t) * num_threads_in_pool);
        tPool->qhead=NULL;
        tPool->qtail=NULL;
        pthread_mutex_init(&tPool->qlock,NULL);
        pthread_cond_init(&tPool->q_not_empty,NULL);
        pthread_cond_init(&tPool->q_empty,NULL);
        tPool->shutdown=FALSE;
        tPool->dont_accept=FALSE;

        int ptCreate;
        int i;
        for(i=0 ; i<num_threads_in_pool ; i++){
            ptCreate = pthread_create(&tPool->threads[i], NULL, do_work,tPool);
            if(ptCreate){
                perror("error: pthread_create\n");
                exit(EXIT_FAILURE);
            }
        }
        return tPool;
    }
}
/**
 * dispatches a thread to do a work based on the do_work function
 * @param from_me
 * @param dispatch_to_here
 * @param arg
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if (from_me->dont_accept==TRUE){
        return;
    }
    work_t *work=(work_t*) malloc(sizeof (work_t));
    if (work==NULL){
        perror("error: malloc\n");
        exit(EXIT_FAILURE);
    }
    work->routine=dispatch_to_here;
    work->arg=arg;
    work->next=NULL;

    pthread_mutex_lock(&from_me->qlock);

    if (from_me->qhead==NULL){
        from_me->qhead=work;
        from_me->qtail=work;
    }
    else{
        from_me->qtail->next=work;
        from_me->qtail=from_me->qtail->next;
    }
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    pthread_cond_signal(&from_me->q_not_empty);
}
/**
 * a function that runs in a thread
 * @param p
 * @return
 */
void* do_work(void* p){

    threadpool *tmp= (threadpool*)p;
    while (1){
        pthread_mutex_lock(&tmp->qlock);
        if (tmp->shutdown==TRUE){
            pthread_mutex_unlock(&tmp->qlock);
            return NULL;
        }
        if (tmp->qsize==0){
            pthread_cond_wait(&tmp->q_not_empty,&tmp->qlock);
            if (tmp->shutdown==TRUE){
                pthread_mutex_unlock(&tmp->qlock);
                return NULL;
            }
        }
        work_t *node=tmp->qhead;
        if(node!=NULL){
            tmp->qsize--;
            if (tmp->qsize==0){
                tmp->qhead=NULL;
                tmp->qtail=NULL;
                if (tmp->dont_accept==TRUE){
                    pthread_cond_signal(&tmp->q_empty);
                }
            }
            else{
                if(tmp->qhead!=NULL){
                    tmp->qhead=tmp->qhead->next;
                }

            }
            pthread_mutex_unlock(&tmp->qlock);
        }
        if(node != NULL){
            node->routine(node->arg);
            free(node);
        }

    }
}
/**
 * destroys the previously created threadpool
 * @param destroyme
 */
void destroy_threadpool(threadpool* destroyme){
    destroyme->dont_accept=TRUE;
    pthread_mutex_lock(&destroyme->qlock);
    if(destroyme->qsize!=0){
        pthread_cond_wait(&destroyme->q_not_empty,&destroyme->qlock);
    }
    destroyme->shutdown=TRUE;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);


    int join;
    int i;
    for(i=0;i<destroyme->num_threads;i++){
        join= pthread_join(destroyme->threads[i],NULL);
        if ( join !=0){
            perror("error: pthread_join\n");
            exit(EXIT_FAILURE);
        }
    }
    free(destroyme->threads);
    free(destroyme);
}

