#pragma once
#include <atomic>
#include <vector>
#include "gpu_tasks.hpp"
#include "transfer_memory.hpp"

using namespace std;

using TaskVector = vector<Task*>;

enum PackState : uint8_t {
	can_be_filled,
	being_filled,
	filled_by_scheduler,
	copying_to_dev,
	copied_to_dev, // ready_to_run
	running,
	finished,
	dep_tracking,
	dep_tracked,
	exit_the_loop,
	no_of_states
};

struct GPUTaskPlaceState{
	void* dev_mem;
	size_t dev_mem_size;
	PackState state;
};

struct ParallelLane {
	TransferMemory::RID rid;
	vector<GPUTask> ready_tasks;
};

struct TaskPack {
	Scheduler* sch;
	std::atomic<int>* async_barrier_count;
	long task_count;		
	vector<ParallelLane>* para_lanes;
	TaskPack() : sch(nullptr),  task_count(0), para_lanes(nullptr) {
		async_barrier_count = new std::atomic<int>(0);
	}
};

class Scheduler;

class PackageManager {
private:
	vector<TaskPack>* packs;
	vector<Scheduler*>* schedulers;
	GPUTaskPlaceState* gpu_packs; 	
	int last_pack_filled;
	bool* kernels_end;
	const int DEV_MEM_SIZE = 1024 * 1024;
	int parallel_lane_count;
	int min_tasks_to_pack;
public:
	PackageManager(vector<Scheduler*>* s, int p, int pl, int min_tasks);	
	bool* get_kernel_end() { return kernels_end; }
	vector<TaskPack>* get_packs();
	GPUTaskPlaceState* get_gpu_packs();
	GPUTaskPlaceState& get_gpu_pack(int p, int pl);
	void create_first_packs();
	void create_next_pack(bool first_pack = false);
	void pack_parallel_lane_finished(int p, int pl);
	void copy_results_to_host(const GPUTask& gtask);
};
