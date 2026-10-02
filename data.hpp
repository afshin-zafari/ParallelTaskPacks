#pragma once
#include <string>
#include <vector>
#include "common.hpp"

class Partition;
using namespace std;
class Scheduler;

class Data {
private:
	data_id_t id;
	vector<task_id_t> tasks;
	label_t rt_state, last_read, last_modify;
	//				   |- last-read
	//   last-modify   |
	//       |         |
	// r r r w r r r r r
	//                 ^- modify-request
	//       ^- read-request

	void* address;
	size_t size;
	string name;
	Scheduler* scheduler;
	Data* parent;
	Partition* _partition;
public:
	Data();
	~Data();
	Data(Scheduler *s, void *addr, size_t size);
	void set_id(data_id_t d);
	data_id_t get_id() const;
	label_t read_request(task_id_t);
	label_t modify_request(task_id_t);
	label_t get_last_read() const;
	label_t get_last_modify() const;
	void read_completed();
	void modify_completed();
	void set_name(string s);
	string get_name();
	label_t get_state() const;
	void* get_address() { return address; }
	size_t get_size() { return size; }
	Scheduler* get_scheduler() { return scheduler; }
	const vector<task_id_t>& get_tasks() { return tasks; }
	void set_parent(Data* p) { parent = p; }
	Data *get_parent() { return parent;  }
	Partition* get_partition() { return _partition; }
	Partition* partition(int rows, int cols);
};