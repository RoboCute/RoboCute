#pragma once
#include <string>
#include <vector>

class RBCType {
  const RBCType *super_type_;
  std::string name_;
  std::vector<std::string> name_hierarchy_;

public:
  RBCType(const RBCType *p_super_type, std::string p_name);

  const RBCType *get_super_type() const { return super_type_; }
  const std::string &get_name() const { return name_; }
  const std::vector<std::string> &get_name_hierarchy() const {
    return name_hierarchy_;
  }
};