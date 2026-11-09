#include "gpu_tasks.hpp"
#include "scheduler.hpp"
#include "task.hpp"

GPUTask::GPUTask(Task* task) : _args{} {
	this->_op_code = task->get_op_code();
	task_id = task->get_id();
	sch_id = task->get_scheduler()->get_id();
	switch (_op_code) {
	case Op_GEMM:		
		task->get_tile_info(0, _args.gemm_args.operands[0]);
		task->get_tile_info(1, _args.gemm_args.operands[1]);
		task->get_tile_info(2, _args.gemm_args.operands[2]);
		_args.gemm_args.alpha = task->get_extra_arg(0);
		break;
	case Op_Scale:
		task->get_tile_info(0, _args.scale_args.operands[0]);
		task->get_tile_info(1, _args.scale_args.operands[1]);
		_args.scale_args.alpha = task->get_extra_arg(0);
		break;
	default:
		break;
	}
}

GPUTask::GPUTask(op_code op) : sch_id{ 0 }, task_id { 0 }, _op_code{ op }, _args{} {
}