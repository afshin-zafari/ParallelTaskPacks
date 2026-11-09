#pragma once
#include "common.hpp"
#include "cuda_code.hpp"
#include "task.hpp"

struct TileMemory {
	void* dev_mem{ nullptr };
	size_t dev_mem_size{ 0 };
	size_t parent_row_size{ 0 };
	size_t elem_size{ 0 };
	size_t rows{0}, cols{0};
};

struct GEMM_Op_Args { TileMemory operands[3]; float alpha{ 1.0 }; };
struct Scale_Op_Args { TileMemory operands[2]; float alpha{ 1.0 }; };


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
#ifdef __CUDA
	__host__ __device__	
#endif
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