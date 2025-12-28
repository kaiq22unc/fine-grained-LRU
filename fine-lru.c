/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* @* Place your name here, and any other comments *@
 * @* that deanonymize your work inside this syntax *@
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "lru.h"

/* Define the simple, singly-linked list we are going to use for tracking lru */
struct list_node {
    struct list_node* next;
    int key;
    int refcount;
    // Protects this node's contents
    pthread_mutex_t mutex;
};

static struct list_node* list_head = NULL;

/* A static mutex; protects the count and head.
 * XXX: We will have to tolerate some lag in updating the count to avoid
 * deadlock. */
static pthread_mutex_t mutex;
static int count = 0;
static pthread_cond_t cv_low, cv_high;

static volatile int done = 0;

/* Initialize the mutex. */
int init (int numthreads) {
    /* Your code here */

    /* Temporary code to suppress compiler warnings about unused variables.
     * You should remove the following lines once this file is implemented.
     */
        /*(void)list_head;
    (void)mutex;
    (void)count;
    (void)cv_low;
    (void)cv_high;*/
    /* End temporary code */
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        return 0;
    }

    /* Initialize other data structures if needed. */
    pthread_cond_init(&cv_low, NULL);
    pthread_cond_init(&cv_high, NULL);

    return 0;
}

/* Return 1 on success, 0 on failure.
 * Should set the reference count up by one if found; add if not.*/
int reference (int key) {
    /* Your code here */
    pthread_mutex_lock(&mutex); //hold global lock until traverse past list_head


    while (count >= HIGH_WATER_MARK) {
        pthread_cond_wait(&cv_high, &mutex);
    }

    if(done == 1){
        pthread_cond_signal(&cv_high);
        pthread_cond_signal(&cv_low);
        pthread_mutex_unlock(&mutex);
        return 1;
    }

    int lock = 1; //track if the thread has the global lock
    int found = 0;
    int added = 0;
    struct list_node* cursor = list_head;
    struct list_node* last = NULL;

    //acquire cursor lock if cursor is not NULL
    if(cursor){
        pthread_mutex_lock(&cursor->mutex);
    }


    while(cursor) {
        if (cursor->key < key) {
            if(last){
                pthread_mutex_unlock(&last->mutex);
                //unlock global lock once we passed list_head
                if(lock && last!=list_head){
                    pthread_mutex_unlock(&mutex);
                    lock = 0;
                }
            }
            last = cursor;
            cursor = cursor->next;
            if(cursor){
                pthread_mutex_lock(&cursor->mutex);
            }
        } else {
            if (cursor->key == key) {
                cursor->refcount++;
                found++;
                //if (last != NULL) {
                    //pthread_mutex_unlock(&(last->mutex));
                //}
            }
            break;
        }
    }

    if (!found) {
        // Handle 2 cases: the list is empty/we are trying to put this at the
        // front, and we want to insert somewhere in the middle or end of the
        // list.
        //pthread_mutex_unlock(&mutex);

        struct list_node* new_node = malloc(sizeof(struct list_node));
        if (!new_node) {
            //pthread_mutex_unlock(&cursor->mutex);
            if(lock) pthread_mutex_unlock(&mutex);
            return 0;
        }
        //count++;
        added = 1; //indicate that new node added
        new_node->key = key;
        new_node->refcount = 1;

        new_node->next = cursor;

        // initialize new  node mutex
        pthread_mutex_init(&new_node->mutex, NULL);


        if (last == NULL){
            list_head = new_node;
            if(lock){
            pthread_mutex_unlock(&mutex); //release global lock after finising modifying list_head
            lock = 0; 
            }
        }
        else{
            last->next = new_node;
            //pthread_mutex_unlock(&(last->mutex));
        }
        /*if(lock){
            pthread_mutex_unlock(&mutex);
            lock = 0;
        }*/
    }

    if(cursor){
        pthread_mutex_unlock(&cursor->mutex);
    }
    if(last){
        pthread_mutex_unlock(&last->mutex);
    }

    if(!lock) pthread_mutex_lock(&mutex); //acquire global lock before cv and updating count

    if(added) count++;

    if(count > LOW_WATER_MARK){
        pthread_cond_signal(&cv_low);
    }

    pthread_mutex_unlock(&mutex);
    return 1;
}

/* Do a pass through all elements, either decrement the reference count,
 * or remove if it hasn't been referenced since last cleaning pass.
 *
 * check_water_mark: If 1, block until there are more elements in the cache
 * than the LOW_WATER_MARK.  This should only be 0 during self-testing or in
 * single-threaded mode.
 */
void clean(int check_water_mark) {
    /* Your code here */
    pthread_mutex_lock(&mutex); //hold global lock until trasvese passed list_head

    while (check_water_mark && count <= LOW_WATER_MARK) {
        pthread_cond_wait(&cv_low, &mutex);
    }

    if(done == 1){
        pthread_cond_signal(&cv_high);
        pthread_cond_signal(&cv_low);
        pthread_mutex_unlock(&mutex);
        return;
    }

    int lock = 1;
    int removed = 0;
    struct list_node* last = NULL;
    struct list_node* cursor = list_head;

    if(cursor){
        pthread_mutex_lock(&cursor->mutex);
    }


    while(cursor) {
        cursor->refcount--;
        if (cursor->refcount == 0) {
            struct list_node* tmp = cursor;
            if (last) {
                last->next = cursor->next;
                 if(lock && last!=list_head){
                     pthread_mutex_unlock(&mutex);
                     lock = 0;
                 }
            } else {
                list_head = cursor->next;
            }
            tmp = cursor->next;
            free(cursor);
            cursor = tmp;
            if(cursor){
                pthread_mutex_lock(&cursor->mutex);
            }
            //count--;
            removed++; //track the number of nodes removed
        } else {
            if(last){
                pthread_mutex_unlock(&last->mutex);
            }
            last = cursor;
            cursor = cursor->next;
            if(cursor){
                pthread_mutex_lock(&cursor->mutex);
            }
        }
    }

    if(cursor){
        pthread_mutex_unlock(&cursor->mutex);
    }
    if(last){
        pthread_mutex_unlock(&last->mutex);
    }

    if(!lock){
        pthread_mutex_lock(&mutex);
        lock = 0;
    }

    if(removed) count-=removed;

    if (count < HIGH_WATER_MARK){
        pthread_cond_signal(&cv_high);
    }
    pthread_mutex_unlock(&mutex);
}


/* Optional shut-down routine to wake up blocked threads.
   May not be required. */
void shutdown_threads (void) {
    /* Your code here */
    pthread_mutex_lock(&mutex);
    done = 1;
    // pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&cv_low);
    pthread_cond_broadcast(&cv_high);
    pthread_mutex_unlock(&mutex);
    return;
}

/* Print the contents of the list.  Mostly useful for debugging. */
void print (void) {
    /* Your code here */
    int lock = 1;

    pthread_mutex_lock(&mutex);
    printf("=== Starting list print ===\n");
    printf("=== Total count is %d ===\n", count);
    struct list_node* cursor = list_head;

    if(cursor){
        pthread_mutex_lock(&cursor->mutex);
    }

    while (cursor) {
        printf("Key %d, Ref Count %d\n", cursor->key, cursor->refcount);
        if(cursor->next){
            pthread_mutex_lock(&cursor->next->mutex);
        }
        pthread_mutex_unlock(&cursor->mutex);
        cursor = cursor->next;
        if(lock){
            pthread_mutex_unlock(&mutex);
            lock = 0;
        }
    }

    if(lock){
        pthread_mutex_unlock(&mutex);
    }

    if(cursor){
        pthread_mutex_unlock(&cursor->mutex);
    }
    printf("=== Ending list print ===\n");
    //pthread_mutex_unlock(&mutex);

}
