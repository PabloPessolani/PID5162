/****
 * file example.c
 *
   Program to demonstrate the thread management package *
   Written by Douglas Jones, Feb 18, 1998               *
                                                     ****/

#include <stdio.h>
#include "threads.h"

void test_thread( int n )
{
	printf( "A%d\n", n );
	if (n > 1) thread_launch( 4000, test_thread, n-1 );
	thread_relinquish();
	printf( "B%d\n", n );
	thread_relinquish();
	printf( "C%d\n", n );
	thread_relinquish();
}

int main()
{
	thread_manager_init();
	thread_startup_report(); /***********/
	thread_launch( 4000, test_thread, 5 );
	thread_manager_start();
	/* control never reaches this point */
}
