#pragma once
#include <vector>
#include <stdio.h>
#include "data.hpp"

using namespace std;

class Partition {
private:
	vector<Data*> blocks;
	size_t row_count, col_count;
public:
	Data* operator()(size_t i, size_t j);
	Data* operator()(size_t index);
	Partition(Data *d, size_t rows, size_t cols);
};
