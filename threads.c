#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <semaphore.h>

#define MAX_BYTES 32767
#define MAX_THREADS 128
#define READY 1
#define RUNNING 2
#define EXITED 3
#define BLOCKED 4
#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3
#define JB_SP 4
#define JB_PC 5

// disable control operations while in critical section
int interrupts_disabled=0;
int exitcount = 0;
int threadcount = 0; //for pthread_create, number of threads
int isfirst = 1; //"boolean" for checking if first time running pthread_create
int count = 0; //for indexing threads
int qindex = 0; //for indexing through semaphore queue
int unitialized = -10; //for head and tail

struct Thread
{
  pthread_t id;
  int stackpointer;
  int PCpointer;
  unsigned long int * stack;
  int basic_state;
  int saved_state;
  jmp_buf jump_state;
  void * exit_state;
  int join_id;
};

struct Thread mythreads[MAX_THREADS];

struct Queue
{
  pthread_t array[MAX_THREADS];
};

struct semaphore
{
  int id;
  int val;
  int numthreads;
  int flag;
  int head;
  int tail;
  struct Queue * q;
};

struct semaphore mysems[MAX_THREADS];

void lock()
{
  sigset_t mysignal;
  sigemptyset(&mysignal);
  sigaddset(&mysignal,SIGALRM);
  sigprocmask(SIG_BLOCK, &mysignal, NULL);
}

void unlock()
{
  sigset_t mysignal;
  sigemptyset(&mysignal);
  sigaddset(&mysignal,SIGALRM);
  sigprocmask(SIG_UNBLOCK, &mysignal, NULL);
}

pthread_t pthread_self()
{
  return mythreads[count].id;
}

static int ptr_mangle(int p)
{
  unsigned int ret;
  asm(" movl %1, %%eax;\n"
      " xorl %%gs:0x18, %%eax;" " roll $0x9, %%eax;"
      " movl %%eax, %0;"
      : "=r"(ret)
      : "r"(p)
      : "%eax"
      );
  return ret;
}

void schedule()
{
  int prev = interrupts_disabled;
  interrupts_disabled = 1;
  if (prev) return; 

  if(setjmp(mythreads[count].jump_state) == 0)
    {
      if(isfirst == 1)
	{
	  count = 0;
	  isfirst = 0;
	}
      else
	{
	  int i;
	  for(i = 0; i < MAX_THREADS; i++)
	    {
	      count++;
	      if(count == MAX_THREADS)
		{
		  count = 0;
		}
	      if(mythreads[count].basic_state == RUNNING)
		{
		  interrupts_disabled = 0;
		  longjmp(mythreads[count].jump_state, 1);
		  break;
		}
	      if(mythreads[count].basic_state == READY)
		{
		  mythreads[count].jump_state[0].__jmpbuf[JB_SP] = mythreads[count].stackpointer;
		  mythreads[count].jump_state[0].__jmpbuf[JB_PC] = mythreads[count].PCpointer;
		  mythreads[count].basic_state = RUNNING;
		  interrupts_disabled = 0;
        

		  break;
		}
		//count++;
	    }
	}

      if(mythreads[count].basic_state == RUNNING)
	{
	  interrupts_disabled = 0;
	  //unlock();
	  longjmp(mythreads[count].jump_state, 1);
	}
      interrupts_disabled = 0;
    
    }
  else
    {

      unlock();
      /*sigset_t sigublock;
      sigemptyset(&sigublock);
      sigaddset(&sigublock, SIGALRM);
      sigprocmask(SIG_UNBLOCK, &sigublock, NULL);*/
    }
  interrupts_disabled = 0;

}

void start_timer()
{
  struct sigaction mysignal;
  memset (&mysignal, 0, sizeof(mysignal));
  mysignal.sa_sigaction = schedule;
  sigaction(SIGALRM, &mysignal, NULL);
  struct itimerval mytimer;
  mytimer.it_value.tv_usec = 50;
  mytimer.it_value.tv_sec = 0;
  mytimer.it_interval.tv_usec = 50000;
  mytimer.it_interval.tv_sec = 0;
  setitimer(ITIMER_REAL, &mytimer, NULL);
}


void pthread_exit(void *value_ptr)
{
  //lock();
  int *test = (int *)value_ptr;

  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  //threadcount--;
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  exitcount++;
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  mythreads[count].basic_state = EXITED;
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  mythreads[count].exit_state = value_ptr;
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  if(mythreads[count].join_id != unitialized && mythreads[mythreads[count].join_id].basic_state == BLOCKED)
    {
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
      mythreads[mythreads[count].join_id].basic_state = mythreads[mythreads[count].join_id].saved_state; //restores old state
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
    }
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  //unlock();
  schedule();
  printf("%s:%d value is %d\n", __func__, __LINE__, *test );
  
  __builtin_unreachable();
}

void pthread_exit_wrapper()
{
  unsigned int res;
  asm("movl %%eax, %0\n":"=r"(res)); pthread_exit((void *) res);
}

int pthread_create(pthread_t *thread, const pthread_attr_t  *attr, void *(*start_routine) (void *), void *arg) //add
{
  if(threadcount >= MAX_THREADS)
    {
      perror("ERROR");
      return -1;
    }
  else if(threadcount < MAX_THREADS)
    {
      if(threadcount == 0)
	{
	  *thread = (pthread_t)0;
	  mythreads[0].id = *thread;
	  mythreads[0].stack = malloc(MAX_BYTES);
	  mythreads[0].join_id = unitialized;
	  mythreads[0].stack[MAX_BYTES/4 - 1] = (unsigned long int)arg;
	  mythreads[0].stack[MAX_BYTES/4 - 2] = (unsigned long int)pthread_exit_wrapper;
	  mythreads[0].basic_state = RUNNING;
	  mythreads[0].exit_state = NULL;
	  setjmp(mythreads[0].jump_state);
	  mythreads[0].jump_state[0].__jmpbuf[JB_SP] = mythreads[0].stackpointer;
	  mythreads[0].jump_state[0].__jmpbuf[JB_PC] = mythreads[0].PCpointer;
	  start_timer(); 

	}
      threadcount++;


      *thread = (pthread_t)threadcount;
      mythreads[threadcount].id = *thread;
      mythreads[threadcount].join_id = unitialized;
      mythreads[threadcount].basic_state = READY;
      setjmp(mythreads[threadcount].jump_state);
      mythreads[threadcount].exit_state = NULL;

      mythreads[threadcount].stack = malloc(MAX_BYTES);

      mythreads[threadcount].stack[MAX_BYTES/4 - 1] = (unsigned long int)arg;
      mythreads[threadcount].stack[MAX_BYTES/4 - 2] = (unsigned long int)pthread_exit_wrapper;

      mythreads[threadcount].stackpointer = ptr_mangle((int)(mythreads[threadcount].stack + MAX_BYTES/4 - 2));
      mythreads[threadcount].PCpointer = ptr_mangle((int) start_routine);

    }
	unlock();


  return 2;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
  /* int curr = 0;
  while(mythreads[curr].id != thread && curr < MAX_THREADS) //finds current thread
    {
      curr++;
      }*/
  lock();
  int *test = (int *) *value_ptr;
  int *test1 = (int* )*test;
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
  value_ptr = &mythreads[thread].exit_state; //value ptr is the exit value of curr
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );

  if(mythreads[thread].basic_state != EXITED && mythreads[thread].join_id == unitialized && mythreads[thread].basic_state != 0) //if not exited
    {
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
      mythreads[count].saved_state = mythreads[count].basic_state; //saves old state
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
      mythreads[count].basic_state = BLOCKED; //blocks thread
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
      mythreads[thread].join_id = count;
  printf("%s:%d value is %d\n", __func__, __LINE__, (int)*test1 );
      /*int i;
      for(i = 0; i < MAX_THREADS; i++)
  {
    if(mythreads[curr].join_id == 0)
      {
        mythreads[curr].join_id = count; //saves count to the join id of curr
        break;
      }
      }*/
	unlock();
      schedule();
    }

  return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
  int curr;
  for(curr = 0; curr < MAX_THREADS; curr++) //finds next unitialized semaphore
    {
      if(mysems[curr].flag == 0)
	{
	  break;
	}
    }

  sem->__align = curr; //gives the id of sem to curr for indexing
  mysems[curr].id = curr;
  mysems[curr].flag = 1; //initalized
  mysems[curr].val = value;
  mysems[curr].q = malloc(sizeof(struct Queue)); //create queue

  mysems[curr].head = unitialized; //makes sure these aren't set to zero, causes indexing problems
  mysems[curr].tail = unitialized;

  return 0;
}

int sem_wait(sem_t *sem)
{
  int curr = sem->__align;

  if(mysems[curr].val > 0) //basic locking
    {
      mysems[curr].val--;
      qindex = count;
      lock();
      return 0;
    }
  
  while(mysems[curr].val == 0 && count != qindex) //if val is zero, but queue index is not synced with thread index
    {
      if(mysems[curr].head == unitialized && mysems[curr].tail == unitialized) //creates first threads if none mader yet
	{
	  mysems[curr].head = 0;
	  mysems[curr].tail = 0;
	  mysems[curr].q -> array[mysems[curr].tail] = mythreads[count].id;
	  mysems[curr].numthreads++; //new threads created
      
	}
      else if(mysems[curr].q -> array[mysems[curr].tail] != mythreads[count].id) //if the tail contents is not the current thread
	{
	  mysems[curr].tail++;
	  mysems[curr].q -> array[mysems[curr].tail] = mythreads[count].id;
	  mysems[curr].numthreads++;
	}

      mysems[curr].numthreads--; //decrement threads
    
      if(mysems[curr].numthreads != 0) //locking after adding threads
	{
	  mysems[curr].head = qindex;
	  mysems[curr].head++;
	  mysems[curr].numthreads++;
	  lock();
	  return 0;
	}

    }

  return 0;

}

int sem_post(sem_t *sem)
{
  int curr = sem->__align;
  if(mysems[curr].val > 0 && mysems[curr].id == curr) //if val > 0 and in correct semaphore
    {
      mysems[curr].val++; //basic unlocking
    }
  unlock();
  return 0;
}

int sem_destroy(sem_t *sem)
{
  int curr = sem->__align;
  if(mysems[curr].id == curr && mysems[curr].flag == 1 && mysems[curr].numthreads == 0) //if right sem, intialized, and no more threads waiting
    {
      mysems[curr].id = 0;
      mysems[curr].val = 0;
      mysems[curr].q = NULL;
      mysems[curr].flag = 0;
      mysems[curr].head = unitialized;
      mysems[curr].tail = unitialized;
    }

  return 0;
}
