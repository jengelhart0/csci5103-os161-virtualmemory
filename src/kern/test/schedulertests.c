/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Thread test code.
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <synch.h>
#include <test.h>

#define NTHREADS  18

static struct semaphore *tsem = NULL;
static int useLetters = 'a' - '0';
static int useUpperLetters = 'A' - '0';

static
void
init_sem(void)
{
	if (tsem==NULL) {
		tsem = sem_create("tsem", 0);
		if (tsem == NULL) {
			panic("threadtest: sem_create failed\n");
		}
	}
}

static
void
loudthread(void *junk, unsigned long num)
{
	int ch = '0' + num;
	int i;

	(void)junk;

	for (i=0; i<120; i++) {
		putch(ch);
		for (int j=0; j<100000; j++); // pause a bit while printing
	}
	V(tsem);
}

/*
 * The idea with this is that you should see
 *
 *   01234567 <pause> 01234567
 *
 * (possibly with the numbers in different orders)
 *
 * The delay loop is supposed to be long enough that it should be clear
 * if either timeslicing or the scheduler is not working right.
 */
static
void
quietthread(void *junk, unsigned long num)
{
	int ch = '0' + num;
	volatile int i;

	(void)junk;

	putch(ch);
	for (i=0; i<900000; i++);
	putch(ch);

	V(tsem);
}

static
void
runthreads_priority(int doloud, int priority1threadcount, int starvethreadcount)
{
	char name[16];
	int i = 0, result;

	for (; i<NTHREADS-priority1threadcount-starvethreadcount; i++) {
		snprintf(name, sizeof(name), "threadtest%d", i);
		result = thread_fork(name, 15, NULL,
				     doloud ? loudthread : quietthread,
				     NULL, useUpperLetters + i);
		if (result) {
			panic("threadtest: thread_fork failed %s)\n",
			      strerror(result));
		}
	}

	for (; i<NTHREADS-starvethreadcount; i++) {
		snprintf(name, sizeof(name), "threadtestP%d", i);
		result = thread_fork(name, 1, NULL,
						 doloud ? loudthread : quietthread,
						 NULL, useUpperLetters + i);
		if (result) {
			panic("threadtest: thread_fork failed %s)\n",
						strerror(result));
		}
	}

	for (; i<NTHREADS; i++) {
		snprintf(name, sizeof(name), "threadtestS%d", i);
		result = thread_fork(name, 30, NULL,
						 doloud ? loudthread : quietthread,
						 NULL, useUpperLetters + i);
		if (result) {
			panic("threadtest: thread_fork failed %s)\n",
						strerror(result));
		}
	}

	for (i=0; i<NTHREADS; i++) {
		P(tsem);
	}
}

// expect high priority threads to have sole control of the cpu until
// lower priority thread ages out to a higher priority.
// remember we don't reschedule everytime just every 4th thead_yield (hardclock)
int age_priority_test(int nargs, char **args)
{
	(void)nargs;
	(void)args;

	init_sem();
	kprintf("Starting aging test...\n");
	kprintf("Aging 'a' among 5 higher priority 'H' threads\n");
	//runthreads_priority(1, NTHREADS - 1, 0);
	char name[16];
	int result, idx = 0;
	int highPriority = 'H' - '0';
	int agingThread = 'a' - '0';

	for (; idx < 5; idx++) {
		snprintf(name, sizeof(name), "threadtest%d", idx);
		result = thread_fork(name, 1, NULL,
						 loudthread,
						 NULL, highPriority);
		if (result) {
			panic("threadtest: thread_fork failed %s)\n",
						strerror(result));
		}
	}

  snprintf(name, sizeof(name), "threadtest%d", idx);
  result = thread_fork(name, 10, NULL,
				 loudthread,
				 NULL, agingThread);
  if (result) {
	  panic("threadtest: thread_fork failed %s)\n",
				strerror(result));
  }

	for (int idx2 = 0; idx2 <= idx; idx2++) {
		P(tsem);
	}

	kprintf("\nAging test done.\n");

	return 0;
}

// this will put 1 thread on each queue.
// Expect the first thread to run for a while until
// thread2 and thread3 age up and are promoted to queue1
int priority_queue_test(int nargs, char **args)
{
	(void)nargs;
	(void)args;

	init_sem();
	kprintf("Starting queue test...\n");
	char name[16];
	int result;

	for(int i = 0; i < 3; i++){
    snprintf(name, sizeof(name), "threadtest%d", i);
    result = thread_fork(name, i * 10, NULL,
				 loudthread,
				 NULL, i + useLetters);
    if (result) {
	    panic("threadtest: thread_fork failed %s)\n",
				strerror(result));
    }
	}

	for(int i = 0; i < 3; i++){
		P(tsem);
	}
	kprintf("\nQueue test done.\n");

	return 0;
}

/*
* first test: A lot of 'R's until the aging catches up
* second test: 'O', 'P', 'Q', and 'R' are prioritized at begining
*/
int thread_schedule_test(int nargs, char **args)
{
	(void)nargs;
	(void)args;
	int last = 'A' + NTHREADS - 1;

	init_sem();
	kprintf("Starting 1 prioritized schedule test...\n");
	kprintf("'%c' should move to the top priority quickly until others age\n", last);
	runthreads_priority(1, 1, 0);
	kprintf("\n1 prioritized Schedule test done.\n");
	// kprintf("Starting 3 prioritized schedule test...\n");
	// runthreads_priority(1, 3);
	// kprintf("\n3 loud prioritized Schedule test done.\n");
	kprintf("Starting 4 prioritized schedule test - quite mode...\n\n");
	kprintf("'%c'-'%c' should move to the top priority finishing early\n",
		last - 3, last);
	runthreads_priority(0, 4, 0);
	kprintf("\n4 prioritized Schedule test done.\n");

	// kprintf("Starting 1 starved thread schedule test...\n");
	// runthreads_priority(1, 0, 1);
	// kprintf("\n3 quiet 1 starved thread Schedule test done.\n");

	return 0;
}
