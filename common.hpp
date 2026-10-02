#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
//#define DEBUG_ONLY(code) code
#define DEBUG_ONLY(code) 

typedef size_t data_id_t;
typedef long task_id_t;
typedef long scheduler_id_t;
typedef unsigned long data_state_t;
typedef unsigned int label_t;

enum task_state_e : int {
    TASK_CREATED    = 0,
    TASK_REGISTERED = 1,
	TASK_QUEUED     = 2,
    TASK_STARTED    = 3,
    TASK_FINISHED   = 4,
	TASK_OUTPUT_CHECKED = 5
};

enum access_t : unsigned int {
    ACCESS_READ = 1,
    ACCESS_MODIFY = 2
};

enum op_code : uint8_t {
    Op_None,
    Op_arriveBarrier,
    Op_waitBarrier,
    Op_end_the_kernels,
    Op_GEMM,
    Op_SumReduction,
    Op_Square,
    Op_ReduceMax,
    Op_Scale,
    Op_sAxPy
};

std::string get_op_name(op_code op);