
// Copyright 2024-present the vsag project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "graph_interface_test.h"

#include <fstream>

#include "catch2/catch_test_macros.hpp"
#include "fixtures.h"

using namespace vsag;

void
GraphInterfaceTest::BasicTest(uint64_t max_id,
                              uint64_t count,
                              std::shared_ptr<GraphInterface> other) {
    auto max_degree = this->graph_->MaximumDegree();
    auto old_count = this->graph_->TotalCount();
    std::unordered_map<uint64_t, std::vector<uint64_t>> maps;
    for (auto i = 0; i < count; ++i) {
        auto length = random() % max_degree;
        std::vector<uint64_t> ids(length);
        for (auto& id : ids) {
            id = random() % max_id;
        }
        auto cur_id = random() % max_id;
        maps[cur_id] = ids;
    }

    for (auto& [key, value] : maps) {
        this->graph_->InsertNeighborsById(key, value);
        this->graph_->IncreaseTotalCount(1);
    }

    // Test GetNeighborSize
    SECTION("Test GetNeighborSize") {
        for (auto& [key, value] : maps) {
            REQUIRE(this->graph_->GetNeighborSize(key) == value.size());
        }
    }

    // Test GetNeighbors
    SECTION("Test GetNeighbors") {
        for (auto& [key, value] : maps) {
            std::vector<uint64_t> neighbors;
            this->graph_->GetNeighbors(key, neighbors);
            REQUIRE(memcmp(neighbors.data(), value.data(), value.size() * sizeof(uint64_t)) == 0);
        }
    }

    // Test Others
    SECTION("Test Others") {
        REQUIRE(this->graph_->TotalCount() == old_count + maps.size());
        REQUIRE(this->graph_->MaxCapacity() > this->graph_->TotalCount());
        REQUIRE(this->graph_->MaximumDegree() == max_degree);
    }

    SECTION("Serialize & Deserialize") {
        fixtures::temp_dir dir("");
        auto path = dir.path + "/graph";
        std::ofstream outfile(path.c_str(), std::ios::binary);
        IOStreamWriter writer(outfile);
        this->graph_->Serialize(writer);
        outfile.close();

        std::ifstream infile(path.c_str(), std::ios::binary);
        IOStreamReader reader(infile);
        other->Deserialize(reader);

        REQUIRE(this->graph_->TotalCount() == other->TotalCount());
        REQUIRE(this->graph_->MaxCapacity() == other->MaxCapacity());
        REQUIRE(this->graph_->MaximumDegree() == other->MaximumDegree());

        for (auto& [key, value] : maps) {
            std::vector<uint64_t> neighbors;
            other->GetNeighbors(key, neighbors);
            REQUIRE(memcmp(neighbors.data(), value.data(), value.size() * sizeof(uint64_t)) == 0);
        }

        infile.close();
    }
}