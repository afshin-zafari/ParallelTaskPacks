#include <chrono>
#include <cstring>

#include "scheduler.hpp"
#include "transfer_memory.hpp"


bool TransferMemory::main_loop() {
	while (true) {
		for (auto& se : s_events) {
			for (auto& [e, r] : se) {
				if (!is_done(e)) break;
				destroy_event(e);
				auto& req = map[r];
				if (req.task == nullptr) continue;
				auto& s = *req.task->get_scheduler();
				s.task_finished(req.task->get_id());
			}
		}
	}
	return true;
}


TransferMemory::RID TransferMemory::register_xfer(TransferMemory::Request& req) {
	req.rid = last_rid++;
	map[req.rid] = req;
	req.stream_index = last_stream++;
#ifdef __CUDA
	using namespace cudaProxy;
	cudaEventCreate(&req.event);
	cudaMemcpyAsync(req.dst, req.src, req.size, 
		req.dir == HOST_TO_DEV ? cudaMemcpyHostToDevice : cudaMemcpyDeviceToHost, streams[req.stream_index]);
	cudaEventRecord(req.event, streams[req.stream_index % sizeof(streams)]);
#else
	req.async_state = std::async(std::launch::async, [&req]() {
			if (req.dst != nullptr && req.src != nullptr) {
				memcpy(req.dst, req.src, req.size);
			}
		}).share();
#endif
	return req.rid;
}

bool TransferMemory::is_done(TransferMemory::RID rid) {
	if (map.find(rid) == map.end()) {
		return false;
	}
	auto& req = map[rid];
#ifdef __CUDA	
	return id_done(req.event);
#else
	using namespace std::chrono_literals;
	if (req.async_state.valid()) {
		if (req.async_state.wait_for(0ms) == std::future_status::ready) {
			req.async_state.get();
		}
	}
	return true;
#endif
	
}

bool TransferMemory::check_and_remove_if_done(TransferMemory::RID rid) {
	if (is_done(rid)) {
		map.erase(rid);
		return true;
	}
	return false;
}

bool TransferMemory::is_done(Event event) {
#ifdef __CUDA
	using namespace cudaProxy;
	return cudaEventQuery(event) == cudaSuccess;
#else
	return false;
#endif
}

void TransferMemory::destroy_event(Event event) {	
#ifdef __CUDA
	cudaEventDestroy(event);
#endif
}