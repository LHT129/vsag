
add_subdirectory (allocator)
add_subdirectory (filter)
add_subdirectory (heap)
add_subdirectory (transform)
add_subdirectory (bitset)

file (GLOB IMPL_SRCS "*.cpp")
list (FILTER IMPL_SRCS EXCLUDE REGEX "_test.cpp")

add_library (impl OBJECT ${IMPL_SRCS})
target_link_libraries (impl PRIVATE transform allocator heap bitset filter fmt::fmt)
maybe_add_dependencies (impl spdlog)

set (IMPL_LIBS transform allocator heap bitset filter impl PARENT_SCOPE)
