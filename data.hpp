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
	size_t size, row_count, col_count;
	string name;
	Scheduler* scheduler;
	Data* parent;
	Partition* _partition;
	size_t my_tile_x_in_parent, my_tile_y_in_parent;
	size_t parent_row_size, element_size;
	size_t my_tile_rows, my_tile_cols;
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
	Partition* partition(size_t rows, size_t cols);
	void set_block_index(size_t x, size_t y) { this->my_tile_x_in_parent = x; this->my_tile_y_in_parent = y; }
	void get_block_index(size_t& x, size_t& y) { x = this->my_tile_x_in_parent; y = this->my_tile_y_in_parent; }
	void get_shape(size_t& rows, size_t& cols) { rows = row_count; cols = col_count; }
	void set_shape(size_t rows, size_t cols) { row_count = rows; col_count = cols; }
	void set_parent_tile_size(size_t row_size, size_t elem_size) { parent_row_size = row_size; element_size = elem_size; }
	void get_parent_tile_size(size_t& row_size, size_t& elem_size) { row_size = parent_row_size; elem_size = element_size; }
	void get_tile_size(size_t& rows, size_t& cols) { rows = my_tile_rows; cols = my_tile_cols; }
	void set_tile_size(size_t rows, size_t cols) { my_tile_rows = rows; my_tile_cols = cols; }
};