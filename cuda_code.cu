#include "cuda_code.hpp"
#include "device_task_manager.hpp"
#include "gpu_tasks.hpp"
#include "package_manager.hpp"

__device__ 
static op_code get_op_code(GPUTask* t) {
	return t->get_op_code();
}

// run_gpu_task
__device__
void run_task(int p, GPUTask* gt) {
	switch (get_op_code(gt)) {

	}
}

__global__
void persistent_kernel(bool* kernels_end, GPUTaskPlaceState* gpu_packs, size_t np, size_t n_pl, size_t pl) {
	int p = 0;
	while (true) {
		volatile auto& pack_entry = gpu_packs[pl + p * n_pl];
		volatile auto& state = pack_entry.state;
		if (state == exit_the_loop) {
			break;
		}

		while (gpu_packs[pl + p * n_pl].state != copied_to_dev) {
			//this_thread::yield();
		}

		GPUTask* tasks = static_cast<GPUTask*>(pack_entry.dev_mem);
		size_t max_task_count = pack_entry.dev_mem_size / sizeof(GPUTask);
		int t = 0;
		while (t < max_task_count) {
			if (get_op_code(&tasks[t]) == Op_arriveBarrier) {
				if (get_op_code(&tasks[t + 1]) == Op_end_the_kernels) {
					kernels_end[pl] = true;
					return;
				}
				state = finished;
				break;
			}
			run_task(p, &tasks[t]);
			t++;
		}
		if (t >= max_task_count) {
			state = finished; // temp solution, to be fixed/completed			
		}
		p = (p + 1) % np;
	}
}


void DeviceTaskManager::start_gpu() {
	dim3 grid, blocks;
	cudaStream_t stream;
	size_t shared_mem_size = 0;
	cudaEvent_t event;
	size_t pl_count = packs->at(0).para_lanes->size();
	size_t pack_count = packs->size();
	for (size_t k = 0; k < pl_count; k++) {
		cudaStreamCreate(&stream);
		cudaEventCreate(&event);
		persistent_kernel << <grid, blocks, shared_mem_size, stream >> > (pkg_manager->get_kernel_end(), gpu_packs, pack_count, pl_count, k);
		cudaEventRecord(event, stream);
		cudaEventSynchronize(event);
	}
}
