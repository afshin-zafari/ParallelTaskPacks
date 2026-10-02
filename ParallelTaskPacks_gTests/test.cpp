#include "pch.h"
#include <stdio.h>
#include "data.hpp"
#include "device_task_manager.hpp"
#include "logger.hpp"
#include "package_manager.hpp"
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
	char mem[256];
	Data* a = s.register_data("A", mem, 16);
	Data* b = s.register_data("B", mem + 16, 16);

	Task t(&s);
	task_id_t task_id = t.get_id();
	t.reads(a)->modifies(b);
	EXPECT_TRUE(task_id == 0) << "Expected task ID 0";
	EXPECT_TRUE(a->get_id() == 0) << "Expected data ID 0";
	EXPECT_TRUE(b->get_id() == 1) << "Expected data ID 1";
	Task t2(&s);
	task_id_t task_id2 = t2.get_id();
	t2.reads(b);
	EXPECT_TRUE(task_id2 == 1) << "Expected task ID 1";
	EXPECT_TRUE(a->get_last_read() == 1) << "Expected last read label 1";
	EXPECT_TRUE(b->get_last_read() == 2) << "Expected last read label 2";
	EXPECT_TRUE(b->get_last_modify() == 1) << "Expected last modify label 1";
	s.dump_tasks();
	EXPECT_TRUE(t.can_run()) << "Task t should be able to run";
	EXPECT_TRUE(!t2.can_run() ) << "Task t2 should not be able to run yet";
	t.run();
	t.on_finished();
	EXPECT_TRUE(t2.can_run()) << "Task t2 should be able to run after t";
}

TEST(TestPartition, Basic) {
	Scheduler s;
	char mem[256];
	Data* a = s.register_data("A", mem, 256);
	Partition p(a, 2, 2);
	Data* d00 = p(0, 0);
	Data* d01 = p(0, 1);
	Data* d10 = p(1, 0);
	Data* d11 = p(1, 1);
	EXPECT_EQ(d00->get_id(), 1);
	EXPECT_EQ(d01->get_id(), 2);
	EXPECT_EQ(d10->get_id(), 3);
	EXPECT_EQ(d11->get_id(), 4);
	// assert that address of d01 is correct
	EXPECT_EQ(d01->get_address(), mem + 64);
	EXPECT_EQ(d00->get_address(), mem);
	// and for d11
	EXPECT_EQ(d11->get_address(), mem + 3*64);
}

TEST(TestPartition, WithTasks) {
	Logger::instance().set_level(LogLevel::Error);
	Scheduler s;
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));
	const int N = 10;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));		
		t->reads(a(i))->modifies(b(i));
	}
	for (int i = 0; i < N; i += 2) {
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
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));
	const int N = 10;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));
		t->reads(a(i))->modifies(b(i));
	}
	for (int i = 0; i < N; i += 2) {
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
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));
	const int N = 10;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	for (int i = 0; i < N; ++i) {
		Task* t = new Task(&s, Op_Square, "square_" + std::to_string(i));
		t->reads(a(i))->modifies(b(i));
	}
	for (int i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
	}
	for (int i = 2; i < N; i += 2) {
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
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));
	const int N = 10;
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
	for (int i = 0; i < N; i += 2) {
		task_3_args("sum_", b(i), b(i + 1), c(i));
	}
	for (int i = 2; i < N; i += 2) {
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
	auto dataArgs = parentTask->get_data();
	Data* A = s.get_data(dataArgs[0]);
	Data* B = s.get_data(dataArgs[1]);
	Data* C = s.get_data(dataArgs[2]);

	Partition a(A, 1, parts);
	Partition b(B, 1, parts);
	Partition c(C, 1, parts);
	for (int i = 0; i < parts; i++) {
		Task* t = new Task(&s, Op_sAxPy, "saxpy");
		s.register_task(t, parentTask);//ToDo
		t->reads(a(i))->reads(b(i))->modifies(c(i));
	}
}

TEST(TestExecution, Hierarchical) {
	Scheduler s;
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));

	const int N = 10;
	Task* SAXPY = new Task(&s, Op_sAxPy, "SAXPY");	
	SAXPY->reads(A)->reads(B)->modifies(C);
	decompose_sAxPy(s, SAXPY, N);
	auto& children = SAXPY->get_children();
	Task* first_child = s.get_task(children[0]);
	decompose_sAxPy(s, first_child, N);

	s.main_loop();

	EXPECT_TRUE(s.verify_self()) << "Not all tasks are in their final state";
}

struct dim2 { int x, y; };
struct dim3 { int x, y, z; };

Data* create_tensor(Scheduler& s, const char* A, dim2 d, dim2 tile) {
	int _n = d.x * d.y * sizeof(int); \
		void* mem_A = malloc(_n);\
		Data* dA = s.register_data(A, mem_A, _n); \
		dA->partition(tile.x, tile.y);
	return dA;
}

void create_SumRow(string name, Scheduler& s, Data* A, Data* B, dim2 tiles) {
	int M = tiles.x;
	int N = tiles.y;
	Partition& a = *A->get_partition();
	Partition& b = *B->get_partition();

	for (int i = 0; i < M; ++i) {
		for (int j = 0; j < N; ++j) {
			char _name[100];
			snprintf(_name, sizeof(_name),  "_%02d_%02d", i, j);
			Task* t = new Task(&s, Op_SumReduction, _name);
			t->reads(a(i, j))->modifies(b(i));
		}
	}
}

void create_MatMul(string name, Scheduler& s, Data* A, Data* B, Data* C, dim2 tiles) {
	int M = tiles.x;
	int N = tiles.y;
	Partition& a = *A->get_partition();
	Partition& b = *B->get_partition();
	Partition& c = *C->get_partition();
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

void create_QK_SumRow(Scheduler& s, string tag) {
	dim2 dim_qk{ 128, 2048 };
	dim2 dim_d{ 128, 1 };
	dim2 tiles_qk{ 4, 4 };
	dim2 tiles_sum_row{ 4, 1 };
	auto Q = create_tensor(s, "Q", dim_qk, tiles_qk);
	auto K = create_tensor(s, "K", dim_qk, tiles_qk);
	auto C = create_tensor(s, "C", dim_qk, tiles_qk);
	auto D = create_tensor(s, "D", dim_d, tiles_sum_row);

	create_MatMul(string("QKMult") + tag, s, Q, K, C, tiles_qk);
	create_SumRow(string("SumRow") + tag, s, C, D, tiles_sum_row);
}


TEST(TestExecution, TaskPacks) {

	Logger::instance().set_level(LogLevel::Error);
	Scheduler s1, s2;
	create_QK_SumRow(s1, "_s1");
	create_QK_SumRow(s2, "_s2");
	vector<Scheduler*> sch;
	sch.push_back(&s1);
	sch.push_back(&s2);

	PackageManager pmngr(&sch, 20, 4, 20);
	DeviceTaskManager dmngr(&pmngr);
	pmngr.create_first_packs();
	dmngr.start();
	dmngr.finalize();
	EXPECT_TRUE(s1.verify_self());
	EXPECT_TRUE(s2.verify_self());	
}

TEST(TestExecution, TimingExecution) {
	Logger::instance().set_level(LogLevel::Error);
	
	dim2 shape, tiles;	
	int M_list[] = {128};	
	int Mb_list[] = {4};
	Scheduler s;
	vector<Scheduler*> sch_list{ &s };
	for (auto m : M_list) {
		for (auto mb : Mb_list) {
			for (bool with_task_pack : {false, true}) {
				for (int iter = 0; iter < 2; iter++) {
					shape.x = shape.y = m;
					tiles.x = tiles.y = mb;
					Data* A = create_tensor(s, "A", shape, tiles);
					Data* B = create_tensor(s, "B", shape, tiles);
					Data* C = create_tensor(s, "C", shape, tiles);
					create_MatMul("gemm", s, A, B, C, tiles);
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

