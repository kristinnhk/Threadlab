#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * User 1: Kári Mímisson
 * SSN: 1711922859
 * User 2: Hörður Ragnarsson
 * SSN:  
 * === End User Information ===
 ********************************************************/

struct chairs
{
    struct customer **customer; /* Array of customers */
    int max, f, r; /* Added f and r for que */
	sem_t chair;
	sem_t mutex;
	sem_t barber;
};

struct barber
{
    int room;
    struct simulator *simulator;
};

struct simulator
{
    struct chairs chairs;
    
    pthread_t *barberThread;
    struct barber **barber;
};

static void *barber_work(void *arg)
{
    struct barber *barber = arg;
    struct chairs *chairs = &barber->simulator->chairs;
    struct customer *customer = 0; 

    /* Main barber loop */
    while (true) {
		sem_wait(&chairs->barber); 
    
		sem_wait(&chairs->mutex);
        /* Incrementing rear by one and modulo max to return back to 0 if
        * the que is at max index */
		customer = chairs->customer[chairs->r++ % chairs->max]; 
        thrlab_prepare_customer(customer, barber->room);
		sem_post(&chairs->mutex);
		sem_post(&chairs->chair);
		
        thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
        thrlab_dismiss_customer(customer, barber->room);
		sem_post(&customer->mutex);
    }
    return NULL;
}


/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
    struct chairs *chairs = &simulator->chairs;
    /* Setup semaphores*/
    chairs->max = thrlab_get_num_chairs();
    /* Front and Rear of que both begin and index 0. */
    chairs->f = 0;
    chairs->r = 0;
 
	sem_init(&chairs->mutex, 0, 1);
	sem_init(&chairs->chair, 0, chairs->max);
	sem_init(&chairs->barber, 0, 0);
	
    /* Create chairs*/
    chairs->customer = malloc(sizeof(struct customer *) * thrlab_get_num_chairs());

    /* Create barber thread data */
    simulator->barberThread = malloc(sizeof(pthread_t) * thrlab_get_num_barbers());
    simulator->barber = malloc(sizeof(struct barber*) * thrlab_get_num_barbers());

    /* Start barber threads */
    struct barber *barber;
    for (unsigned int i = 0; i < thrlab_get_num_barbers(); i++) {
	barber = calloc(sizeof(struct barber), 1);
	barber->room = i;
	barber->simulator = simulator;
	simulator->barber[i] = barber;
	pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
	pthread_detach(simulator->barberThread[i]);
    }
}

/**
 * Free all used resources and end the barber threads.
 */
static void cleanup(struct simulator *simulator)
{
    /* Free chairs */
    free(simulator->chairs.customer);

    /* Free barber thread data */
    free(simulator->barber);
    free(simulator->barberThread);
}

/**
 * Called in a new thread each time a customer has arrived.
 */
static void customer_arrived(struct customer *customer, void *arg)
{
    struct simulator *simulator = arg;
    struct chairs *chairs = &simulator->chairs;

    sem_init(&customer->mutex, 0, 0);

    /* If a chair is available we let the customer in, else we turn him
     * away. */
    if (sem_trywait(&chairs->chair) == 0) {

        sem_wait(&chairs->mutex);
        thrlab_accept_customer(customer);
        /* Incrementing front by one and modulo max to return back to 0 if
         * the que is at max index. */
        chairs->customer[chairs->f++ % chairs->max] = customer;
        sem_post(&chairs->mutex);
        sem_post(&chairs->barber);

        sem_wait(&customer->mutex);
        return;
    } else {
        thrlab_reject_customer(customer);
    }
}


int main (int argc, char **argv)
{
    struct simulator simulator;

    thrlab_setup(&argc, &argv);
    setup(&simulator);

    thrlab_wait_for_customers(customer_arrived, &simulator);

    thrlab_cleanup();
    cleanup(&simulator);

    return EXIT_SUCCESS;
}
