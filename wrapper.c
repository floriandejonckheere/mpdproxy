#include <stdio.h>
#include <string.h>

#include <pthread.h>

void __real_pthread_exit(int*);

void
__wrap_pthread_exit(void *retval)
{
	printf("__pthread_exit %d\n", (int) pthread_self());
	__real_pthread_exit(retval);
}
