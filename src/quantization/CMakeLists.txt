
set (QUANTIZER_SRC
        quantizer_parameter.cpp
        fp32_quantizer_parameter.cpp
        scalar_quantization/sq8_quantizer_parameter.cpp
        scalar_quantization/sq8_uniform_quantizer_parameter.cpp
        scalar_quantization/sq4_quantizer_parameter.cpp
        scalar_quantization/sq4_uniform_quantizer_parameter.cpp
        scalar_quantization/bf16_quantizer_parameter.cpp
        scalar_quantization/scalar_quantization_trainer.cpp
        scalar_quantization/fp16_quantizer_parameter.cpp
        rabitq_quantization/rabitq_quantizer_parameter.cpp
        product_quantization/product_quantizer_parameter.cpp
        product_quantization/pq_fastscan_quantizer_parameter.cpp
)

add_library (quantizer OBJECT ${QUANTIZER_SRC})
# if(TARGET openblas)
#     target_link_libraries(quantizer PRIVATE OpenBLAS)
# endif()
# if(TARGET mkl)
#     target_link_libraries(quantizer PRIVATE mkl)
# endif ()
# target_link_libraries(quantizer PRIVATE fmt::fmt)
maybe_add_dependencies (quantizer spdlog openblas mkl)