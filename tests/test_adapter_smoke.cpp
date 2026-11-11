// Validation-only TU that instantiates all adapters and registers them.
// If this compiles + links, the Adapter infrastructure is usable.
#include <iostream>

#include "fixtures/adapters/hgraph_test_adapter.h"
#include "fixtures/adapters/ivf_test_adapter.h"
#include "fixtures/adapters/pyramid_test_adapter.h"
#include "fixtures/index_test_adapter.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

TEST_CASE("[phase1] adapter smoke: registry round-trip", "[adapter]") {
    using namespace fixtures;

    // Register each adapter.
    IndexTestRegistry::instance().register_adapter(
        "hgraph", std::make_unique<HGraphTestAdapter>(),
        IndexTestParams{"hgraph", "hgraph", 0.99F, {}});

    IndexTestRegistry::instance().register_adapter(
        "pyramid", std::make_unique<PyramidTestAdapter>(),
        IndexTestParams{"pyramid", "pyramid", 0.99F, {}});

    IndexTestRegistry::instance().register_adapter(
        "ivf", std::make_unique<IVFTestAdapter>(),
        IndexTestParams{"ivf", "ivf", 0.99F, {}});

    REQUIRE(IndexTestRegistry::instance().size() == 3);

    auto* h = IndexTestRegistry::instance().adapter_for("hgraph");
    REQUIRE(h != nullptr);

    auto* p = IndexTestRegistry::instance().adapter_for("pyramid");
    REQUIRE(p != nullptr);

    auto* i = IndexTestRegistry::instance().adapter_for("ivf");
    REQUIRE(i != nullptr);

    // Verify basic properties.
    REQUIRE_FALSE(h->default_build_params().empty());
    REQUIRE_FALSE(h->default_search_params().empty());
    REQUIRE_FALSE(p->default_build_params().empty());
    REQUIRE_FALSE(p->default_search_params().empty());
    REQUIRE_FALSE(i->default_build_params().empty());
    REQUIRE_FALSE(i->default_search_params().empty());

    std::cout << "[phase1] Adapter smoke test passed.\n";
}
