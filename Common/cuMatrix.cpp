#include "cuMatrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include "cublas_v2.h"


cublasHandle_t &getHandle() {
    static cublasHandle_t handle = NULL;
    if (handle == NULL) {
        cublasStatus_t stat;
        stat = cublasCreate(&handle);
        if (stat != CUBLAS_STATUS_SUCCESS) {
            printf("init: CUBLAS initialization failed\n");
            exit(0);
        }
    }
    return handle;
}

/*matrix transpose*/
/*x = T(x)*/
void matrixTranspose(cuMatrix<float>* x) {
    float alpha = 1.0;
    float beta = 0.0;
    float *y;
    cudaMalloc(&y, x->rows * x->cols * sizeof(float));
    cudaMemcpy(y, x->getDev(), x->rows * x->cols * sizeof(float), cudaMemcpyDeviceToDevice);

    cublasSgeam(getHandle(),
                CUBLAS_OP_T,
                CUBLAS_OP_N,
                x->rows, x->cols,
                &alpha,
                y, x->cols,
                &beta,
                NULL, x->rows,
                x->getDev(), x->rows);

    int temp_r = x->rows;
    x->rows = x->cols;
    x->cols = temp_r;
    cudaFree(y);
}

/*matrix multiply*/
/*z = x * y*/
void matrixMul(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->cols != y->rows || z->rows != x->rows || z->cols != y->cols) {
        printf("matrix mul chanels != 1\n");
        exit(0);
    }
    float alpha = 1.0;
    float beta = 0.0;
    cublasStatus_t stat;
    stat = cublasSgemm(
            getHandle(),
            CUBLAS_OP_N,
            CUBLAS_OP_N,
            y->cols,
            x->rows,
            y->rows,
            &alpha,
            y->getDev(),
            y->cols,
            x->getDev(),
            x->cols,
            &beta,
            z->getDev(),
            z->cols);
    cudaStreamSynchronize(0);
    getLastCudaError("matrixMul");
    if (stat != CUBLAS_STATUS_SUCCESS) {
        printf("matrixMul cublasSgemm error\n");
        cudaFree(x->getDev());
        cudaFree(y->getDev());
        cudaFree(z->getDev());
        exit(0);
    }
}

/*z = T(x) * y*/
void matrixMulTA(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->rows != y->rows || z->rows != x->cols || z->cols != y->cols) {
        printf("matrix mul chanels != 1\n");
        exit(0);
    }
    cublasStatus_t stat;
    float alpha = 1.0;
    float beta = 0.0;
    stat = cublasSgemm(
            getHandle(),
            CUBLAS_OP_N,
            CUBLAS_OP_T,
            y->cols,
            x->cols,
            y->rows,
            &alpha,
            y->getDev(),
            y->cols,
            x->getDev(),
            x->cols,
            &beta,
            z->getDev(),
            z->cols);
    cudaStreamSynchronize(0);
    getLastCudaError("matrixMulTA");
    if (stat != CUBLAS_STATUS_SUCCESS) {
        printf("matrixMulTA cublasSgemm error\n");
        exit(0);
    }
}

/*z = x * T(y)*/
void matrixMulTB(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->cols != y->cols || z->rows != x->rows || z->cols != y->rows) {
        printf("matrix mul chanels != 1\n");
        exit(0);
    }
    cublasStatus_t stat;
    float alpha = 1.0;
    float beta = 0.0;
    stat = cublasSgemm(
            getHandle(),
            CUBLAS_OP_T,
            CUBLAS_OP_N,
            y->rows,
            x->rows,
            y->cols,
            &alpha,
            y->getDev(),
            y->cols,
            x->getDev(),
            x->cols,
            &beta,
            z->getDev(),
            z->cols);
    cudaStreamSynchronize(0);
    getLastCudaError("matrixMulTB");
    if (stat != CUBLAS_STATUS_SUCCESS) {
        printf("matrixMulTB cublasSgemm error\n");
        exit(0);
    }
}