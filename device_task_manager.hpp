#pragma once
#include <vector>
#include "transfer_memory.hpp"


class PackageManager;
struct TaskPack;
struct GPUTaskPlaceState;
using namespace std;

class DeviceTaskManager {
private:
	TransferMemory* xfer_mngr;	
	shared_future<void> loop_future;
	vector<shared_future<void>> kernel_futures;
	PackageManager* pkg_manager;
	vector<TaskPack>* packs;
	GPUTaskPlaceState* gpu_packs;
public:
	DeviceTaskManager(PackageManager* pmngr);	
	void start(vector<Scheduler*>* = nullptr);
	void copy_to_dev(int p, int pl);
	void parallel_lane_finished(int p, int pl);
	void finalize();
};
