
set (EVAL_SRCS vsag_eval.cpp eval_dataset.cpp)
add_executable (vsag_eval ${EVAL_SRCS})
target_link_libraries (vsag_eval vsag)