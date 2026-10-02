#include <iostream>

#include "data.hpp"
#include "gpu_tasks.hpp"
#include "logger.hpp"
#include "package_manager.hpp"
#include "scheduler.hpp"

long Scheduler::last_id = 0;
Scheduler::Scheduler() {
	id = last_id++;	
}
Scheduler::Scheduler(bool with_task_pack) {
	id = last_id++;
	task_pack_mode = with_task_pack;
}

task_id_t Scheduler::register_task(Task* t, Task *parentTask) {
	t->set_scheduler(this);
	t->set_id(static_cast<task_id_t>(tasks.size()));
	t->set_parent(parentTask);
	if (parentTask != nullptr) {
		parentTask->add_to_children(t);
	}
	tasks.push_back(t);
	return tasks.size() - 1;
}

void Scheduler::remove_task(task_id_t t) {
	tasks.erase(tasks.begin() + t);
}

Data* Scheduler::register_data(const char* name, void *addr, size_t size) {
	Data* d = new Data(this, addr, size);
	d->set_id(data.size());
	d->set_name(name);
	data.push_back(d);
	return d;
}

void Scheduler::remove_data(data_id_t d) {
	data.erase(data.begin() + d);
}

//get data by id
Data* Scheduler::get_data(data_id_t d) {
	if (d < data.size()) {
		return data[d];
	}
	return nullptr;
}

// dump tasks
void Scheduler::dump_tasks() {
	DEBUG_ONLY(std::cout << "--------\n";)
	for (size_t i = 0; i < tasks.size(); ++i) {
		tasks[i]->dump();
	}
}

// run tasks
void Scheduler::run_tasks() {
	for (size_t i = 0; i < tasks.size(); ++i) {
		if (tasks[i]->can_run()) {
			tasks[i]->run();
		}
	}
}

//check ready tasks
bool Scheduler::check_ready_tasks() {
	std::lock_guard<std::recursive_mutex> lock(_mutex);	
	bool all_finished = true;

	for (Task* t: tasks) {
		if (t->can_run() && t->get_state() != TASK_QUEUED) {			
			ready_tasks.push(t->get_id());
			t->set_state(TASK_QUEUED);
			all_finished = false;
		}
		if (!t->is_finished()) {
			all_finished = false;
		}
	}
	return all_finished;
}

//get one ready task
task_id_t Scheduler::get_ready_task() {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	if (!ready_tasks.empty()) {
		task_id_t t = ready_tasks.front();
		ready_tasks.pop();
		return t;
	}
	return static_cast<task_id_t>(-1);
}

// checkings after task finished
void Scheduler::check_after_task_finished(task_id_t t) {		
	Task& task = *tasks[t];
	if (task.get_state() != TASK_FINISHED) {
		return;
	}
	// for all task's data
	for (const data_id_t data : task.get_data()) {
		// for all tasks of data
		Data& d = *get_data(data);
		for (const auto& other_task : d.get_tasks()) {
			if (other_task != t && tasks[other_task]->can_run()) {
				if (tasks[other_task]->get_state() != TASK_QUEUED) {
					tasks[other_task]->set_state(TASK_QUEUED);
					ready_tasks.push(other_task);	
				}
			}
		}
	}
	task.set_state(TASK_OUTPUT_CHECKED);

}

void Scheduler::task_finished(task_id_t t) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	tasks[t]->on_finished();
	Task* p = tasks[t]->get_parent();
	if (p != nullptr) {
		p->child_finished(tasks[t]);
	}
	check_after_task_finished(t);
}

void Scheduler::main_loop() {
	while (!check_ready_tasks()) {		
		while (true) {
			task_id_t t = get_ready_task();
			if (t == static_cast<task_id_t>(-1)) {
				break; // No more ready tasks
			}
			tasks[t]->run();
			task_finished(t);
		}		
	}
}

void Scheduler::main_loop_task_pack() {
	vector<task_id_t> finished_tasks;
	while (!check_ready_tasks()) {		
		while (true) {
			task_id_t t = get_ready_task();
			if (t == static_cast<task_id_t>(-1)) {
				break; // No more ready tasks
			}
			tasks[t]->run();
			finished_tasks.push_back(t);
		}		
		for (auto t : finished_tasks) {
			task_finished(t);
		}
		finished_tasks.clear();
	}
}

//verify all tasks are at final state
bool Scheduler::verify_self() {
	for (const auto& task : tasks) {
		if (!task->is_finished()) {
			return false;
		}
	}
	return true;
}

void Scheduler::get_task_packs(TaskPack *pack_ptr, bool first_pack) {
	int pl= 0;
	TaskPack& pack = *pack_ptr;
	auto para_lane = [&](int pli) { return pack.para_lanes->at(pli);};
	WaitBarrierTask* waitBarrier = new WaitBarrierTask();
	ArriveBarrierTask* arriveBarrier = new ArriveBarrierTask();
	EndKernelsTask* endKernels = new EndKernelsTask();
	size_t parallel_lanes = pack.para_lanes->size();
	vector<task_id_t> temp;	
	pack.task_count = 0;
	if (!first_pack) {
		for (int pack_idx = 0; pack_idx < parallel_lanes; pack_idx++) {
			pack.para_lanes->at(pack_idx).ready_tasks.emplace_back(*waitBarrier);
		}
	}
	check_ready_tasks();
	while (true) {
		task_id_t t = get_ready_task();
		if (t == static_cast<int>(-1)) {
			break;
		}
		pack.para_lanes->at(pl++).ready_tasks.emplace_back(GPUTask(tasks[t]));
		pl %= parallel_lanes;
		pack.task_count++;
		temp.push_back(t);
	}
	for (int pack_idx = 0; pack_idx < parallel_lanes; pack_idx++) {
		pack.para_lanes->at(pack_idx).ready_tasks.emplace_back(*arriveBarrier);
		if (temp.size() == 0) {
			pack.para_lanes->at(pack_idx).ready_tasks.emplace_back(*endKernels);
		}
	}
	if (temp.size() > 0) {
		dump_lanes(pack.para_lanes);
	}
	for (auto t : temp) {
		tasks[t]->set_state(TASK_STARTED);
		task_finished(t);
	}
	temp.clear();
}

void Scheduler::dump_lanes(void* lanes) {
	for (auto lane : *(vector<ParallelLane> *)lanes) {
		for (auto t : lane.ready_tasks) {
			LOG_DEBUG("%s ==> ", get_op_name(t.get_op_code()).c_str());
		}
		LOG_DEBUG("\n");
	}
}
