#pragma once
#include <future>
#include <vector>
#include <unordered_map>


#include "cuda_proxy.hpp"
#include "task.hpp"

using namespace std;
using Stream = cudaProxy::cudaStream_t;
using Event  = cudaProxy::cudaEvent_t;
class TransferMemory {
public:
	typedef size_t RID;
	enum Direction : uint8_t { DEV_TO_HOST, HOST_TO_DEV };
	struct Request {
		RID			rid;
		Event		event;
		Direction	dir;
		int			stream_index;
		void*		src;
		void*		dst;
		size_t		size;
		Task*		task;
		shared_future<void> async_state;
	};
	RID register_xfer(Request& req);
	bool is_done(RID);
	bool check_and_remove_if_done(RID);
	bool is_done(Event e);
	void destroy_event(Event);
	bool main_loop();
private:
	struct StreamEvents { Event event; RID rid; };
	static const int STREAM_COUNT = 3;
	Stream streams[STREAM_COUNT];
	vector<StreamEvents> s_events[STREAM_COUNT];
	unordered_map<RID, Request> map;
	RID last_rid;
	int last_stream;
};


