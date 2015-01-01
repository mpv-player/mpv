#ifndef MP_WRAP_SEMAPHORE_H_
#define MP_WRAP_SEMAPHORE_H_

#include <pthread.h>

// See pthread.h for rationale.
#define sem_init m_sem_init
#define sem_destroy m_sem_destroy
#define sem_wait m_sem_wait
#define sem_trywait m_sem_trywait
#define sem_timedwait m_sem_timedwait
#define sem_post m_sem_post

#define SEM_VALUE_MAX 100

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    unsigned int value;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);

#endif
