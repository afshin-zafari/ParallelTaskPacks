#pragma once

#include <mutex>
#include <queue>
#include <vector>

#include "data.hpp"
#include "task.hpp"

class GPUTask;
struct TaskPack;
using namespace std;

class Scheduler {
private:
	static long last_id;
	scheduler_id_t id;
	vector<Task*> tasks;
	vector<Data*> data;
	queue<task_id_t> ready_tasks;	
	recursive_mutex _mutex;
	bool task_pack_mode;
public:
	Scheduler();
	Scheduler(bool);
	Scheduler(const Scheduler&) = default;
	scheduler_id_t get_id() { return id; }
	task_id_t register_task(Task* t, Task *parentTask = nullptr);
	void remove_task(task_id_t t);
	Task* get_task(task_id_t t) { return tasks[t]; }
	Data* register_data(const char* name, void *addr, size_t size);
	Data* get_data(data_id_t d);
	void remove_data(data_id_t d);
	void dump_tasks();
	void run_tasks();
	size_t task_count() { return tasks.size(); }
	bool check_ready_tasks();
	task_id_t get_ready_task();
	void task_finished(task_id_t);
	void check_after_task_finished(task_id_t);	
	void main_loop();
	void main_loop_task_pack();
	bool verify_self();
	void get_task_packs(TaskPack* pack, bool first_pack = false);
	void dump_lanes(void* para_lanes);
	void clear() {
		tasks.clear();
		data.clear();
		ready_tasks = queue<task_id_t>();
	}
};
