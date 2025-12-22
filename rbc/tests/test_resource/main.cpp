#include <rbc_core/type_info.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world_v2/resource.h>
#include <rbc_core/blob.h>
#include <rbc_core/memory.h>
#include <luisa/vstl/v_guid.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>

using namespace rbc;
using namespace luisa;

// =====================================================
// Test Resource Implementation
// =====================================================
struct TestResource : IResource {
    luisa::string name;
    int value = 0;

    TestResource(luisa::string_view n, int v) : name(n), value(v) {
        LUISA_INFO("[TestResource] Created: {} (value: {})", name, value);
    }

    ~TestResource() override {
        LUISA_INFO("[TestResource] Destroyed: {}", name);
    }

    void OnInstall() override {
        LUISA_INFO("[TestResource] OnInstall: {}", name);
    }

    void OnUninstall() override {
        LUISA_INFO("[TestResource] OnUninstall: {}", name);
    }

    // Implement BaseObject pure virtual functions
    void dispose() override {
        LUISA_INFO("[TestResource] dispose: {}", name);
        // Resource cleanup handled by ResourceSystem
    }

    [[nodiscard]] world::BaseObjectType base_type() const override {
        return world::BaseObjectType::Resource;
    }

    [[nodiscard]] const char *type_name() const override {
        return "TestResource";
    }

    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
        static vstd::MD5 md5{luisa::string_view{"TestResource"}};
        auto bin = md5.to_binary();
        return {bin.data0, bin.data1};
    }
};

RBC_RTTI(TestResource);

// =====================================================
// Test Resource Factory
// =====================================================
struct TestResourceFactory : ResourceFactory {
    rbc::TypeInfo GetResourceType() override {
        return rbc::TypeInfo::get<TestResource>();
    }

    EResourceInstallStatus Install(ResourceRecord *record) override {
        auto guid_str = record->header.guid.to_string();
        LUISA_INFO("[TestResourceFactory] Install resource: {}", guid_str);
        if (record->resource) {
            static_cast<TestResource *>(record->resource)->OnInstall();
        }
        return EResourceInstallStatus::Succeed;
    }

    bool Uninstall(ResourceRecord *record) override {
        auto guid_str = record->header.guid.to_string();
        LUISA_INFO("[TestResourceFactory] Uninstall resource: {}", guid_str);
        if (record->resource) {
            static_cast<TestResource *>(record->resource)->OnUninstall();
        }
        return true;
    }
};

// =====================================================
// Test Resource Registry Implementation
// =====================================================
struct TestResourceRegistry : ResourceRegistry {
    luisa::string_view _base_path;

    explicit TestResourceRegistry(luisa::string_view base_path) : _base_path(base_path) {
        LUISA_INFO("[TestResourceRegistry] Initialized with base_path: {}", base_path);
    }

    bool RequestResourceFile(ResourceRequest *request) override {
        if (!request) {
            LUISA_ERROR("[TestResourceRegistry] Invalid request");
            return false;
        }

        auto guid = request->GetGuid();
        auto guid_str = guid.to_string();
        LUISA_INFO("[TestResourceRegistry] RequestResourceFile: {}", guid_str);

        // Create a simple test blob with resource data
        // In real implementation, this would read from file system
        luisa::string test_data = luisa::format("TestResourceData_{}", guid_str);
        auto blob = rbc::IBlob::Create(
            reinterpret_cast<const uint8_t *>(test_data.data()),
            test_data.size(),
            false,
            "test_resource_blob"
        );

        // Access protected member through friend class mechanism
        // ResourceRegistry is friend of ResourceRequest (see resource.h line 121)
        // This means member functions of ResourceRegistry can access protected members
        // Since we inherit from ResourceRegistry, our member functions should also have access
        // However, some compilers may be strict about this, so we'll use a cast workaround
        struct ResourceRequestAccess : ResourceRequest {
            using ResourceRequest::blob;
        };
        static_cast<ResourceRequestAccess *>(request)->blob = std::move(blob);
        LUISA_INFO("[TestResourceRegistry] Created blob for resource: {} (size: {})", guid_str, test_data.size());

        // Note: Status transitions are handled by ResourceSystem
        return true;
    }

    bool CancelRequestFile(ResourceRequest *request) override {
        if (!request) {
            return false;
        }
        auto guid = request->GetGuid();
        auto guid_str = guid.to_string();
        LUISA_INFO("[TestResourceRegistry] CancelRequestFile: {}", guid_str);
        return true;
    }
};

// =====================================================
// Resource System Update Thread
// =====================================================
void ResourceSystemUpdateThread(std::atomic<bool> &running) {
    LUISA_INFO("[UpdateThread] Started");
    auto *system = GetResourceSystem();

    while (running.load()) {
        system->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay to avoid busy waiting
    }

    LUISA_INFO("[UpdateThread] Stopped");
}

// =====================================================
// Helper function to create test resources
// =====================================================
void LoadTestResource(int resource_id) {
    auto *system = GetResourceSystem();
    if (!system->IsInitialized()) {
        LUISA_ERROR("[Main] ResourceSystem not initialized!");
        return;
    }

    // Create unique GUID for each resource using a deterministic approach
    vstd::Guid guid{};
    // Use a deterministic string-based approach for GUIDs
    luisa::string guid_seed = luisa::format("TestResource_{:08x}", resource_id);
    auto parsed_guid = vstd::Guid::TryParseGuid(guid_seed);
    if (parsed_guid) {
        guid = *parsed_guid;
    } else {
        // Fallback: generate new GUID
        guid.remake();
    }

    luisa::string resource_name = luisa::format("TestResource_{}", resource_id);
    int resource_value = resource_id * 100;

    auto guid_str = guid.to_string();
    LUISA_INFO("[Main] Loading resource {}: {} (value: {})", resource_id, guid_str, resource_value);

    // Create resource instance
    auto *resource = RBCNew<TestResource>(resource_name, resource_value);

    // Enqueue resource with Loading status (will be processed by Update thread)
    auto handle = system->EnqueueResource(
        guid,
        rbc::TypeInfo::get<TestResource>(),
        resource,
        true, // requireInstall
        {},   // no dependencies
        EResourceLoadingStatus::Loading
    );

    LUISA_INFO("[Main] Resource {} enqueued, handle status: {}", resource_id, static_cast<uint32_t>(handle.get_status()));

    // Wait a bit for the update thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Try to load/install
    void *loaded_ptr = handle.load();
    if (loaded_ptr) {
        LUISA_INFO("[Main] Resource {} loaded successfully", resource_id);
    } else {
        LUISA_WARNING("[Main] Resource {} not yet loaded (status: {})", resource_id, static_cast<uint32_t>(handle.get_status()));
    }

    // Try to install
    void *installed_ptr = handle.install();
    if (installed_ptr) {
        LUISA_INFO("[Main] Resource {} installed successfully", resource_id);
    } else {
        LUISA_WARNING("[Main] Resource {} not yet installed (status: {})", resource_id, static_cast<uint32_t>(handle.get_status()));
    }
}

// =====================================================
// Main Function
// =====================================================
int main() {
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        RuntimeStaticBase::dispose_all();
    });

    LUISA_INFO("========================================");
    LUISA_INFO("ResourceSystem Test Framework Started");
    LUISA_INFO("========================================");

    // Initialize ResourceSystem
    auto *system = GetResourceSystem();
    auto *registry = RBCNew<TestResourceRegistry>("./test_resources");
    system->Initialize(registry);

    // Register factory
    auto *factory = RBCNew<TestResourceFactory>();
    system->RegisterFactory(factory);
    LUISA_INFO("[Main] Registered TestResourceFactory");

    // Start update thread
    std::atomic<bool> running{true};
    std::thread update_thread(ResourceSystemUpdateThread, std::ref(running));

    LUISA_INFO("[Main] ResourceSystem initialized");
    LUISA_INFO("[Main] Commands:");
    LUISA_INFO("  1 - Load TestResource_1");
    LUISA_INFO("  2 - Load TestResource_2");
    LUISA_INFO("  3 - Load TestResource_3");
    LUISA_INFO("  exit - Exit program");
    LUISA_INFO("========================================");

    // Main loop: listen for keyboard input
    std::string input;
    while (true) {
        std::cout << "\n> ";
        std::cin >> input;

        if (input == "exit" || input == "quit" || input == "q") {
            LUISA_INFO("[Main] Exiting...");
            break;
        } else if (input == "1") {
            LoadTestResource(1);
        } else if (input == "2") {
            LoadTestResource(2);
        } else if (input == "3") {
            LoadTestResource(3);
        } else {
            LUISA_WARNING("[Main] Unknown command: {}", input);
            LUISA_INFO("[Main] Available commands: 1, 2, 3, exit");
        }
    }

    // Shutdown
    LUISA_INFO("[Main] Shutting down ResourceSystem...");
    running.store(false);
    system->Quit();

    // Wait for update thread to finish
    if (update_thread.joinable()) {
        update_thread.join();
    }

    system->Shutdown();
    LUISA_INFO("[Main] ResourceSystem shutdown complete");

    // Cleanup
    RBCDelete(factory);
    RBCDelete(registry);

    LUISA_INFO("========================================");
    LUISA_INFO("Test Framework Exited");
    LUISA_INFO("========================================");

    return 0;
}