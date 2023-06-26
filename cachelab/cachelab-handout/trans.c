/* 
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

void transpose_32x32(int M, int N, int A[N][M], int B[M][N])
{
    int tmp[8];
    for (int i = 0; i < 32; i += 8)
    {
        for (int j = 0; j < 32; j += 8)
        {
            for (int a = i; a < i + 8; a++)
            {
                for (int m = 0; m < 8; m++) tmp[m] = A[a][j + m];
                for (int m = 0; m < 8; m++) B[j + m][a] = tmp[m];

            }
        }
    }
}

void transpose_64x64(int M, int N, int A[N][M], int B[M][N])
{
    int tmp[8];
    for (int i = 0; i < 64; i += 8)
    {
        for (int j = 0; j < 64; j += 8)
        {
            // 每个8x8的数组(subA, subB)再分成2x2个大小为4x4的数组，索引分别为0，1，2，3

            // subA          subB
            // [0, 1         [0, 1
            //  2, 3]        2, 3]

            for (int a = i; a < i + 4; a++)
            {
                // 起始[a][j]
                // 将转置后的subA的0、1号保存到subB的0、1号
                for (int m = 0; m < 8; m++) tmp[m] = A[a][j + m];
                for (int m = 0; m < 4; m++) B[j + m][a] = tmp[m];
                for (int m = 0; m < 4; m++) B[j + m][a + 4] = tmp[m + 4];
            }
            for (int a = j; a < j + 4; a++)
            {
                // 起始[i][a]
                // 保存subB的1号
                tmp[0] = B[a][i + 4];
                tmp[1] = B[a][i + 5];
                tmp[2] = B[a][i + 6];
                tmp[3] = B[a][i + 7];

                // 保存subA的2号
                tmp[4] = A[i + 4][a];
                tmp[5] = A[i + 5][a];
                tmp[6] = A[i + 6][a];
                tmp[7] = A[i + 7][a];

                // subA的2号转置到subB的3号
                B[a][i + 4] = tmp[4];
                B[a][i + 5] = tmp[5];
                B[a][i + 6] = tmp[6];
                B[a][i + 7] = tmp[7];

                // subB的1号保存（平移）到subB的2号
                B[a + 4][i + 0] = tmp[0];
                B[a + 4][i + 1] = tmp[1];
                B[a + 4][i + 2] = tmp[2];
                B[a + 4][i + 3] = tmp[3];
            }
            for (int a = i + 4; a < i + 8; a++)
            {
                for (int m = 0; m < 4; m++) tmp[m] = A[a][j + 4 + m];
                for (int m = 0; m < 4; m++) B[j + 4 + m][a] = tmp[m];                
            }
        }
    }
}

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
    if (M == N && M == 32)
    {
        transpose_32x32(M, N, A, B);
    }
    else if (M == N && M == 64)
    {
        transpose_64x64(M, N, A, B);
    }
    else
    {
        for (int i = 0; i < N; i += 16)
        {
            for (int j = 0; j < M; j += 16)
            {
                for (int a = i; a < i + 16 && a < N; a++)
                {
                    for (int b = j; b < j + 16 && b < M; b++)
                    {
                        B[b][a] = A[a][b];
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

