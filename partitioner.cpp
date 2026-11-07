#include "data.hpp"
#include "partitioner.hpp"
#include "scheduler.hpp"

Partition::Partition(Data *d, size_t rows, size_t cols) : row_count(rows), col_count(cols) {
	size_t parent_rows, parent_cols;
	d->get_shape(parent_rows, parent_cols);
	size_t elem_size_in_bytes = d->get_size() / parent_cols / parent_rows;
	size_t parent_row_size = parent_cols * elem_size_in_bytes;
	size_t rows_per_block = parent_rows / rows;
	size_t cols_per_block = parent_cols / cols;
	size_t block_size = rows_per_block * cols_per_block * elem_size_in_bytes;
	for (size_t i = 0; i < row_count; ++i) {
		for (size_t j = 0; j < col_count; ++j) {
			size_t offset = i * rows_per_block * parent_row_size + j * cols_per_block * elem_size_in_bytes;
			char ss[100];
			snprintf(ss, sizeof(ss), "%s_%zu_%zu", d->get_name().c_str(), i, j);
			Data* block = d->get_scheduler()->register_data(ss, (char*)d->get_address() + offset, block_size); // ToDo: the memory is not adjacent for the blocks, need to fix this
			block->set_parent(d);
			block->set_block_index(i, j);
			block->set_parent_tile_size(parent_row_size, elem_size_in_bytes);
			blocks.push_back(block);
		}
	}	
}

Data* Partition::operator()(size_t row, size_t col) {
	return blocks[row * col_count + col];
}

Data* Partition::operator()(size_t index) {
	return blocks[index];
}
