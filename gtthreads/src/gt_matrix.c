#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "gt_include.h"


#define ROWS 256
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 2
#define NUM_GROUPS NUM_CPUS

#define NUM_THREADS 128


/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
} matrix_t;


typedef struct __uthread_arg
{
	unsigned int mat_size;
	unsigned int tid;
	unsigned int gid;
}uthread_arg_t;

struct timeval tv1;

static void generate_matrix(matrix_t *mat, int val)
{
	int i,j;
	mat->rows = SIZE;
	mat->cols = SIZE;
	for(i = 0; i < mat->rows;i++)
		for( j = 0; j < mat->cols; j++ )
		{
			mat->m[i][j] = val;
		}
	return;
}


extern int uthread_create(uthread_t *, void *, void *, uthread_group_t);

static void * uthread_mulmat(void *p)
{
	struct timeval tv2;

#define ptr ((uthread_arg_t *)p)

	fprintf(stderr, "\nThread(id:%d, group:%d) started",ptr->tid, ptr->gid);

	matrix_t *A = (matrix_t *)MALLOCZ_SAFE(sizeof(matrix_t));
	matrix_t *B = (matrix_t *)MALLOCZ_SAFE(sizeof(matrix_t));
	matrix_t *C = (matrix_t *)MALLOCZ_SAFE(sizeof(matrix_t));

	generate_matrix(A, 1);
	generate_matrix(B, 1);
	generate_matrix(C, 0);

	for(int i = 0; i < ptr->mat_size; i++){
		for(int j = 0; j < ptr->mat_size; j++){
			for(int k = 0; k < ptr->mat_size; k++){
				C->m[i][j] += (A->m[i][k]) * (B->m[k][j]);
			}
		}
	}

	gettimeofday(&tv2,NULL);
	fprintf(stderr, "\nThread(id:%d, group:%d, size:%d) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, ptr->mat_size, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));

#undef ptr
	return 0;
}

void parse_args(int argc, char* argv[])
{
	int inx;

	for(inx=0; inx<argc; inx++)
	{
		if(argv[inx][0]=='-')
		{
			if(!strcmp(&argv[inx][1], "lb"))
			{
				//TODO: add option of load balancing mechanism
				printf("enable load balancing\n");
			}
			else if(!strcmp(&argv[inx][1], "s"))
			{
				//TODO: add different types of scheduler
				inx++;
				if(!strcmp(&argv[inx][0], "0"))
				{
					printf("use priority scheduler\n");
				}
				else if(!strcmp(&argv[inx][0], "1"))
				{
					printf("use credit scheduler\n");
				}
			}
		}
	}

	return;
}



uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];

int main()
{
	uthread_arg_t *uarg;
	int inx;

	//parse_args(argc, argv);

	gtthread_app_init();

	gettimeofday(&tv1,NULL);

	int credit_groups[] = {25, 50, 75, 100};
	unsigned int mat_sizes[] = {32, 64, 128, 256};

	for(int cdi = 0; cdi < 4; cdi++){
		for(int msi = 0; msi < 4; msi++){
			for(int i = 0; i < 8; i++){
				int inx = cdi * 32 + msi * 8 + i;
				
				uarg = &uargs[inx];

				uarg->mat_size = mat_sizes[msi];
				uarg->tid = inx;
				uarg->gid = 0;

				uthread_create(&utids[inx], uthread_mulmat, uarg, uarg->gid);
			}
		}
	}

	gtthread_app_exit();

	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
