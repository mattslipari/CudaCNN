/*
Modified from
https://github.com/zhxfl/CUDA-CNN
*/

#include "cuMatrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include "cublas_v2.h"
#include "CycleTimer.h"


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

__global__ void elementwiseMul(float *x, float *y, float *z, int rows, int cols) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (j >= cols || i >= rows) return;
    z[i * cols + j] = x[i * cols + j] * y[i * cols + j];
}

void matrixElementWiseMul(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->cols != y->cols || z->cols != x->cols || x->rows != y->rows || x->rows != z->rows) {
        printf("matrix elementwise multiply invalid dim\n");
        exit(0);
    }
    dim3 blockDim(16, 16, 1);
    dim3 gridDim((x->cols + blockDim.x - 1) / blockDim.x,
                 (x->rows + blockDim.y - 1) / blockDim.y);
    elementwiseMul << < blockDim, gridDim >> > (x->getDev(), y->getDev(), z->getDev(), x->rows, x->cols);

}

/*Matrix Concatenation*/
/*z = [x;y]*/
void matrixConcat(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->cols != y->cols || z->cols != x->cols || z->rows != x->rows + y->rows) {
        printf("matrix concat invalid dim\n");
        exit(0);
    }

    float *res = z->getDev();
    cudaMemcpy(res, x->getDev(), x->rows * x->cols * sizeof(float), cudaMemcpyDeviceToDevice);
    cudaMemcpy(&res[x->rows * x->cols], y->getDev(), y->rows * y->cols * sizeof(float), cudaMemcpyDeviceToDevice);
}

/*Matrix Split*/
/*y = x[1:row][:] z = x[row:end][:]*/
void matrixSplit(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z) {
    if (x->cols != y->cols || x->cols != z->cols || x->rows != y->rows + z->rows) {
        printf("matrix split invalid dim\n");
        exit(0);
    }

    cudaMemcpy(y->getDev(), x->getDev(), y->rows * y->cols * sizeof(float), cudaMemcpyDeviceToDevice);
    cudaMemcpy(z->getDev(), &(x->getDev())[y->rows * y->cols], z->rows * z->cols * sizeof(float),
               cudaMemcpyDeviceToDevice);
}

__global__ void matrixTransKernel(float *A, int rows, int cols) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (j >= cols || i >= rows) return;
    float tmp = A[i * cols + j];
    A[i * cols + j] = A[j * cols + i];
    A[j * cols + i] = tmp;
}

/*matrix transpose*/
/*x = T(x)*/
void matrixTranspose(cuMatrix<float> *x) {
    dim3 blockDim(16, 16);
    dim3 gridDim((x->cols + blockDim.x - 1) / blockDim.x,
                 (x->rows + blockDim.y - 1) / blockDim.y);

    matrixTransKernel << < blockDim, gridDim >> > (x->getDev(), x->rows,x->cols);
    cudaThreadSynchronize();
    int temp_r = x->rows;
    x->rows = x->cols;
    x->cols = temp_r;
}

void matrixSub(cuMatrix<float> *x, cuMatrix<float> *y, cuMatrix<float> *z, float lambda) {
    lambda = -lambda;
    float alpha = 1.0;
    cublasStatus_t stat;
    stat = cublasSgeam(getHandle(),
                       CUBLAS_OP_N,
                       CUBLAS_OP_N,
                       x->cols, y->rows,
                       &alpha,
                       x->getDev(), x->cols,
                       &lambda,
                       y->getDev(), y->cols,
                       z->getDev(), z->cols);
    cudaStreamSynchronize(0);
    getLastCudaError("matrixSub");
    if (stat != CUBLAS_STATUS_SUCCESS) {
        printf("matrixSub cublasSgemm error\n");
        cudaFree(x->getDev());
        cudaFree(y->getDev());
        cudaFree(z->getDev());
        exit(0);
    }
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