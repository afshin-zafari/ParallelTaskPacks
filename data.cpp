#include <string>

#include "data.hpp"
#include "partitioner.hpp"

using std::string;

Data::Data(): 
	id(),
	tasks(),
	rt_state(),
	last_read(0),
	last_modify(0),
	address(nullptr),
	size(0),
	scheduler(nullptr),
	parent(nullptr),
	_partition(nullptr) {
}

Data::~Data() {
	delete _partition;
	free(address);
}
Data::Data(Scheduler* s, void *addr, size_t size) : 
	id(),
	tasks(),
	rt_state(),
	last_read(0),
	last_modify(0),
	address(addr),
	size(size),
	scheduler(s),
	parent(nullptr),
	_partition(nullptr) {
}

// Set the ID of the data
void Data::set_id(data_id_t d) {
	id = d;
}

// Get the ID of the data
data_id_t Data::get_id() const {
	return id;
}

//				   +- last-read
//   last-modify   |
//       |         |
// r r r w r r r r r
//                 ^- modify-request
//       ^- read-request
label_t Data::read_request(task_id_t t) {
	tasks.push_back(t);
	last_read++;
	return last_modify;
}

// modify request
label_t Data::modify_request(task_id_t tid) {
	tasks.push_back(tid);
	label_t r = last_read;
	last_modify = r + 1;
	last_read = r + 1;
	return r;
}

// get last read label
label_t Data::get_last_read() const {
	return last_read;
}

// get last modify label
label_t Data::get_last_modify() const {
	return last_modify;
}

//set name of data
void Data::set_name(string s) {
	name = s;
}

string Data::get_name() {
	return name;
}
// get state of data
label_t Data::get_state() const {
	return rt_state;
}
void Data::read_completed() {
	rt_state++;
}
void Data::modify_completed() {
	rt_state++;
}

Partition* Data::partition(int rows, int cols) {
	_partition = new Partition(this, rows, cols);
	return _partition;
}