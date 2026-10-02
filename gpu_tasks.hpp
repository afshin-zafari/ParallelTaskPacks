#pragma once
#include "common.hpp"
#include "task.hpp"

struct GEMM_Op_Args { void* A{ nullptr }; void* B{ nullptr }; void* C{ nullptr }; float alpha{ 1.0 }; };
struct Scale_Op_Args { void* A{ nullptr }; void* B{ nullptr }; float alpha{ 1.0 }; };

class GPUTask {
private:
	scheduler_id_t sch_id{ -1 };
	task_id_t task_id{ -1 };
	op_code _op_code{ Op_None };
	union Args{
		GEMM_Op_Args gemm_args;
		Scale_Op_Args scale_args;
	} _args{};
public:
	GPUTask(Task*);
	GPUTask(op_code);
	op_code get_op_code() const { return _op_code; }
	task_id_t get_task_id() const { return task_id; }
	task_id_t get_scheduler_id() const { return sch_id; }
	const Args& get_args() const { return _args; }
};

class ArriveBarrierTask : public GPUTask {	
public:
	ArriveBarrierTask() : GPUTask(Op_arriveBarrier) {}
};

class WaitBarrierTask : public GPUTask {
public:
	WaitBarrierTask() : GPUTask(Op_waitBarrier) {}
};
class EndKernelsTask: public GPUTask {
public:
	EndKernelsTask() : GPUTask(Op_end_the_kernels) {}
};