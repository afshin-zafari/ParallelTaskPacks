#include <thread>

#include "logger.hpp"
#include "package_manager.hpp"
#include "scheduler.hpp"
#include "timer_aggregate.hpp"

PackageManager::PackageManager(vector<Scheduler*>* s, int pack_count, int pl_count, int min_tasks) {
	schedulers = s;
	last_pack_filled = 0;
	parallel_lane_count = pl_count;
	min_tasks_to_pack = min_tasks;
	packs = new vector<TaskPack>();
	for (int i = 0; i < pack_count; i++) {
		TaskPack tp{};
		packs->push_back(tp);
	}
	size_t size = (size_t)pack_count * (size_t)parallel_lane_count * sizeof(GPUTaskPlaceState);
#ifdef __CUDA
	cudaMallocManaged(&gpu_packs, size);
	cudaMallocManaged(&kernels_end, parallel_lane_count);
#else
	kernels_end = new bool[parallel_lane_count];
	gpu_packs = static_cast<GPUTaskPlaceState*>(malloc(size));
#endif	
	for (int p = 0; p < pack_count; p++) {
		packs->at(p).para_lanes = new vector<ParallelLane>();
		for (int pl = 0; pl < parallel_lane_count; pl++) {
			kernels_end[pl] = false;
			ParallelLane pl_{};
			packs->at(p).para_lanes->emplace_back(pl_);
			if (gpu_packs != nullptr) {
				get_gpu_pack(p, pl).dev_mem = malloc(DEV_MEM_SIZE);
				get_gpu_pack(p, pl).dev_mem_size = DEV_MEM_SIZE;
				get_gpu_pack(p, pl).state= can_be_filled;
			}
		}
	}
}

void PackageManager::create_first_packs() {
	create_next_pack(true /*first_pack*/);
}

void PackageManager::create_next_pack(bool first_pack) {
	int s_idx = 0;	
	size_t total_task_count = 0;
	int p_idx = last_pack_filled;	
	do {
		while (packs->at(p_idx).async_barrier_count->load() != 0) {
			std::this_thread::yield();
		}
		LOG_DEBUG("s-p, %d-%d\n", s_idx, p_idx);
		schedulers->at(s_idx)->get_task_packs(&packs->at(p_idx), first_pack);
		for (int pl = 0; pl < parallel_lane_count; pl++) {
			get_gpu_pack(p_idx, pl).state = filled_by_scheduler;
		}
		packs->at(p_idx).async_barrier_count->store(parallel_lane_count);
		total_task_count += packs->at(p_idx).task_count;
		last_pack_filled = (last_pack_filled + 1) % packs->size();
		p_idx = last_pack_filled;
		if (total_task_count >= min_tasks_to_pack) break;
		if (++s_idx >= schedulers->size()) break;		
	} while (true);
	LOG_DEBUG("s-p finished  === ");
}

GPUTaskPlaceState* PackageManager::get_gpu_packs() {
	return gpu_packs;	
}

vector<TaskPack>* PackageManager::get_packs() {
	return packs;
}

void PackageManager::copy_results_to_host(const GPUTask& gtask) {
	switch (gtask.get_op_code()) {
	case Op_None:
	case Op_waitBarrier:
	case Op_arriveBarrier:
	case Op_end_the_kernels:
		return;
	}

	task_id_t task_id = gtask.get_task_id();
	if (task_id == static_cast<task_id_t>(-1)) {
		return;
	}

	scheduler_id_t scheduler_id = gtask.get_scheduler_id();
	if (scheduler_id == static_cast<task_id_t>(-1)) {
		return;
	}

	for (auto s : *schedulers) {
		if (s->get_id() == scheduler_id) {
			auto task = s->get_task(task_id);
			void* src = nullptr;
			void* dest= nullptr;
			size_t size = 0;
			switch (gtask.get_op_code()) {
			case Op_GEMM:
				src = gtask.get_args().gemm_args.C;
				dest = task->get_address_of_arg(2);
				size = task->get_size_of_arg(2);				
			}
			if (src != nullptr && dest != nullptr && size != 0) {
				//memcpy(dest, src, size);// to be async
			}
		}
	}
}

void PackageManager::pack_parallel_lane_finished(int p, int pl) {
	LOG_DEBUG("p,pl finished: %d,%d\n", p, pl);
	if (packs->at(p).async_barrier_count->fetch_sub(1) == 1) {		
		get_gpu_pack(p, pl).state = can_be_filled;
		for (int pl_idx = 0; pl_idx < parallel_lane_count; pl_idx++) {
			for (const GPUTask& gtask: packs->at(p).para_lanes->at(pl_idx).ready_tasks) {
				copy_results_to_host(gtask);
			}
			packs->at(p).para_lanes->at(pl_idx).ready_tasks.clear();
		}
		LOG_DEBUG("create next pack %d \n", last_pack_filled);
		create_next_pack((p + 1) % packs->size());
	}

}

GPUTaskPlaceState& PackageManager::get_gpu_pack(int p, int pl) {
	return gpu_packs[pl + p * parallel_lane_count];
}