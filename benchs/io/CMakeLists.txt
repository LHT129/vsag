# Copyright 2024-present the vsag project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_executable (io_micro_benchmark io_micro_benchmark.cpp)
target_link_libraries (io_micro_benchmark PRIVATE vsag_static)

add_executable (io_backend_profile_benchmark io_backend_profile_benchmark.cpp)
target_link_libraries (io_backend_profile_benchmark PRIVATE vsag_static)

add_executable (io_storage_profile_benchmark io_storage_profile_benchmark.cpp)
target_link_libraries (io_storage_profile_benchmark PRIVATE vsag_static)
