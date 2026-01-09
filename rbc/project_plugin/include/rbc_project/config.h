#pragma once
/**
 * Project Configurations
 */

// TODO: let all of the config generated through python
namespace rbc {
enum struct ConfigLevel {
    LOCAL,  // per-machine
    USER,   // per-login
    PROJECT,// per-project
};

struct LocalConfig {
};

struct UserConfig {
};

struct ProjectConfig {
};

}// namespace rbc