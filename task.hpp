#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "common.hpp"

using namespace std;

class Data;
class Scheduler;

class Task {
private:
	task_id_t id;
	op_code _op_code;
	vector<data_id_t> data;
	vector<int> access;
	vector<label_t> labels;
	Scheduler* scheduler;
	string name;
	task_state_e state;
	Task* parent;
	vector<task_id_t> children;
	size_t child_finished_count;
	mutex _mutex;
	float extra_arg0, extra_arg1;
public:
	Task(Scheduler* s=nullptr, op_code op = Op_None, const string& n = "");
	task_id_t get_id();
	op_code get_op_code() { return _op_code; }
	void set_op_code(op_code op) { _op_code = op; }
	void set_id(task_id_t t);
	Task* reads(Data* d);
	Task* modifies(Data* d);
	void dump();
	bool can_run();
	bool is_finished() { return state >= TASK_FINISHED; }
	void on_finished();
	void set_scheduler(Scheduler* s);
	Scheduler* get_scheduler() { return scheduler; }
	bool is_waiting_for(data_id_t d, label_t expected);
	void run();
	vector<data_id_t>& get_data() { return data; }
	string get_name() { return name; }
	void set_state(task_state_e s) { state = s; }
	task_state_e get_state() { return state; }
	void set_parent(Task* t) { parent = t; }
	Task* get_parent() { return parent; }
	vector<task_id_t>& get_children() { return children; }
	void add_to_children(Task* t);
	void child_finished(Task* child);
	void* get_address_of_arg(int arg);
	float get_extra_arg(int arg) {
		return arg == 0 ? extra_arg0 : extra_arg1;
	}
	size_t get_size_of_arg(int arg);
};
