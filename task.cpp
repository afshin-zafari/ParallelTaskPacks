#include <iostream>
#include <string>

#include "data.hpp"
#include "scheduler.hpp"
#include "task.hpp"

std::string get_op_name(op_code op) {
	string name;
	switch (op) {
	case Op_None:
		name = "None";
		break;
	case Op_arriveBarrier:
		name = "arrBarrier";
		break;
	case Op_waitBarrier:
		name = "waitBarrier";
		break;
	case Op_end_the_kernels:
		name = "endKernels";
		break;
	case Op_GEMM:
		name = "GEMM";
		break;
	case Op_Scale:
		name = "Scale";
		break;
	case Op_SumReduction:
		name = "SumRow";
		break;
	case Op_Square:
		name = "Square";
		break;
	case Op_ReduceMax:
		name = "ReduceMax";
		break;			
	case Op_sAxPy:
		name = "sAxPy";
		break;
	default:
		name = "unknown";
		break;
	}
	return name;
}

Task::Task(Scheduler *s, op_code op, const string& n) : id(0), _op_code(op), scheduler(nullptr), state(TASK_CREATED), parent(nullptr), child_finished_count(0) {
	scheduler = s;
	s->register_task(this);
	name = get_op_name(_op_code);
	name += n;		
}

Task* Task::reads(Data* d) {
	data.push_back(d->get_id());
	access.push_back(ACCESS_READ);
	labels.push_back(d->read_request(id));
	return this;
}

Task* Task::modifies(Data* d) {
	data.push_back(d->get_id());
	access.push_back(ACCESS_MODIFY);
	labels.push_back(d->modify_request(id));
	return this;
}

void Task::set_id(task_id_t t) {
	id = t;
	state = TASK_REGISTERED;
}

task_id_t Task::get_id() {
	return id;
}

// can run if all data is in a state that allows the access
bool Task::can_run() {
	//for over data and access, check if data is in a state that allows the access
	if (state != TASK_REGISTERED) {
		return false;
	}
	for (size_t i = 0; i < data.size(); ++i) {
		const Data &d = *scheduler->get_data(data[i]);
		if ( d.get_state() != labels[i] ) {
			return false;
		}
	}
	return true;
}

// dump the task's data and access
void Task::dump() {
	DEBUG_ONLY(
			std::cout << "Task Name: " << name << std::endl;
			// write state of task with its enum name
			switch (state) {
				case TASK_CREATED:
					std::cout << "  State: TASK_CREATED" << std::endl;
					break;
				case TASK_REGISTERED:
					std::cout << "  State: TASK_REGISTERED" << std::endl;
					break;
				case TASK_STARTED:
					std::cout << "  State: TASK_STARTED" << std::endl;
					break;
				case TASK_FINISHED:
					std::cout << "  State: TASK_FINISHED" << std::endl;
					break;
			}
			for (size_t i = 0; i < data.size(); ++i) {
				std::cout << "  Data: " << scheduler->get_data(data[i])->get_name() << (access[i] == ACCESS_READ ? "   READ" : " MODIFY") << ", Wait for: " << labels[i] << std::endl;
			}
		) // debug-only pair
}

// set the scheduler for the task
void Task::set_scheduler(Scheduler* s) {
	scheduler = s;
}

void Task::on_finished() {
	if (state != TASK_STARTED) {
		return;
	}
	state = TASK_FINISHED;
	//for over data and access, mark the data as completed
	for (size_t i = 0; i < data.size(); ++i) {
		Data& d = *scheduler->get_data(data[i]);
		if (access[i] == ACCESS_READ) {
			d.read_completed();
		}
		else if (access[i] == ACCESS_MODIFY) {
			d.modify_completed();
		}
		DEBUG_ONLY(std::cout << d.get_name() << " state after task finished: " << d.get_state() << std::endl;)
	}
}

// is waiting for
bool Task::is_waiting_for(data_id_t d, label_t expected) {
	// for all data
	for (size_t i = 0; i < data.size(); ++i) {
		if (data[i] == d) {
			if (labels[i] == expected) {
				return true;
			}			
			return false;
		}
	}
	return false;
}

// run task
void Task::run() {
	// Implementation for running the task
	state = TASK_STARTED;
	DEBUG_ONLY(std::cout << name << " is running." << std::endl;)
	dump();
}

// add a task to children
void Task::add_to_children(Task* child) {
	children.push_back(child->get_id());
}

void Task::child_finished(Task* child) {
	std::lock_guard<std::mutex> _lock(_mutex);
	if (++child_finished_count >= children.size()) {
		scheduler->task_finished(id);
	}
}

void* Task::get_address_of_arg(int arg) {
	if (arg >= data.size()) {
		return nullptr;
	}
	data_id_t arg_id = data[arg];
	Data* d = get_scheduler()->get_data(arg_id);
	return d->get_address();
}

size_t Task::get_size_of_arg(int arg) {
	if (arg >= data.size()) {
		return 0;
	}
	data_id_t arg_id = data[arg];
	Data* d = get_scheduler()->get_data(arg_id);
	return d->get_size();
}