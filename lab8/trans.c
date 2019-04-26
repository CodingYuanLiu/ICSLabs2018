/* 
 * Liu Qingyuan 516072910016
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    /* Each cache line is able to accommodate 32 bytes with b=4. So the bsize is 32/sizeof(int) = 8*/
    int t1,t2,t3,t4,t5,t6,t7,t8;
    int row,col,i;
    /*
     * Case 32*32:
     * 
    */
    if(M == 32)
    {
        for(row = 0;row < N;row+=8)
        {
            for(col = 0; col < M; col+=8)
            {
                for(i = row;i<row+8;i++)
                {
                    //Use temps to avoid cache conflict between A and B.
                    t1 = A[i][col];
                    t2 = A[i][col+1];
                    t3 = A[i][col+2];
                    t4 = A[i][col+3];
                    t5 = A[i][col+4];
                    t6 = A[i][col+5];
                    t7 = A[i][col+6];
                    t8 = A[i][col+7];

                    B[col][i] = t1;
                    B[col+1][i] = t2;
                    B[col+2][i] = t3;
                    B[col+3][i] = t4;
                    B[col+4][i] = t5;
                    B[col+5][i] = t6;
                    B[col+6][i] = t7;
                    B[col+7][i] = t8;
                }
            }
        }
    }

    /*
     * Case 64*64: Cache is able to accommodate 4 lines in total. Consequently, 
     * it's better to set 4*4 blocks than 8*8 blocks. However, as a cache line 
     * can handle 8 elements, 4*4 blocks will cause additional waste and thus 
     * need optimizing to get full use of the cache line. 
     */
    else if(M == 64){
        for(row = 0;row < N;row+=8)
        {
            for(col = 0; col < M; col+=8)
            {
                /*
                 * Set 4 4*4 blocks as an operation set.
                 */
                for(i = row;i<row+4;i++)
                {

                    t1 = A[i][col];
                    t2 = A[i][col+1];
                    t3 = A[i][col+2];
                    t4 = A[i][col+3];
                    
                    t5 = A[i][col+4];
                    t6 = A[i][col+5];
                    t7 = A[i][col+6];
                    t8 = A[i][col+7];

                    B[col][i] = t1;
                    B[col+1][i] = t2;
                    B[col+2][i] = t3;
                    B[col+3][i] = t4;
                    
                    // Regard the [0,1] 4*4 block as a buffer.
                    B[col][i+4] = t5;
                    B[col+1][i+4] = t6;
                    B[col+2][i+4] = t7;
                    B[col+3][i+4] = t8;
                }

                for(int j = col;j<col+4;j++)
                {
                    t1 = A[row+4][j];
                    t2 = A[row+4+1][j];
                    t3 = A[row+4+2][j];
                    t4 = A[row+4+3][j];

                    t5 = B[j][row+4];
                    t6 = B[j][row+4+1];
                    t7 = B[j][row+4+2];
                    t8 = B[j][row+4+3];

                    B[j][row+4] = t1;
                    B[j][row+4+1] = t2;
                    B[j][row+4+2] = t3;
                    B[j][row+4+3] = t4;

                    B[j+4][row] = t5;
                    B[j+4][row+1] = t6;
                    B[j+4][row+2] = t7;
                    B[j+4][row+3] = t8;
                }

                for(i = row+4;i<row+8;i++)
                {
                    t1 = A[i][col+4];
                    t2 = A[i][col+5];
                    t3 = A[i][col+6];
                    t4 = A[i][col+7];

                    B[col+4][i] = t1;
                    B[col+5][i] = t2;
                    B[col+6][i] = t3;
                    B[col+7][i] = t4;
                }
            }
        }
    }
    
    /* simply change the block size to 16 will satisfy the demands*/
    else if(M==61)
    {
        for (row = 0; row < N; row += 16)
		{
			for (col = 0; col< M; col += 16)
			{
				for (i = row; i < row+16 && i<N; i++)
				{
					for (int j = col; j < col+16 && j<M; j++)
					{
                        //Diagonal elements may case conflict between A and B, so handle it outside the single loop.
						if (i != j)
							B[j][i] = A[i][j];
						else{
							t1 = A[i][j]; 
							t2 = i;
						}				
					}
					if (row == col)
					{
					    B[t2][t2] = t1;
					}
				}
			}
		}
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

