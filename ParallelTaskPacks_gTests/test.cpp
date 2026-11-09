#include "pch.h"
#include <stdio.h>
#include "data.hpp"
#include "device_task_manager.hpp"
#include "logger.hpp"
#include "pack_manager.hpp"
#include "partitioner.hpp"
#include "scheduler.hpp"
#include "timer_aggregate.hpp"



TEST(TestLogger, Basic) {
	// Optional: Lower the threshold to capture everything (default is Info)
	Logger::instance().set_level(LogLevel::Error);

	int errorCode = 404;
	double cpuUsage = 12.847;
	const char* moduleName = "GraphicsEngine";

	// Using printf-style formatting
	LOG_INFO("Initializing module %s with code %d\n", moduleName, errorCode);
	LOG_DEBUG("Current CPU utilization: %.2f%%\n", cpuUsage);
	LOG_ERROR("Fatal crash in pointer 0x%X\n", 0x7FFF5000);	
}

TEST(TestScheduler, Basic){
	Scheduler s;
	const int SIZE = 16 * 16 * sizeof(float);
	char *mem_a = (char*)malloc(SIZE);
	char* mem_b = (char*)malloc(SIZE);
	Data* a = s.register_data("A", mem_a, SIZE, { 16, 16 });
	Data* b = s.register_data("B", mem_b, SIZE, { 16, 16 });

	Task *t=new Task(&s);
	task_id_t task_id = t->get_id();
	t->reads(a)->modifies(b);
	EXPECT_TRUE(task_id == 0) << "Expected task ID 0";
	EXPECT_TRUE(a->get_id() == 0) << "Expected data ID 0";
	EXPECT_TRUE(b->get_id() == 1) << "Expected data ID 1";
	Task *t2=new Task(&s);
	task_id_t task_id2 = t2->get_id();
	t2->reads(b);
	EXPECT_TRUE(task_id2 == 1) << "Expected task ID 1";
	EXPECT_TRUE(a->get_last_read() == 1) << "Expected last read label 1";
	EXPECT_TRUE(b->get_last_read() == 2) << "Expected last read label 2";
	EXPECT_TRUE(b->get_last_modify() == 1) << "Expected last modify label 1";
	s.dump_tasks();
	EXPECT_TRUE(t->can_run()) << "Task t should be able to run";
	EXPECT_TRUE(!t2->can_run() ) << "Task t2 should not be able to run yet";
	t->run();
	t->on_finished();
	EXPECT_TRUE(t2->can_run()) << "Task t2 should be able to run after t";
}

TEST(TestPartition, Basic) {
	const int M = 4;
	const int N = 8;
	const int mat_size = M * N * sizeof(float);
	char *mem = (char *)malloc(mat_size);
	Scheduler s;
	Data* a = s.register_data("A", mem, mat_size, {M, N});

	dim2 tiles{ 2,2 };
	Partition p(a, tiles.rows, tiles.cols);
	Data* d00 = p(0, 0);
	Data* d01 = p(0, 1);
	Data* d10 = p(1, 0);
	Data* d11 = p(1, 1);
	EXPECT_EQ(d00->get_id(), 1);
	EXPECT_EQ(d01->get_id(), 2);
	EXPECT_EQ(d10->get_id(), 3);
	EXPECT_EQ(d11->get_id(), 4);
	// assert that address of d01 is correct
	EXPECT_EQ(d01->get_address(), mem + N / tiles.cols * sizeof(float));
	EXPECT_EQ(d00->get_address(), mem);
	// and for d11
	EXPECT_EQ(d11->get_address(), mem + M / tiles.rows * N * sizeof(float) + N / tiles.cols * sizeof(float));
}

TEST(TestPartition, WithTasks) {
	Logger::instance().set_level(LogLevel::Error);
	Scheduler s;
	char *mem_a = (char *)malloc(256);
	char *mem_b = (char *)malloc(256);
	char *mem_c = (char *)malloc(256);
	Data* A = s.register_data("A", mem_a, 256, { 16, 16 });
	Data* B = s.register_data("B", mem_b, 256, { 16, 16 });
	Data* C = s.register_data("C", mem_c, 256, { 16, 16 });
	const int N = 4;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));		
		t->reads(a(i))->modifies(b(i));
	}
	for (size_t i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));		
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
	}
	for (int i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "reduce_" + std::to_string(i));		
		t->reads(c(i))->modifies(c(0));
	}
	s.dump_tasks();
	size_t M = s.task_count();
	auto finish_running_tasks = [&]() {
		for (int i = 0; i < M; ++i) { s.get_task(i)->on_finished(); }
		};
	auto simulate_run = [&](const char* step) {
		LOG_DEBUG("%s\n", step);
		s.run_tasks();
		finish_running_tasks();
		};
	simulate_run("Step 1: square ...");
	simulate_run("Step 2: sum ...");
	for (int i = 0; i < M; ++i) {
		simulate_run(("Step 3, reduce round " + std::to_string(i)).c_str());
	}
	EXPECT_TRUE(s.verify_self()) <<  "Not all tasks are in their final state";
}

TEST(TestPartition, MainLoopGoesToEnd) {
	Scheduler s;
	char *mem_a = (char *)malloc(256);
	char *mem_b = (char *)malloc(256);
	char *mem_c = (char *)malloc(256);
	Data* A = s.register_data("A", mem_a, 256, { 16, 16 });
	Data* B = s.register_data("B", mem_b, 256, { 16, 16 });
	Data* C = s.register_data("C", mem_c, 256, { 16, 16 });
	const int N = 4;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));
		t->reads(a(i))->modifies(b(i));
	}
	for (size_t i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
	}
	for (int i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_ReduceMax, "reduce_" + std::to_string(i));
		t->reads(c(i))->modifies(c(0));
	}
	s.main_loop();
	EXPECT_TRUE(s.verify_self()) << "Not all tasks are in their final state";
}

void consumer_kernel(Scheduler& s) {
	while (!s.check_ready_tasks()) {
		while (true) {
			auto t = s.get_ready_task();
			if (t == static_cast<task_id_t>(-1)) {
				break;
			}
			s.get_task(t)->run();
			s.task_finished(t);
		}
	}
}

TEST(TestExecution, ConsumerThreads) {
	Scheduler s;
	const int SIZE = 16 * 16 * sizeof(float);
	char *mem_a = (char *)malloc(SIZE);
	char *mem_b = (char *)malloc(SIZE);
	char *mem_c = (char *)malloc(SIZE);
	Data* A = s.register_data("A", mem_a, SIZE, { 16, 16 });
	Data* B = s.register_data("B", mem_b, SIZE, { 16, 16 });
	Data* C = s.register_data("C", mem_c, SIZE, { 16, 16 });
	const int N = 4	;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));
		t->reads(a(i))->modifies(b(i));
	}
	for (size_t i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
	}
	for (size_t i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_ReduceMax, "reduce_" + std::to_string(i));
		t->reads(c(i))->modifies(c(0));		
	}

	std::thread t1(consumer_kernel, std::ref(s));
	std::thread t2(consumer_kernel, std::ref(s));
	t1.join();
	t2.join();
	s.dump_tasks();
	EXPECT_TRUE(s.verify_self()) << "Not all tasks are in their final state";
}

TEST(TestSugarring, Basic) {
	Scheduler s;
	const int SIZE = 16 * 16 * sizeof(float);
	char *mem_a = (char *)malloc(SIZE);
	char *mem_b = (char *)malloc(SIZE);
	char *mem_c = (char *)malloc(SIZE);
	Data* A = s.register_data("A", mem_a, SIZE, { 16, 16 });
	Data* B = s.register_data("B", mem_b, SIZE, { 16, 16 });
	Data* C = s.register_data("C", mem_c, SIZE, { 16, 16 });
	const int N = 4;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	auto task_2_args = [&](string t_name, auto d1, auto d2) {
		Task* t = new Task(&s, Op_SumReduction, t_name);		
		t->reads(d1)->modifies(d2);
		};
	auto task_3_args = [&](string t_name, Data* d1, Data* d2, Data* d3) {
		Task* t = new Task(&s, Op_GEMM, t_name);		
		t->reads(d1)->reads(d2)->modifies(d3);
		};
	for (int i = 0; i < N; ++i) {
		task_2_args("square_", a(i), b(i));
	}
	for (size_t i = 0; i < N; i += 2) {
		task_3_args("sum_", b(i), b(i + 1), c(i));
	}
	for (size_t i = 2; i < N; i += 2) {
		task_2_args("reduce_", c(i), c(0));
	}

	std::thread t1(consumer_kernel, std::ref(s));
	std::thread t2(consumer_kernel, std::ref(s));
	t1.join();
	t2.join();
	s.dump_tasks();
	EXPECT_TRUE(s.verify_self()) << "Not all tasks are in their final state";
}

void decompose_sAxPy(Scheduler& s, Task* parentTask, const int parts) {
	auto& dataArgs = parentTask->get_data();
	Data* A = s.get_data(dataArgs[0]);
	Data* B = s.get_data(dataArgs[1]);
	Data* C = s.get_data(dataArgs[2]);

	Partition a(A, 1, parts);
	Partition b(B, 1, parts);
	Partition c(C, 1, parts);
	for (int i = 0; i < parts; i++) {
		Task* t = new Task(&s, Op_sAxPy, "saxpy");
		t->set_parent(parentTask);
		parentTask->add_to_children(t);
		t->reads(a(i))->reads(b(i))->modifies(c(i));
	}
}

TEST(TestExecution, Hierarchical) {
	Scheduler s;
	const int SIZE = 16 * 16 * sizeof(float);
	char *mem_a = (char *)malloc(SIZE);
	char *mem_b = (char *)malloc(SIZE);
	char *mem_c = (char *)malloc(SIZE);
	Data* A = s.register_data("A", mem_a, SIZE, { 16, 16 });
	Data* B = s.register_data("B", mem_b, SIZE, { 16, 16 });
	Data* C = s.register_data("C", mem_c, SIZE, { 16, 16 });

	const int N = 4;
	Task* SAXPY = new Task(&s, Op_sAxPy, "SAXPY");	
	SAXPY->reads(A)->reads(B)->modifies(C);
	decompose_sAxPy(s, SAXPY, N);
	auto& children = SAXPY->get_children();
	Task* first_child = s.get_task(children[0]);
	decompose_sAxPy(s, first_child, N);

	s.main_loop();

	EXPECT_TRUE(s.verify_self()) << "Not all tasks are in their final state";
}



Data* create_tensor(Scheduler& s, const char* A, dim2 shape, dim2 tile, float fill_with = -1.0f) {
	size_t bytes_A = (size_t)shape.rows * shape.cols * sizeof(float); 
	void* mem_A = malloc(bytes_A);
	Data* ptr_A = s.register_data(A, mem_A, bytes_A, shape); 
	ptr_A->partition(tile.rows, tile.cols);
	float* mem = static_cast<float*>(ptr_A->get_address());
	for (size_t i = 0; i < shape.rows * shape.cols; ++i) {
		if (fill_with == -1.0f) {
			mem[i] = static_cast<float>(i);
		}
		else {
			mem[i] = fill_with;
		}
	}
	return ptr_A;
}

void create_SumRow(string name, Scheduler& s, Data* A, Data* B) {
	size_t M, N;
	Partition& a = *A->get_partition();
	Partition& b = *B->get_partition();
	a.get_tiles_info(M, N);

	for (int i = 0; i < M; ++i) {
		for (int j = 0; j < N; ++j) {
			char _name[100];
			snprintf(_name, sizeof(_name),  "_%02d_%02d", i, j);
			Task* t = new Task(&s, Op_SumReduction, _name);
			t->reads(a(i, j))->modifies(b(i));
		}
	}
}

void create_MatMul(string name, Scheduler& s, Data* A, Data* B, Data* C) {
	size_t M, N, K;
	Partition& a = *A->get_partition();
	Partition& b = *B->get_partition();
	Partition& c = *C->get_partition();
	a.get_tiles_info(M, K);
	b.get_tiles_info(K, N);
	for (int i = 0; i < M; ++i) {
		for (int j = 0; j < N; ++j) {
			for (int k = 0; k < N; ++k) {
				char _name[100];
				snprintf(_name, sizeof(_name), "_%02d_%02d_%02d", i, k, j);
				Task* t = new Task(&s, Op_GEMM, _name);
				t->reads(a(i, k))->reads(b(j, k))->modifies(c(i, j));
			}
		}
	}
}

struct TensorShape {
	dim2 shape;
	dim2 tile;
};
struct GemmConfig {
	TensorShape A, B, C;
};
struct FillABC {
	float A, B, C;
};
void create_QK_SumRow(Scheduler& s, string tag, GemmConfig config) {
	auto Q = create_tensor(s, "Q", config.A.shape, config.A.tile);
	auto K = create_tensor(s, "K", config.B.shape, config.B.tile);
	auto C = create_tensor(s, "C", config.C.shape, config.C.tile);
	auto D = create_tensor(s, "D", config.C.shape, config.C.tile);

	create_MatMul(string("QKMult") + tag, s, Q, K, C);
	create_SumRow(string("SumRow") + tag, s, C, D);
}

void create_QK_SumRow_with_fills(Scheduler& s, string tag, GemmConfig config, FillABC fills) {
	auto Q = create_tensor(s, "Q", config.A.shape, config.A.tile, fills.A);
	auto K = create_tensor(s, "K", config.B.shape, config.B.tile, fills.B);
	auto C = create_tensor(s, "C", config.C.shape, config.C.tile, fills.C);
	auto D = create_tensor(s, "D", config.C.shape, config.C.tile, fills.C);

	create_MatMul(string("QKMult") + tag, s, Q, K, C);
	create_SumRow(string("SumRow") + tag, s, C, D);
}


TEST(DataPartitions, TileContents) {
	Scheduler s;
	const int M = 16, N = 32;
	const int BM = 4, BN = 4;
	const int BMM = M / BM, BNN = N / BN;
	Data& A = *create_tensor(s, "A", { M, N }, { BM, BN });

	float* mem = static_cast<float*>(A.get_address());
	for (size_t i = 0; i < M * N; ++i) {
		mem[i] = static_cast<float>(i);
	}
	for (int ti = 0; ti < BM; ++ti) {
		for (int tj = 0; tj < BN; ++tj) {
			Partition& a = *A.get_partition();
			Data& tile = *a(ti, tj);
			size_t parent_row_size, elem_size;
			tile.get_parent_tile_size(parent_row_size, elem_size);	
			char* base = static_cast<char*>(tile.get_address());			
			for (size_t i = 0; i < BMM; ++i) {
				for (size_t j = 0; j < BNN; ++j) {
					size_t parent_index = (ti * BMM + i) * N + (tj * BNN + j);
					float expected_value = static_cast<float>(parent_index);
					char* elem_addr = base + i * parent_row_size + j * elem_size;
					float actual_value = *(float*)(elem_addr);
					EXPECT_EQ(expected_value, actual_value)
						<< "Mismatch at tile (" << ti << ", " << tj 
						<< "), element (" << i << ", " << j << ")";
				}				
			}
		}
	}
}

TEST(DataPartitions, PackContents) {
	Scheduler::clear_all();
	Scheduler s;
	const int M = 16, N = 32, K = 16;
	const int BM = 4, BN = 4, BK = 4;
	const int BMM = M / BM, BNN = N / BN;
	
	
	GemmConfig gemm_cfg{ {{M,K},{BM,BK}}, {{K,N},{BK,BN}} , {{M,N},{BM,BN}} };
	create_QK_SumRow(s, "_s", gemm_cfg);
	vector<Scheduler*> sch{ &s };


	PackManager pmngr(&sch, 2, 1, 1);
	DeviceTaskManager dtm(&pmngr);
	pmngr.create_first_packs();
	dtm.copy_to_dev(0, 0);
	auto* g_packs = pmngr.get_gpu_packs();
	GPUTask* g_tasks = static_cast<GPUTask*>(g_packs[0].dev_mem);
	for (int t = 0; t < BM * BN; t++) {
		auto tid = g_tasks[t].get_task_id();
		auto* ctask = s.get_task(tid);
		char* g_mem;
		size_t parent_row_size;
		size_t elem_size;
		char* c_mem;

		for (int i = 0; i < BMM; i++) {
			for (int j = 0; j < BNN; j++) {
				for (int arg = 0; arg < 3; arg++) {
					g_mem = (char*)g_tasks[t].get_args().gemm_args.operands[arg].dev_mem;
					parent_row_size = g_tasks[t].get_args().gemm_args.operands[arg].parent_row_size;
					elem_size = g_tasks[t].get_args().gemm_args.operands[arg].elem_size;
					c_mem = (char*)ctask->get_address_of_arg(arg);
					float cpu_elem = *(float*)(c_mem + i * parent_row_size + j * elem_size);
					float gpu_elem = *(float*)(g_mem + i * parent_row_size + j * elem_size);
					EXPECT_EQ(cpu_elem, gpu_elem);
				}
			}
		}

	}

}


void simulate_gpu_gemm(GPUTask* gt) {
	auto get_mem = [&](int arg, int ii, int jj) {
		void* tile_base = gt->get_args().gemm_args.operands[arg].dev_mem;
		size_t parent_row_size = gt->get_args().gemm_args.operands[arg].parent_row_size;
		size_t elem_size = gt->get_args().gemm_args.operands[arg].elem_size;
		float* mem_elem = (float*)((char*)tile_base + ii * parent_row_size + jj * elem_size);
		return mem_elem;
		};
	size_t rows_A = gt->get_args().gemm_args.operands[0].rows;
	size_t cols_A = gt->get_args().gemm_args.operands[0].cols;
	size_t cols_B = gt->get_args().gemm_args.operands[1].cols;
	for (int k = 0; k < cols_A; k++) {
		for (int i = 0; i < rows_A; i++) {
			for (int j = 0; j < cols_B; j++) {
				float* mem_elem_A = get_mem(0, i, k);
				float* mem_elem_B = get_mem(1, k, j);
				float* mem_elem_C = get_mem(2, i, j);
				float elem_A = *mem_elem_A;
				float elem_B = *mem_elem_B;
				float& elem_C = *mem_elem_C;
				elem_C = elem_A * elem_B + elem_C;
			}
		}
	}
}
void create_sample_gemm_config(GemmConfig& gemm_cfg, int M = 16, int N = 32, int K = 16, int BM = 4, int BN = 4, int BK = 4) {
	gemm_cfg.A.shape = { M, K };
	gemm_cfg.A.tile = { BM, BK };
	gemm_cfg.B.shape = { K, N };
	gemm_cfg.B.tile = { BK, BN };
	gemm_cfg.C.shape = { M, N };
	gemm_cfg.C.tile = { BM, BN };
}
TEST(DataPartitions, PackContentsOnDevice) {
	Scheduler::clear_all();
	Scheduler s;
	const int M = 16, N = 32, K = 16;
	const int BM = 4, BN = 4, BK = 4;
	const int BMM = M / BM, BNN = N / BN;
	
	
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg, M, N, K, BM, BN , BK);

	create_QK_SumRow_with_fills(s, "_s", gemm_cfg, {1.0f, 2.0f, 0.0f});
	vector<Scheduler*> sch{ &s };


	PackManager pmngr(&sch, BK, 1, 1);
	DeviceTaskManager dtm(&pmngr);
	for (int k_iter = 1; k_iter < BK; k_iter++) {
		pmngr.create_next_pack(k_iter == 1);
		dtm.copy_to_dev(0, 0);	
		auto* g_packs = pmngr.get_gpu_packs();
		GPUTask* g_tasks = static_cast<GPUTask*>(g_packs[0].dev_mem);
		for (int t = 0; t < BM * BN; t++) {
			auto tid = g_tasks[t].get_task_id();
			auto* task = s.get_task(tid);
			simulate_gpu_gemm(&g_tasks[t]);
			for (int i = 0; i < BMM; i++) {
				for (int j = 0; j < BNN; j++) {
					float gpu_elem = g_tasks[t].get_elem_val_arg_at_ij(2, i, j);
					float expected = 2.0f * k_iter * BK;
					EXPECT_EQ(expected, gpu_elem);
				}
			}
		}
		auto* packs = pmngr.get_packs();
		packs->at(k_iter-1).async_barrier_count->store(0);		
	}
}

TEST(PacksTest, PackContents_OneScheduler) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler::clear_all();
	
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg);

	Scheduler s;
	create_QK_SumRow(s, "_s", gemm_cfg);
	vector<Scheduler*> sch{ &s };
	

	PackManager pmngr(&sch, 20, 2, 1);		
	map<data_id_t, bool> data_written;

	pmngr.create_first_packs();
	for (auto& tp : *pmngr.get_packs()) {
		data_written.clear();
		for (auto& pl : *tp.para_lanes) {
			for (auto& gtask : pl.ready_tasks) {
				if (gtask.get_op_code() <= Op_last_internal_code) {
					continue;
				}
				auto tid = gtask.get_task_id();
				auto sid = gtask.get_scheduler_id();
				auto *task = sch[sid]->get_task(tid);
				auto& task_data = task->get_data();
				for (int i = 0; i < task_data.size(); i++) {
					auto data = sch[sid]->get_data(task_data[i]);
					auto access = task->get_access()->at(i);
					if (access == ACCESS_MODIFY) {
						size_t x, y;
						data->get_block_index(x, y);
						EXPECT_TRUE(data_written.find(data->get_id()) == data_written.end()) << "Block (" << x << ", " << y << ") was written to multiple times";
						data_written[data->get_id()] = true;
					}
				}
			}
		}
	}
	
}

TEST(PacksTest, PackContents_TwoSchedulers) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler::clear_all();
	Scheduler s1,s2;	
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg, 16, 32, 16, 4, 4, 4);
	create_QK_SumRow(s1, "_s1", gemm_cfg);
	create_QK_SumRow(s2, "_s2", gemm_cfg);
	vector<Scheduler*> sch{ &s1, &s2 };
	

	PackManager pmngr(&sch, 20, 2, 1);		
	map<data_id_t, bool> data_written;

	pmngr.create_first_packs();
	for (auto& tp : *pmngr.get_packs()) {
		data_written.clear();
		for (auto& pl : *tp.para_lanes) {
			for (auto& gtask : pl.ready_tasks) {
				if (gtask.get_op_code() <= Op_last_internal_code) {
					continue;
				}
				auto tid = gtask.get_task_id();
				auto sid = gtask.get_scheduler_id();
				auto *task = sch[sid]->get_task(tid);
				auto& task_data = task->get_data();
				for (int i = 0; i < task_data.size(); i++) {
					auto data = sch[sid]->get_data(task_data[i]);
					auto access = task->get_access()->at(i);
					if (access == ACCESS_MODIFY) {
						size_t x, y;
						data->get_block_index(x, y);
						EXPECT_TRUE(data_written.find(data->get_id()) == data_written.end()) << "Block (" << x << ", " << y << ") was written to multiple times";						
						data_written[data->get_id()] = true;
					}
				}
			}
		}
	}
	
}

TEST(PacksTest, PackContents_PackSequences) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler::clear_all();
	Scheduler s1,s2;
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg);
	create_QK_SumRow(s1, "_s1", gemm_cfg);
	create_QK_SumRow(s2, "_s2", gemm_cfg);
	vector<Scheduler*> sch{ &s1, &s2 };
	

	PackManager pmngr(&sch, 20, 2, 1);		
	map<data_id_t, bool> data_written;

	pmngr.create_first_packs();
	pmngr.create_next_pack();
	for (auto& tp : *pmngr.get_packs()) {
		data_written.clear();
		for (auto& pl : *tp.para_lanes) {
			for (auto& gtask : pl.ready_tasks) {
				if (gtask.get_op_code() <= Op_last_internal_code) {
					continue;
				}
				auto tid = gtask.get_task_id();
				auto sid = gtask.get_scheduler_id();
				auto *task = sch[sid]->get_task(tid);
				auto& task_data = task->get_data();
				for (size_t i = 0; i < task_data.size(); i++) {
					auto data = sch[sid]->get_data(task_data[i]);
					auto access = task->get_access()->at(i);
					if (access == ACCESS_MODIFY) {
						size_t x, y;
						data->get_block_index(x, y);
						EXPECT_TRUE(data_written.find(data->get_id()) == data_written.end()) << "Block (" << x << ", " << y << ") was written to multiple times";						
						data_written[data->get_id()] = true;
					}
				}
			}
		}
	}
	
}

TEST(PacksTest, PackContents_InsufficientTaskCount) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler::clear_all();
	Scheduler s1,s2;	
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg);
	create_QK_SumRow(s1, "_s1", gemm_cfg);
	create_QK_SumRow(s2, "_s2", gemm_cfg);
	vector<Scheduler*> sch{ &s1, &s2 };
	
	const int min_task_count = 20;
	PackManager pmngr(&sch, 20, 2, min_task_count);		
	map<data_id_t, bool> data_written;
	int sch_count[2] = { 0,0 };
	int task_count = 0;

	pmngr.create_first_packs();
	
	for (auto& tp : *pmngr.get_packs()) {
		data_written.clear();
		for (auto& pl : *tp.para_lanes) {
			for (auto& gtask : pl.ready_tasks) {
				if (gtask.get_op_code() <= Op_last_internal_code) {
					continue;
				}
				auto tid = gtask.get_task_id();
				auto sid = gtask.get_scheduler_id();
				auto *task = sch[sid]->get_task(tid);
				sch_count[sid]++;
				task_count++;
				auto& task_data = task->get_data();
				for (size_t i = 0; i < task_data.size(); i++) {
					auto data = sch[sid]->get_data(task_data[i]);
					auto access = task->get_access()->at(i);
					if (access == ACCESS_MODIFY) {
						size_t x, y;
						data->get_block_index(x, y);
						EXPECT_TRUE(data_written.find(data->get_id()) == data_written.end()) << "Block (" << x << ", " << y << ") was written to multiple times";						
						data_written[data->get_id()] = true;
					}
				}
			}
		}
	}
	EXPECT_TRUE(sch_count[0] > 0 && sch_count[1] > 0) << "Both schedulers should have tasks in the first pack";
	EXPECT_TRUE(task_count >= min_task_count) << "Not enough tasks in the first pack";
}

TEST(TestExecution, TaskPacks) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler s1, s2;
	GemmConfig gemm_cfg;
	create_sample_gemm_config(gemm_cfg);


	create_QK_SumRow(s1, "_s1", gemm_cfg);
	create_QK_SumRow(s2, "_s2", gemm_cfg);
	vector<Scheduler*> sch;
	sch.push_back(&s1);
	sch.push_back(&s2);

	PackManager pmngr(&sch, 20, 4, 20);
	DeviceTaskManager dmngr(&pmngr);
	pmngr.create_first_packs();
	dmngr.start();
	dmngr.finalize();
	EXPECT_TRUE(s1.verify_self());
	EXPECT_TRUE(s2.verify_self());
}

TEST(TestExecution, TimingExecution) {
	Logger::instance().set_level(LogLevel::Error);
	
	dim2 shape{0, 0}, tiles{0, 0};
	int M_list[] = {128};	
	int Mb_list[] = {4};
	Scheduler s;
	vector<Scheduler*> sch_list{ &s };
	for (auto m : M_list) {
		for (auto mb : Mb_list) {
			for (bool with_task_pack : {false, true}) {
				for (int iter = 0; iter < 2; iter++) {
					shape.rows = shape.cols = m;
					tiles.rows = tiles.cols = mb;
					Data* A = create_tensor(s, "A", shape, tiles);
					Data* B = create_tensor(s, "B", shape, tiles);
					Data* C = create_tensor(s, "C", shape, tiles);
					create_MatMul("gemm", s, A, B, C);
					{
						EventTimer::Scoped _(AggregateTimerEvent::TaskExecution);
						if (with_task_pack) {
							s.main_loop_task_pack();
						}
						else {
							s.main_loop();
						}
					}
					EXPECT_TRUE(s.verify_self());
					delete A;
					delete B;
					delete C;
					s.clear();
				}
				printf("\n-------------------\n%s: M = %d, Mb = %d\n", with_task_pack ? "EventPerTask" : "TaskPack", m, mb);
				EventTimer::instance().report(std::cout);
				EventTimer::instance().reset();
			}
		}
	}
}

TEST(TestExecution, Correctness) {
}

