#include <iostream>
#include <string>
#include <assert.h>
#include <thread>

#include "logger.hpp"
#include "task.hpp"
#include "scheduler.hpp"
#include "data.hpp"
#include "partitioner.hpp"
#include "device_task_manager.hpp"
#include "package_manager.hpp"


unsigned int test_failed_count = 0;
class TestCase {
public:
	string name;
	TestCase(const char *s) {
		test_failed_count = 0;
		name = s;
	}
	~TestCase() {
		if (test_failed_count == 0)
			std::cout << "\n" << name << " passed.\n";
	}
};

#define TEST_NAME(name) TestCase _test_case(name);

#define ASSERT(cond, msg) \
	do { \
		if (!(cond)) { \
			std::cout << "Test: '" << _test_case.name \
					  << "' failed: \n" << (msg) \
					  << ", cond: '" << #cond \
					  << "', File: " << __FILE__ \
					  << ", Line: " << __LINE__ \
					  << std::endl; \
			test_failed_count++; \
		} \
	} while (false)

static void test_scheduler() {
	TEST_NAME("Scheduler Test");
	Scheduler s;
	char mem[256];
	Data* a = s.register_data("A", mem, 16);
	Data* b = s.register_data("B", mem + 16, 16);

	Task t;
	t.reads(a)->modifies(b);
	task_id_t task_id = s.register_task(&t);	
	ASSERT(task_id == 0, "Expected task ID 0");
	ASSERT(a->get_id() == 0, "Expected data ID 0");
	ASSERT(b->get_id() == 1, "Expected data ID 1");
	Task t2;
	t2.reads(b);
	task_id_t task_id2 = s.register_task(&t2);
	ASSERT(task_id2 == 1, "Expected task ID 1");
	ASSERT(a->get_last_read() == 1, "Expected last read label 1");
	ASSERT(b->get_last_read() == 2, "Expected last read label 2");
	ASSERT(b->get_last_modify() == 1, "Expected last modify label 1");
	s.dump_tasks();
	ASSERT(t.can_run() == true, "Task t should be able to run");
	ASSERT(t2.can_run() == false, "Task t2 should not be able to run yet");
	t.on_finished();
	ASSERT(t2.can_run() == true, "Task t2 should be able to run after t");
}
#define Reads(t, exp) Task t; t.reads(a);s.register_task(&t); ASSERT(t.is_waiting_for(0,exp), "should be");
#define Modifies(t, exp) Task t; t.modifies(a);s.register_task(&t); ASSERT(t.is_waiting_for(0,exp), "should be");
void test_read_write_sequence() {
	TEST_NAME("Read Write Seq");
	Scheduler s;
	char mem[256];
	Data* a = s.register_data("A", mem, 16);	
	Reads(t1, 0);
	Reads(t2, 0);
	Reads(t3, 0);
	Modifies(t4, 3);
	Reads(t5, 4);
	s.dump_tasks();
}
void test_write_after_write_sequence() {
	TEST_NAME("Read After Write");
	Scheduler s;
	char mem[256];
	Data* a = s.register_data("A", mem, 16);	
	Reads(t1, 0);
	Modifies(t2, 1);
	Modifies(t3, 2);
	Modifies(t4, 3);
	Reads(t5, 4);
	s.dump_tasks();
}
void test_partition() {
	TEST_NAME("Partition Test");
	Scheduler s;
	char mem[256];
	Data* a = s.register_data("A", mem, 256);
	Partition p(a, 2, 2);
	Data* d00 = p(0, 0);
	Data* d01 = p(0, 1);
	Data* d10 = p(1, 0);
	Data* d11 = p(1, 1);
	ASSERT(d00->get_id() == 1, "Expected data ID 1");
	ASSERT(d01->get_id() == 2, "Expected data ID 2");
	ASSERT(d10->get_id() == 3, "Expected data ID 3");
	ASSERT(d11->get_id() == 4, "Expected data ID 4");
	// assert that address of d01 is correct
	ASSERT(d01->get_address() == mem + 10, "Expected address of d01");
	ASSERT(d00->get_address() == mem, "Expected address of d00");
	// and for d11
	ASSERT(d11->get_address() == mem + 30, "Expected address of d11");
}
void test_partition_with_tasks() {
	TEST_NAME("Partition with Tasks Test");
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
		s.register_task(t);
	}
	for (int i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
		s.register_task(t);
	}
	for (int i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "reduce_" + std::to_string(i));
		t->reads(c(i))->modifies(c(0));
		s.register_task(t);
	}
	s.dump_tasks();
	size_t M = s.task_count();
	auto finish_running_tasks = [&]() {
		for (int i = 0; i < M; ++i) { s.get_task(i)->on_finished(); }
	};
	auto simulate_run = [&](const char* step) {
		std::cout << step << std::endl;
		s.run_tasks();
		finish_running_tasks();
	};
	simulate_run("Step 1: square ...");
	simulate_run("Step 2: sum ...");	
	for (int i = 0; i < M; ++i) {
		simulate_run(("Step 3, reduce round " + std::to_string(i)).c_str());
	}
	ASSERT(s.verify_self(), "Not all tasks are in their final state");
}
void test_main_loop() {
	TEST_NAME("Partition with Scheduler main_loop");
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
		s.register_task(t);
	}
	for (int i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
		s.register_task(t);
	}
	for (int i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_ReduceMax, "reduce_" + std::to_string(i));
		t->reads(c(i))->modifies(c(0));
		s.register_task(t);
	}
	s.main_loop();
	ASSERT(s.verify_self(), "Not all tasks are in their final state");
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
void test_consumer_threads() {
	TEST_NAME("Partition with Scheduler main_loop and consumer threads");
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
		s.register_task(t);
	}
	for (int i = 0; i < N; i += 2) {
		Task* t = new Task(&s, Op_SumReduction, "sum_" + std::to_string(i));
		t->reads(b(i))->reads(b(i + 1))->modifies(c(i));
		s.register_task(t);
	}
	for (int i = 2; i < N; i += 2) {
		Task* t = new Task(&s, Op_ReduceMax, "reduce_" + std::to_string(i));
		t->reads(c(i))->modifies(c(0));
		s.register_task(t);
	}
	
	std::thread t1(consumer_kernel, std::ref(s));
	std::thread t2(consumer_kernel, std::ref(s));
	t1.join();
	t2.join();
	s.dump_tasks();
	ASSERT(s.verify_self(), "Not all tasks are in their final state");
}
void test_sugarring_syntax() {
	TEST_NAME("Sugaring Syntax Test");
	Scheduler s;
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));
	const int N = 10;
	Partition a(A, 1, N);
	Partition b(B, 1, N);
	Partition c(C, 1, N);
	auto task_2_args = [&](string t_name,auto d1, auto d2) {
		Task* t = new Task(&s, Op_SumReduction, t_name);
		t->reads(d1)->modifies(d2);
		s.register_task(t);
		}; 
	auto task_3_args = [&](string t_name,Data* d1, Data *d2, Data *d3) {
		Task* t = new Task(&s, Op_GEMM, t_name);
		t->reads(d1)->reads(d2)->modifies(d3);
		s.register_task(t);
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
	ASSERT(s.verify_self(), "Not all tasks are in their final state");
}
void decompose_sAxPy(Scheduler &s, Task *parentTask, const int parts) {	
	auto dataArgs = parentTask->get_data();
	Data* A = s.get_data(dataArgs[0]);
	Data* B = s.get_data(dataArgs[1]);
	Data* C = s.get_data(dataArgs[2]);
	
	Partition a(A, 1, parts);
	Partition b(B, 1, parts);
	Partition c(C, 1, parts);
	for (int i = 0; i < parts; i++) {
		Task* t = new Task(&s, Op_sAxPy, "saxpy");
		t->reads(a(i))->reads(b(i))->modifies(c(i));
		s.register_task(t, parentTask);
	}
}
void test_hierarchical_tasks() {
	TEST_NAME("Hierarchical tasks");
	Scheduler s;
	char mem_a[256], mem_b[256], mem_c[256];
	Data* A = s.register_data("A", mem_a, sizeof(mem_a));
	Data* B = s.register_data("B", mem_b, sizeof(mem_b));
	Data* C = s.register_data("C", mem_c, sizeof(mem_c));

	const int N = 10;
	Task* SAXPY = new Task(&s, Op_sAxPy,"SAXPY");
	SAXPY->reads(A)->reads(B)->modifies(C);
	s.register_task(SAXPY);
	decompose_sAxPy(s, SAXPY, N);
	auto& children = SAXPY->get_children();
	Task* first_child = s.get_task(children[0]);
	decompose_sAxPy(s, first_child, N);

	s.main_loop();

	ASSERT(s.verify_self(), "Not all tasks are in their final state");

}
struct dim2 { int x, y; };
struct dim3 { int x, y, z; };
Data* CreateTensor(Scheduler& s, const char* A, dim2 d, dim2 tile) {
	int _n = d.x * d.y * sizeof(float); \
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
			snprintf(_name, sizeof(_name), "_%02d_%02d", i, j);
			Task* t = new Task(&s, Op_SumReduction, _name);
			t->reads(a(i, j))->modifies(b(i));
			s.register_task(t);
		}
	}
}
void create_MatMul(string name, Scheduler &s, Data *A, Data*B, Data* C, dim2 tiles) {
	int M = tiles.x;
	int N = tiles.y;
	Partition &a = *A->get_partition();
	Partition &b = *B->get_partition();
	Partition &c = *C->get_partition();
	for (int i = 0; i < M; ++i) {
		for (int j = 0; j < N; ++j) {
			for (int k = 0; k < N; ++k) {
				char _name[100];
				snprintf(_name, sizeof(_name), "_%02d_%02d_%02d", i, k, j);
				Task* t = new Task(&s,Op_GEMM, _name);
				t->reads(a(i, k))->reads(b(j,k))->modifies(c(i, j));
				s.register_task(t);
			}
		}
	}
}
void create_QK_SumRow(Scheduler& s, string tag) {
	dim2 dim_qk{ 128, 2048 };
	dim2 dim_d{ 128, 1 };
	dim2 tiles_qk{ 4, 4 };
	dim2 tiles_sum_row{ 4, 1 };
	auto Q = CreateTensor(s, "Q", dim_qk, tiles_qk);
	auto K = CreateTensor(s, "K", dim_qk, tiles_qk);
	auto C = CreateTensor(s, "C", dim_qk, tiles_qk);
	auto D = CreateTensor(s, "D", dim_d, tiles_sum_row);

	create_MatMul(string("QKMult") + tag, s, Q, K, C, tiles_qk);
	create_SumRow(string("SumRow") + tag, s, C, D, tiles_sum_row);

}
void  test_task_packs() {	
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
	LOG_DEBUG("Sample");
}
int main(){
	/*
	test_scheduler();
	test_read_write_sequence();
	test_write_after_write_sequence();
	test_partition();
	test_partition_with_tasks();
	test_main_loop();
	test_consumer_threads();
	test_sugarring_syntax();
	test_hierarchical_tasks();	
	*/
	test_task_packs();
}