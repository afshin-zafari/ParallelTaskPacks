#pragma once
#include <cstddef>
#ifndef CUDA_PROXY
#define CUDA_PROXY
using namespace std;
namespace cudaProxy {
	struct cudaEvent_t { int i; };
	struct cudaStream_t {};
	constexpr int cudaSuccess = 0;
	typedef int cudaError_t;
	enum cudaMemcpyKind : int { cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost };	
	cudaError_t cudaMemcpyAsync(void* dst, const void* src, size_t count, cudaMemcpyKind kind, cudaStream_t);
	cudaError_t cudaEventCreate(cudaEvent_t*);
	cudaError_t cudaEventDestroy(cudaEvent_t);
	cudaError_t cudaEventQuery(cudaEvent_t);
	cudaError_t cudaEventRecord(cudaEvent_t, cudaStream_t);
	cudaError_t cudaStreamCreate(cudaStream_t*);
	cudaError_t cudaStreamDestroy(cudaStream_t);
	cudaError_t cudaMalloc(void** devPtr, size_t size);
	cudaError_t cudaMallocManaged(void** devPtr, size_t size, int = 1);
}
#endif /// CUDA_PROXY