#ifndef _CU_MATRIX_H_
#define _CU_MATRIX_H_

#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include "helper_cuda.h"
#include "MemoryMonitor.h"

/*rows-major*/
template <class T>
class cuMatrix
{
public:
    /*constructed function with hostData*/
    cuMatrix(T *_data, int _n,int _m):rows(_n), cols(_m), hostData(NULL), devData(NULL){
        /*malloc host data*/
        mallocHost();
        srand(time(0));
        /*deep copy */
        memcpy(hostData, _data, sizeof(*hostData) * cols * rows);
    }

    /*constructed function with rows and cols*/
    cuMatrix(int _n,int _m):rows(_n), cols(_m), hostData(NULL), devData(NULL){
    }

    /*free cuda memery*/
    void freeCudaMem(){
        if(NULL != devData){
            MemoryMonitor::instance()->freeGpuMemory(devData);
            devData = NULL;
        }
    }

    /*destruction function*/
    ~cuMatrix(){
        if(NULL != hostData)
            MemoryMonitor::instance()->freeCpuMemory(hostData);
        if(NULL != devData)
            MemoryMonitor::instance()->freeGpuMemory(devData);
    }

    /*copy the device data to host data*/
    void toCpu(){
        cudaError_t cudaStat;
        mallocDev();
        mallocHost();
        cudaStat = cudaMemcpy (hostData, devData, sizeof(*devData) * cols * rows, cudaMemcpyDeviceToHost);

        if(cudaStat != cudaSuccess) {
            printf("cuMatrix::toCPU data download failed\n");
            MemoryMonitor::instance()->freeGpuMemory(devData);
            exit(0);
        }
    }

    /*copy the host data to device data*/
    void toGpu(){
        cudaError_t cudaStat;
        mallocDev();
        mallocHost();
        cudaStat = cudaMemcpy (devData, hostData, sizeof(*devData) * cols * rows , cudaMemcpyHostToDevice);

        if(cudaStat != cudaSuccess) {
            printf("cuMatrix::toGPU data upload failed\n");
            MemoryMonitor::instance()->freeGpuMemory(devData);
            exit(0);
        }
    }

    /*copy the host data to device data with cuda-streams*/
    void toGpu(cudaStream_t stream1){
        mallocDev();
        checkCudaErrors(cudaMemcpyAsync(devData, hostData, sizeof(*devData) * cols * rows , cudaMemcpyHostToDevice, stream1));
    }

    /*set all device memory to be zeros*/
    void gpuClear(){
        mallocDev();
        cudaError_t cudaStat;
        cudaStat = cudaMemset(devData,0,sizeof(*devData) * cols * rows);
        if(cudaStat != cudaSuccess) {
            printf("device memory cudaMemset failed\n");
            exit(0);
        }
    }

    void cpuClear(){
        mallocHost();
        memset(hostData, 0, cols * rows *sizeof(*hostData));
    }

    void setAllRandom(float lb, float ub) {
        mallocHost();
        for (int j = 0; j < rows; j++) {
            for (int i = 0; i < cols; i++) {
                float rand_val = lb + ((float)(rand()) /((float)(RAND_MAX/(ub-lb))));
                hostData[i * cols + j] = rand_val;
            }
        }
    }

    /*set  value*/
    void set(int i, int j, T v){
        mallocHost();
        hostData[i * cols + j] = v;
    }

    /*get value*/
    T get(int i, int j){
        mallocHost();
        return hostData[i * cols + j];
    }

    /*get the number of values*/
    int getLen(){
        return rows * cols;
    }

    T  *& getHost(){
        mallocHost();
        return hostData;
    }

    T  *& getDev(){
        mallocDev();
        return devData;
    }

    /*column*/
    int cols;

    /*row*/
    int rows;

private:
    /*host data*/
    T *hostData;

    /*device data*/
    T *devData;
private:
    void mallocHost(){
        if(NULL == hostData){
            /*malloc host data*/
            hostData = (T*)MemoryMonitor::instance()->cpuMalloc(cols * rows *sizeof(*hostData));
            if(!hostData) {
                printf("cuMatrix:cuMatrix host memory allocation failed\n");
                exit(0);
            }
            memset(hostData, 0, cols * rows * sizeof(*hostData));
        }
    }
    void mallocDev(){
        if(NULL == devData){
            cudaError_t cudaStat;
            /*malloc device data*/
            cudaStat = MemoryMonitor::instance()->gpuMalloc((void**)&devData, cols * rows * sizeof(*devData));
            if(cudaStat != cudaSuccess) {
                printf("cuMatrix::cuMatrix device memory allocation failed\n");
                exit(0);
            }

            cudaStat = cudaMemset(devData, 0, sizeof(*devData) * cols * rows * channels);
            if(cudaStat != cudaSuccess) {
                printf("cuMatrix::cuMatrix device memory cudaMemset failed\n");
                exit(0);
            }
        }
    }
};


/*matrix multiply*/
/*z = x * y*/
void matrixMul   (cuMatrix<float>* x, cuMatrix<float>*y, cuMatrix<float>*z);
/*z = T(x) * y*/
void matrixMulTA (cuMatrix<float>* x, cuMatrix<float>*y, cuMatrix<float>*z);
/*z = x * T(y)*/
void matrixMulTB (cuMatrix<float>* x, cuMatrix<float>*y, cuMatrix<float>*z);
#endif