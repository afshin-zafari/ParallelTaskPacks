#include "data.hpp"
#include "partitioner.hpp"
#include "scheduler.hpp"

Partition::Partition(Data *d, size_t rows, size_t cols) : row_count(rows), col_count(cols) {
	size_t elem_size_in_bytes = d->get_size() / rows / cols;
	// nested for loops for rows and cols
	for (size_t i = 0; i < row_count; ++i) {
		for (size_t j = 0; j < col_count; ++j) {
			// Initialize each block
			size_t offset = (j + i * col_count) * elem_size_in_bytes;
			char ss[100];
			snprintf(ss, sizeof(ss), "%s_%zu_%zu", d->get_name().c_str(), i, j);
			Data *elem= d->get_scheduler()->register_data(ss, (char*)d->get_address() + offset, elem_size_in_bytes);
			elem->set_parent(d);
			blocks.push_back(elem);
		}
	}	
}

Data* Partition::operator()(size_t row, size_t col) {
	return blocks[row * col_count + col];
}

Data* Partition::operator()(size_t index) {
	return blocks[index];
}
