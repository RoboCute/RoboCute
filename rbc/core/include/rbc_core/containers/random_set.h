#pragma once
#include <luisa/core/stl.h>

// A Simple RandomSet that keeps single value at a time, support random fetch
template<typename T>
struct RandomSet {
    RandomSet() {
        srand((unsigned)time(NULL));
    }
    bool insert(T val) {
        if (indices.count(val)) {
            return false;
        }
        int index = vals.size();
        vals.emplace_back(val);
        indices[val] = index;
        return true;
    }
    bool remove(T val) {
        if (!indices.count(val)) {
            return false;
        }
        int index = indices[val];
        vals[index] = vals.back();// move back to "to remove" index
        indices[vals.back()] = index;
        // remove last
        vals.pop_back();
        indices.erase(val);
    }

    int getRandom() {
        int random_index = rand() % vals.size();
        return vals[random_index];
    }
private:
    luisa::vector<T> vals;
    luisa::unordered_map<T, int> indices;
};