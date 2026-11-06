#pragma once
#include "common.hpp"
#include "data.hpp"

class DataLocationManager {
private:
public:
	static data_location_t get_location_of_data(data_id_t d);
	static void set_location_of_data(data_id_t d, data_location_t loc);
	int copy_data_from_to(data_id_t d, data_location_t loc_from, data_location_t loc_to);

};