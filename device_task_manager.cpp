#include <iostream>

#include "device_task_manager.hpp"
#include "gpu_tasks.hpp"
#include "logger.hpp"
#include "package_manager.hpp"
#include "scheduler.hpp"
#include "timer_aggregate.hpp"
#include "transfer_memory.hpp"


#define __global__
#define __shared__ 
void __syncthreads() {}
void __nanosleep(int ns) {}
void __threadfence() {}
void process_task(GPUTask& t) {}
size_t atomicAdd(size_t* mem, int val) {
	size_t ret = *mem;
	*mem += val;
	return ret;
}

//__device__ 
void run_task(int p, GPUTask* gt) {	
	switch (gt->get_op_code()) {
	case Op_SumReduction:
		break;
	case Op_GEMM:
		break;
	case Op_Scale:
		break;
	default:
		break;
	}
}


static void persistent_kernel(bool* kernels_end, GPUTaskPlaceState* gpu_packs, size_t np, size_t n_pl, size_t pl) {
	int p = 0;	
	vector<string> kernel_tasks;
	while (true) {
		volatile auto& pack_entry = gpu_packs[pl + p * n_pl];
		volatile auto& state = pack_entry.state;
		if (state == exit_the_loop) {
			break;
		}

		while (gpu_packs[pl + p * n_pl].state != copied_to_dev) {
			this_thread::yield();
		}

 		GPUTask* tasks = static_cast<GPUTask*>(pack_entry.dev_mem);
		size_t max_task_count = pack_entry.dev_mem_size / sizeof(GPUTask);
		int t = 0;
		while (t < max_task_count) {
			if (tasks[t].get_op_code() == Op_arriveBarrier) {
				if (tasks[t+1].get_op_code() == Op_end_the_kernels) {
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
		p = (p+1) % np;
	}
}

DeviceTaskManager::DeviceTaskManager(PackageManager* pmngr) {
	pkg_manager = pmngr;
	packs = pkg_manager->get_packs();
	gpu_packs = pkg_manager->get_gpu_packs();
	xfer_mngr = new TransferMemory();
}

void DeviceTaskManager::copy_to_dev(int p, int pl) {	
	TransferMemory::Request req;
	req.dir = TransferMemory::Direction::HOST_TO_DEV;
	auto& the_pack = packs->at(p);
	auto& para_lane= the_pack.para_lanes->at(pl);
	size_t pl_count = the_pack.para_lanes->size();
	vector<GPUTask> &gtasks = para_lane.ready_tasks;
	req.src = gtasks.data();
	req.size = gtasks.size() * sizeof(GPUTask);
	req.dst = (char*)gpu_packs[pl + p * pl_count].dev_mem;	
	if (req.size > gpu_packs[pl + p * pl_count].dev_mem_size) {
		return;
	}
	else {
		para_lane.rid = xfer_mngr->register_xfer(req);
	}
}

void DeviceTaskManager::parallel_lane_finished(int p, int pl) {
	pkg_manager->pack_parallel_lane_finished(p, pl);
}

void DeviceTaskManager::finalize() {
	bool all_done = true;
	bool* kernels_end = pkg_manager->get_kernel_end();
	do {
		all_done = true;
		size_t pl_count = packs->at(0).para_lanes->size();
		for (int pl = 0; pl < pl_count; pl++) {
			all_done &= kernels_end[pl];
		}
	} while (!all_done);
	return;
}

void DeviceTaskManager::start(vector<Scheduler*> *schedulers) {
	loop_future = std::async(std::launch::async, [&]() {
		TransferMemory::RID rid;
		bool exit_loop = false;
		size_t pack_count = packs->size();
		
		while (!exit_loop) {
			for (int p = 0; p < pack_count; p++) {
				bool* kernels_end = pkg_manager->get_kernel_end();
				size_t pl_count = packs->at(p).para_lanes->size();
				for (int pl = 0; pl < pl_count; pl++) {
					exit_loop = kernels_end[pl];
					volatile auto& state = gpu_packs[pl + p * pl_count].state;					
					switch (state) {
					case can_be_filled:
					case being_filled:
						break;
					case filled_by_scheduler:
						copy_to_dev(p, pl);
						state = copying_to_dev;
						break;
					case copying_to_dev:
						rid = packs->at(p).para_lanes->at(pl).rid;
						if (xfer_mngr->check_and_remove_if_done(rid)) {
							state = copied_to_dev;
						}
						break;
					case copied_to_dev:
						// kernel will see and run it
						break;
					case running:
						break;
					case finished:						
						parallel_lane_finished(p, pl);
						state = dep_tracking;
						break;
					case exit_the_loop:
						exit_loop = true;
						break;
					default:
						break;
					}
				}
			}
		}
		});
	size_t pl_count = packs->at(0).para_lanes->size();
	size_t pack_count = packs->size();
	for (size_t k = 0; k < pl_count; k++) {
		kernel_futures.emplace_back(std::async(std::launch::async,
			persistent_kernel, pkg_manager->get_kernel_end(), gpu_packs, pack_count, pl_count, k
		)
		);
	}
}
