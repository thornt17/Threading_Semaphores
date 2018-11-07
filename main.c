#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

void * delayer(void * arg)
{
  int j;

  int *test;
  test = malloc(sizeof(int));

  *test = (int)arg;
  printf("test is %d\n", *test);
  for(j = 0; j < (unsigned long int)arg; j++)
    {

      if ((j % 1000) == 0) {} 

    }
  return test;
}
int main(int argc, char ** argv)
{
  pthread_t mythreads[20];
  unsigned long int rval[5];
  int count = 100000000;

  int i;
  for(i = 0; i < 5; i++) 
    {
      pthread_create(&mythreads[i], NULL, delayer, (void *)((i+1)*count));
      //printf("%d", pthread_self());
      //pthread_join(mythreads[i],NULL);
    }

  for(i = 0; i < 5; i++) 
    {
      //pthread_create(&mythreads[i], NULL, delayer, (void *)((i+1)*count));
     // printf("pthrerad id is %d", (int)mythreads[i]);
      pthread_join(mythreads[i],(void **)&rval[i]);
    }

  int j = 0;
  for(j = 0; j < 1000000000; j++)
    {

      if ((j % 1000) == 0){}

    }

  return 0;
}
