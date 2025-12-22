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
            "test_resource_blob");

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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));// Small delay to avoid busy waiting
    }

    LUISA_INFO("[UpdateThread] Stopped");
}

// =====================================================
// Helper function to get GUID for a resource
// =====================================================
vstd::Guid GetResourceGuid(int resource_id) {
    // Use fixed strings to generate deterministic GUIDs
    // These will always produce the same GUID for each resource_id
    luisa::string guid_seed;
    switch (resource_id) {
        case 1:
            guid_seed = "TestResource_00000001_FixedGuid";
            break;
        case 2:
            guid_seed = "TestResource_00000002_FixedGuid";
            break;
        case 3:
            guid_seed = "TestResource_00000003_FixedGuid";
            break;
        default:
            guid_seed = luisa::format("TestResource_{:08x}_FixedGuid", resource_id);
            break;
    }
    
    // Generate GUID from fixed string using MD5
    // This ensures the same string always produces the same GUID
    vstd::MD5 md5{guid_seed};
    auto bin = md5.to_binary();
    
    // Convert MD5 binary to GUID format
    // GUID is essentially a 128-bit value, same as MD5 hash
    std::array<uint64_t, 2> guid_data = {bin.data0, bin.data1};
    vstd::Guid guid = *reinterpret_cast<vstd::Guid*>(&guid_data);
    
    return guid;
}

// =====================================================
// Helper function to create test resources
// =====================================================
rbc::AsyncResource<TestResource> LoadTestResource(int resource_id, luisa::vector<ResourceHandle> dependencies = {}) {
    auto *system = GetResourceSystem();
    if (!system->IsInitialized()) {
        LUISA_ERROR("[Main] ResourceSystem not initialized!");
        return rbc::AsyncResource<TestResource>();
    }

    // Create unique GUID for each resource using a deterministic approach
    vstd::Guid guid = GetResourceGuid(resource_id);

    luisa::string resource_name = luisa::format("TestResource_{}", resource_id);
    int resource_value = resource_id * 100;

    auto guid_str = guid.to_string();
    if (dependencies.empty()) {
        LUISA_INFO("[Main] Enqueuing resource {}: {} (value: {}, no dependencies)", 
            resource_id, guid_str, resource_value);
    } else {
        LUISA_INFO("[Main] Enqueuing resource {}: {} (value: {}, {} dependencies)", 
            resource_id, guid_str, resource_value, dependencies.size());
        for (size_t i = 0; i < dependencies.size(); ++i) {
            auto dep_guid = dependencies[i].get_guid();
            LUISA_INFO("[Main]   Dependency {}: {}", i, dep_guid.to_string());
        }
    }

    // Create resource instance
    auto *resource = RBCNew<TestResource>(resource_name, resource_value);

    // Enqueue resource with Loading status (will be processed by Update thread)
    auto handle = system->EnqueueResource(
        guid,
        rbc::TypeInfo::get<TestResource>(),
        resource,
        true,// requireInstall
        std::move(dependencies),  // dependencies
        EResourceLoadingStatus::Loading);

    return rbc::AsyncResource<TestResource>(handle);
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
    LUISA_INFO("  1 - TestResource_1 (1st press: Load, 2nd press: Install, 3rd press: Unload)");
    LUISA_INFO("     Dependencies: TestResource_2");
    LUISA_INFO("  2 - TestResource_2 (1st press: Load, 2nd press: Install, 3rd press: Unload)");
    LUISA_INFO("     Dependencies: TestResource_3");
    LUISA_INFO("  3 - TestResource_3 (1st press: Load, 2nd press: Install, 3rd press: Unload)");
    LUISA_INFO("     Dependencies: None");
    LUISA_INFO("  exit - Exit program");
    LUISA_INFO("========================================");

    // Main loop: listen for keyboard input
    std::string input;
    luisa::vector<rbc::AsyncResource<TestResource>> resources;
    resources.resize_uninitialized(3);

    // Helper function to handle resource operations based on current state
    auto HandleResource = [&](int resource_id, int index) {
        auto &resource = resources[index];
        auto status = resource.get_status();
        
        LUISA_INFO("[Main] Resource {} - Current status: {}", resource_id, static_cast<uint32_t>(status));
        
        if (resource.is_null() || status == EResourceLoadingStatus::Unloaded) {
            // 第一次按下：加载资源
            LUISA_INFO("[Main] Resource {} - Step 1: Loading resource...", resource_id);
            
            // Setup dependencies based on resource_id
            luisa::vector<ResourceHandle> dependencies;
            if (resource_id == 1) {
                // Resource 1 depends on Resource 2
                if (resources[1].is_null()) {
                    // Create a handle for resource 2 using its GUID
                    vstd::Guid dep_guid = GetResourceGuid(2);
                    dependencies.emplace_back(dep_guid);
                    LUISA_INFO("[Main] Resource {} - Adding dependency on Resource 2 (GUID: {})", 
                        resource_id, dep_guid.to_string());
                } else {
                    // Use existing resource 2 handle
                    dependencies.emplace_back(resources[1]);
                    LUISA_INFO("[Main] Resource {} - Adding dependency on Resource 2 (existing handle)", resource_id);
                }
            } else if (resource_id == 2) {
                // Resource 2 depends on Resource 3
                // Resource 3 is at index 2 in the resources array
                if (resources[2].is_null()) {
                    // Create a handle for resource 3 using its GUID
                    vstd::Guid dep_guid = GetResourceGuid(3);
                    dependencies.emplace_back(dep_guid);
                    LUISA_INFO("[Main] Resource {} - Adding dependency on Resource 3 (GUID: {})", 
                        resource_id, dep_guid.to_string());
                } else {
                    // Use existing resource 3 handle
                    dependencies.emplace_back(resources[2]);
                    LUISA_INFO("[Main] Resource {} - Adding dependency on Resource 3 (existing handle)", resource_id);
                }
            }
            // Resource 3 has no dependencies
            
            resource = LoadTestResource(resource_id, std::move(dependencies));
            // Wait a bit for the update thread to process the loading request
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Try to load (this will trigger the loading process if not already loaded)
            void *loaded_ptr = resource.load();
            if (loaded_ptr) {
                LUISA_INFO("[Main] Resource {} loaded successfully! (status: {})", 
                    resource_id, static_cast<uint32_t>(resource.get_status()));
            } else {
                LUISA_WARNING("[Main] Resource {} loading in progress... (status: {})", 
                    resource_id, static_cast<uint32_t>(resource.get_status()));
            }
        } else if (resource.is_loaded() && !resource.is_installed()) {
            // 第二次按下：安装资源
            LUISA_INFO("[Main] Resource {} - Step 2: Installing resource...", resource_id);
            void *installed_ptr = resource.install();
            // Wait a bit for installation to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (resource.is_installed()) {
                LUISA_INFO("[Main] Resource {} installed successfully! (status: {})", 
                    resource_id, static_cast<uint32_t>(resource.get_status()));
            } else {
                LUISA_WARNING("[Main] Resource {} installation in progress... (status: {})", 
                    resource_id, static_cast<uint32_t>(resource.get_status()));
            }
        } else if (resource.is_installed()) {
            // 第三次按下：卸载资源
            LUISA_INFO("[Main] Resource {} - Step 3: Unloading resource...", resource_id);
            resource.reset();
            LUISA_INFO("[Main] Resource {} unloaded successfully! (status: {})", 
                resource_id, static_cast<uint32_t>(resource.get_status()));
        } else {
            // 其他状态（如 Loading, WaitingDependencies, Installing 等）
            LUISA_INFO("[Main] Resource {} is in intermediate state: {} (waiting for completion...)", 
                resource_id, static_cast<uint32_t>(status));
        }
    };

    while (true) {
        std::cout << "\n> ";
        std::cin >> input;

        if (input == "exit" || input == "quit" || input == "q") {
            LUISA_INFO("[Main] Exiting...");
            break;
        } else if (input == "1") {
            HandleResource(1, 0);
        } else if (input == "2") {
            HandleResource(2, 1);
        } else if (input == "3") {
            HandleResource(3, 2);
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