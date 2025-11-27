//
//  json_serializer.hpp
//  PeerClient
//
//  Created by jackie.ou on 2025/6/20.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <tuple>
#include <utility>
#include <set>
#include <list>
#include <deque>
#include <forward_list>
#include <array>
#include <variant>
#include <functional>
#include <memory> // Required for std::unique_ptr
#include <fstream>
#include <chrono>
#include <algorithm>
#include <queue>
#include <stack>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/error/en.h"

namespace json_serializer {

struct SerializeError;

inline std::string format_error(const SerializeError& error);

// Enhanced error handling with detailed path information
struct SerializeError {
    enum class ErrorCode {
        NONE,
        PARSE_ERROR,
        MISSING_FIELD,
        TYPE_MISMATCH,
        VALIDATION_ERROR,
        CUSTOM_ERROR,
        RECURSION_DEPTH_EXCEEDED,
        MEMORY_ERROR
    };

    ErrorCode code = ErrorCode::NONE;
    std::string message;
    std::string path;
    std::vector<std::string> context;  // Additional context information

    inline bool has_error() const {
        return code != ErrorCode::NONE;
    }

    // Add method to build nested paths
    static inline std::string build_path(const std::string& parent, const std::string& child) {
        if (parent.empty()) {
            return child;
        }
        return parent + "." + child;
    }

    // Add method to append to path
    inline void append_path(const std::string& part) {
        if (path.empty()) {
            path = part;
        } else {
            path += "." + part;
        }
    }

    // Add context information
    inline void add_context(const std::string& info) {
        context.push_back(info);
    }

    // Get full error description
    inline std::string get_full_description() const {
        std::string result = format_error(*this);
        if (!context.empty()) {
            result += "\nContext:";
            for (const auto& ctx : context) {
                result += "\n  " + ctx;
            }
        }
        return result;
    }
};

// Enhanced serialization options with validation
struct SerializeOptions {
    bool pretty_print = false;
    std::vector<std::string> included_fields;  // If non-empty, only include these fields
    std::vector<std::string> excluded_fields;  // If non-empty, exclude these fields

    size_t buffer_reserve_size = 1024;         // Buffer pre-allocation size

    using ValidationFunction = std::function<SerializeError(const rapidjson::Value&)>;
    ValidationFunction custom_validator;

    // Field validation rules
    struct FieldValidation {
        std::string field_name;
        std::function<bool(const rapidjson::Value&)> validator;
        std::string error_message;
    };
    std::vector<FieldValidation> field_validations;

    // Add validation rule
    inline void add_validation(const std::string& field,
                               std::function<bool(const rapidjson::Value&)> validator,
                               const std::string& error_msg = "") {
        field_validations.push_back({field, validator, error_msg});
    }
};

// Helper type trait is_optional
template<typename T>
struct is_optional {
    static constexpr bool value = false;
};

template<typename T>
struct is_optional<std::optional<T>> {
    static constexpr bool value = true;
};

template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

// Generic default value provider to reduce code duplication
template<typename T>
struct DefaultValueProvider {
    static T default_value() {
        return T{};
    }
};

// Specialization for basic types
template<>
struct DefaultValueProvider<int> {
    static int default_value() { return 0; }
};

template<>
struct DefaultValueProvider<float> {
    static float default_value() { return 0.0f; }
};

template<>
struct DefaultValueProvider<double> {
    static double default_value() { return 0.0; }
};

template<>
struct DefaultValueProvider<std::string> {
    static std::string default_value() { return ""; }
};

template<>
struct DefaultValueProvider<bool> {
    static bool default_value() { return false; }
};

// Generic container default value provider
template<template<typename...> class Container, typename... Args>
struct DefaultValueProvider<Container<Args...>> {
    static Container<Args...> default_value() { return {}; }
};

// Specialization for optional
template<typename T>
struct DefaultValueProvider<std::optional<T>> {
    static std::optional<T> default_value() { return std::nullopt; }
};

// Specialization for array (needs special handling due to size parameter)
template<typename T, std::size_t N>
struct DefaultValueProvider<std::array<T, N>> {
    static std::array<T, N> default_value() {
        std::array<T, N> result{};
        return result;
    }
};

// Specialization for tuple
template<typename... Ts>
struct DefaultValueProvider<std::tuple<Ts...>> {
    static std::tuple<Ts...> default_value() { return std::tuple<Ts...>{}; }
};

// Specialization for variant
template<typename... Ts>
struct DefaultValueProvider<std::variant<Ts...>> {
    static std::variant<Ts...> default_value() {
        // Default initialize to the first type's default value
        return std::variant<Ts...>{std::in_place_index<0>, DefaultValueProvider<std::tuple_element_t<0, std::tuple<Ts...>>>::default_value()};
    }
};

// Specialization for pair
template<typename T1, typename T2>
struct DefaultValueProvider<std::pair<T1, T2>> {
    static std::pair<T1, T2> default_value() {
        return std::pair<T1, T2>{
            DefaultValueProvider<T1>::default_value(),
            DefaultValueProvider<T2>::default_value()
        };
    }
};

// Specialization for enum types
template<typename T>
    requires std::is_enum_v<T>
struct DefaultValueProvider<T> {
    static T default_value() { return static_cast<T>(0); }
};

// Specialization for queue types
template<typename T, typename Container>
struct DefaultValueProvider<std::queue<T, Container>> {
    static std::queue<T, Container> default_value() { return std::queue<T, Container>{}; }
};

template<typename T, typename Container>
struct DefaultValueProvider<std::priority_queue<T, Container>> {
    static std::priority_queue<T, Container> default_value() { return std::priority_queue<T, Container>{}; }
};

template<typename T, typename Container>
struct DefaultValueProvider<std::stack<T, Container>> {
    static std::stack<T, Container> default_value() { return std::stack<T, Container>{}; }
};

// Forward declaration
template<typename T>
struct Serializer;

// Generic container serialization template to reduce code duplication
template<template<typename> class Container, typename T>
struct GenericContainerSerializer {
    static void serialize(const Container<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        if constexpr (std::is_same_v<Container<T>, std::vector<T>>) {
            json_value.Reserve(value.size(), allocator);
        }

        for (const auto& item : value) {
            rapidjson::Value item_value;
            Serializer<T>::serialize(item, item_value, allocator);
            json_value.PushBack(item_value, allocator);
        }
    }

    static Container<T> deserialize(const rapidjson::Value& json_value) {
        Container<T> result;
        if (!json_value.IsArray()) {
            return result;
        }

        if constexpr (std::is_same_v<Container<T>, std::vector<T>>) {
            result.reserve(json_value.Size());
        }

        for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
            if constexpr (std::is_same_v<Container<T>, std::set<T>> ||
                          std::is_same_v<Container<T>, std::multiset<T>> ||
                          std::is_same_v<Container<T>, std::unordered_set<T>> ||
                          std::is_same_v<Container<T>, std::unordered_multiset<T>>) {
                result.insert(Serializer<T>::deserialize(json_value[i]));
            } else if constexpr (std::is_same_v<Container<T>, std::forward_list<T>>) {
                if (i == 0) {
                    result = std::forward_list<T>{Serializer<T>::deserialize(json_value[i])};
                } else {
                    auto it = result.before_begin();
                    for (size_t j = 0; j < i; ++j) ++it;
                    result.insert_after(it, Serializer<T>::deserialize(json_value[i]));
                }
            } else {
                result.push_back(Serializer<T>::deserialize(json_value[i]));
            }
        }
        return result;
    }
};

// Generic map serialization template
template<template<typename, typename> class Map, typename K, typename V>
struct GenericMapSerializer {
    static void serialize(const Map<K, V>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();
        for (const auto& [key, item] : value) {
            rapidjson::Value item_value;
            Serializer<V>::serialize(item, item_value, allocator);

            if constexpr (std::is_same_v<K, std::string>) {
                json_value.AddMember(rapidjson::Value(key.c_str(), allocator).Move(), item_value, allocator);
            } else {
                std::string key_str = std::to_string(key);
                json_value.AddMember(rapidjson::Value(key_str.c_str(), allocator).Move(), item_value, allocator);
            }
        }
    }

    static Map<K, V> deserialize(const rapidjson::Value& json_value) {
        Map<K, V> result;
        if (!json_value.IsObject()) {
            return result;
        }

        for (auto it = json_value.MemberBegin(); it != json_value.MemberEnd(); ++it) {
            if constexpr (std::is_same_v<K, std::string>) {
                result[it->name.GetString()] = Serializer<V>::deserialize(it->value);
            } else if constexpr (std::is_integral_v<K>) {
                try {
                    result[static_cast<K>(std::stoll(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
                } catch (...) {
                    // Malformed key, skip this entry
                }
            } else if constexpr (std::is_floating_point_v<K>) {
                try {
                    result[static_cast<K>(std::stod(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
                } catch (...) {
                    // Malformed key, skip this entry
                }
            }
        }
        return result;
    }
};

// Generic multimap serialization template
template<template<typename, typename> class MultiMap, typename K, typename V>
struct GenericMultiMapSerializer {
    static void serialize(const MultiMap<K, V>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        for (const auto& [key, item] : value) {
            rapidjson::Value pair_value;
            pair_value.SetObject();

            rapidjson::Value key_value;
            Serializer<K>::serialize(key, key_value, allocator);
            pair_value.AddMember("key", key_value, allocator);

            rapidjson::Value value_value;
            Serializer<V>::serialize(item, value_value, allocator);
            pair_value.AddMember("value", value_value, allocator);

            json_value.PushBack(pair_value, allocator);
        }
    }

    static MultiMap<K, V> deserialize(const rapidjson::Value& json_value) {
        MultiMap<K, V> result;
        if (!json_value.IsArray()) {
            return result;
        }

        for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
            const auto& pair_obj = json_value[i];
            if (pair_obj.IsObject() && pair_obj.HasMember("key") && pair_obj.HasMember("value")) {
                K key = Serializer<K>::deserialize(pair_obj["key"]);
                V value = Serializer<V>::deserialize(pair_obj["value"]);
                result.emplace(key, value);
            }
        }
        return result;
    }
};

// Generic smart pointer serialization template
template<template<typename> class SmartPtr, typename T>
struct GenericSmartPtrSerializer {
    static void serialize(const SmartPtr<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        if (value) {
            Serializer<T>::serialize(*value, json_value, allocator);
        } else {
            json_value.SetNull();
        }
    }

    static SmartPtr<T> deserialize(const rapidjson::Value& json_value) {
        if (json_value.IsNull()) {
            return nullptr;
        }
        if constexpr (std::is_same_v<SmartPtr<T>, std::unique_ptr<T>>) {
            return std::make_unique<T>(Serializer<T>::deserialize(json_value));
        } else if constexpr (std::is_same_v<SmartPtr<T>, std::shared_ptr<T>>) {
            return std::make_shared<T>(Serializer<T>::deserialize(json_value));
        }
        return nullptr;
    }
};

// Generic queue serializer for std::queue, std::priority_queue, std::stack
template<template<typename, typename> class Queue, typename T, typename Container>
struct GenericQueueSerializer {
    static void serialize(const Queue<T, Container>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();

        // Create a copy to iterate through elements
        Queue<T, Container> temp_queue = value;

        // For queue and stack, we can iterate through elements
        if constexpr (std::is_same_v<Queue<T, Container>, std::queue<T, Container>>) {
            while (!temp_queue.empty()) {
                rapidjson::Value item_value;
                Serializer<T>::serialize(temp_queue.front(), item_value, allocator);
                json_value.PushBack(item_value, allocator);
                temp_queue.pop();
            }
        } else if constexpr (std::is_same_v<Queue<T, Container>, std::stack<T, Container>>) {
            // For stack, we need to serialize in insertion order (bottom to top)
            // First, extract all elements to a vector in reverse order
            std::vector<T> elements;
            while (!temp_queue.empty()) {
                elements.push_back(temp_queue.top());
                temp_queue.pop();
            }
            
            // Serialize elements in reverse order to maintain insertion order
            for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
                rapidjson::Value item_value;
                Serializer<T>::serialize(*it, item_value, allocator);
                json_value.PushBack(item_value, allocator);
            }
        } else if constexpr (std::is_same_v<Queue<T, Container>, std::priority_queue<T, Container>>) {
            // For priority_queue, we need to extract elements in order
            std::vector<T> elements;
            while (!temp_queue.empty()) {
                elements.push_back(temp_queue.top());
                temp_queue.pop();
            }

            // Serialize elements in reverse order to maintain priority queue order
            for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
                rapidjson::Value item_value;
                Serializer<T>::serialize(*it, item_value, allocator);
                json_value.PushBack(item_value, allocator);
            }
        }
    }

    static Queue<T, Container> deserialize(const rapidjson::Value& json_value) {
        Queue<T, Container> result;
        if (!json_value.IsArray()) {
            return result;
        }

        // For queue and stack, push elements in order
        if constexpr (std::is_same_v<Queue<T, Container>, std::queue<T, Container>>) {
            for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
                result.push(Serializer<T>::deserialize(json_value[i]));
            }
        } else if constexpr (std::is_same_v<Queue<T, Container>, std::stack<T, Container>>) {
            // For stack, push elements in the order they appear in JSON (insertion order)
            for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
                result.push(Serializer<T>::deserialize(json_value[i]));
            }
        } else if constexpr (std::is_same_v<Queue<T, Container>, std::priority_queue<T, Container>>) {
            // For priority_queue, push elements in reverse order to maintain correct heap structure
            for (rapidjson::SizeType i = json_value.Size(); i > 0; i--) {
                result.push(Serializer<T>::deserialize(json_value[i - 1]));
            }
        }

        return result;
    }
};

// Basic type serialization/deserialization
template<typename T>
struct BasicTypeSerializer {
    static void serialize(const T& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, long> || std::is_same_v<T, long long>) {
            json_value.SetInt64(value);
        } else if constexpr (std::is_same_v<T, unsigned int> || std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned long long>) {
            json_value.SetUint64(value);
        } else if constexpr (std::is_same_v<T, float>) {
            json_value.SetFloat(value);
        } else if constexpr (std::is_same_v<T, double>) {
            json_value.SetDouble(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            json_value.SetBool(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            json_value.SetString(value.c_str(), value.length(), allocator);
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported basic type");
        }
    }

    static T deserialize(const rapidjson::Value& json_value) {
        if constexpr (std::is_same_v<T, int>) {
            return json_value.IsInt() ? json_value.GetInt() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, long> || std::is_same_v<T, long long>) {
            return json_value.IsInt64() ? json_value.GetInt64() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            return json_value.IsUint() ? json_value.GetUint() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, unsigned long> || std::is_same_v<T, unsigned long long>) {
            return json_value.IsUint64() ? json_value.GetUint64() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, float>) {
            return json_value.IsNumber() ? json_value.GetFloat() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, double>) {
            return json_value.IsNumber() ? json_value.GetDouble() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, bool>) {
            return json_value.IsBool() ? json_value.GetBool() : DefaultValueProvider<T>::default_value();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return json_value.IsString() ? json_value.GetString() : DefaultValueProvider<T>::default_value();
        } else {
            static_assert(std::is_same_v<T, void>, "Unsupported basic type");
            return T{};
        }
    }
};

// Serialization for std::optional
template<typename T>
struct Serializer<std::optional<T>> {
    static void serialize(const std::optional<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        if (value.has_value()) {
            Serializer<T>::serialize(value.value(), json_value, allocator);
        } else {
            json_value.SetNull();
        }
    }

    static std::optional<T> deserialize(const rapidjson::Value& json_value) {
        if (json_value.IsNull()) {
            return std::nullopt;
        }
        return Serializer<T>::deserialize(json_value);
    }
};

// Serialization for STL container vector
template<typename T>
struct Serializer<std::vector<T>> : GenericContainerSerializer<std::vector, T> {};

// Serialization for STL container map
template<typename K, typename V>
struct Serializer<std::map<K, V>> : GenericMapSerializer<std::map, K, V> {};

// Serialization for unordered_map
template<typename K, typename V>
struct Serializer<std::unordered_map<K, V>> : GenericMapSerializer<std::unordered_map, K, V> {};


// Serialization for enum types
template<typename T>
    requires std::is_enum_v<T>
struct Serializer<T> {
    static void serialize(const T& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        using UnderlyingType = std::underlying_type_t<T>;
        json_value.SetInt(static_cast<UnderlyingType>(value));
    }

    static T deserialize(const rapidjson::Value& json_value) {
        using UnderlyingType = std::underlying_type_t<T>;
        if (json_value.IsInt()) {
            return static_cast<T>(json_value.GetInt());
        }
        return static_cast<T>(0);
    }
};

// Serialization for set
template<typename T>
struct Serializer<std::set<T>> : GenericContainerSerializer<std::set, T> {};

// Serialization for multiset
template<typename T>
struct Serializer<std::multiset<T>> : GenericContainerSerializer<std::multiset, T> {};

// Serialization for list
template<typename T>
struct Serializer<std::list<T>> : GenericContainerSerializer<std::list, T> {};

// Serialization for deque
template<typename T>
struct Serializer<std::deque<T>> : GenericContainerSerializer<std::deque, T> {};

// Serialization for forward_list
template<typename T>
struct Serializer<std::forward_list<T>> : GenericContainerSerializer<std::forward_list, T> {};

// Serialization for array
template<typename T, std::size_t N>
struct Serializer<std::array<T, N>> {
    static void serialize(const std::array<T, N>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        for (const auto& item : value) {
            rapidjson::Value item_value;
            Serializer<T>::serialize(item, item_value, allocator);
            json_value.PushBack(item_value, allocator);
        }
    }

    static std::array<T, N> deserialize(const rapidjson::Value& json_value) {
        std::array<T, N> result{};
        if (!json_value.IsArray()) {
            return result;
        }

        for (rapidjson::SizeType i = 0; i < json_value.Size() && i < N; i++) {
            result[i] = Serializer<T>::deserialize(json_value[i]);
        }
        return result;
    }
};

// Serialization for tuple
template<typename... Ts>
struct Serializer<std::tuple<Ts...>> {
    static void serialize(const std::tuple<Ts...>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        serialize_tuple(value, json_value, allocator, std::index_sequence_for<Ts...>{});
    }

    static std::tuple<Ts...> deserialize(const rapidjson::Value& json_value) {
        if (!json_value.IsArray() || json_value.Size() != sizeof...(Ts)) {
            return std::tuple<Ts...>{}; // Default constructed tuple
        }

        return deserialize_tuple(json_value, std::index_sequence_for<Ts...>{});
    }

private:
    template<size_t... I>
    static void serialize_tuple(const std::tuple<Ts...>& value, rapidjson::Value& json_value,
                                rapidjson::Document::AllocatorType& allocator, std::index_sequence<I...>) {
        // Expand tuple and serialize each element
        (serialize_tuple_element(std::get<I>(value), json_value, allocator), ...);
    }

    template<typename T>
    static void serialize_tuple_element(const T& value, rapidjson::Value& json_value,
                                        rapidjson::Document::AllocatorType& allocator) {
        rapidjson::Value element_value;
        Serializer<T>::serialize(value, element_value, allocator);
        json_value.PushBack(element_value, allocator);
    }

    template<size_t... I>
    static std::tuple<Ts...> deserialize_tuple(const rapidjson::Value& json_value, std::index_sequence<I...>) {
        return std::make_tuple(Serializer<Ts>::deserialize(json_value[I])...);
    }
};

// Serialization for variant
template<typename... Ts>
struct Serializer<std::variant<Ts...>> {
    static void serialize(const std::variant<Ts...>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();

        // Add type index
        rapidjson::Value type_index;
        type_index.SetInt(value.index());
        json_value.AddMember("type_index", type_index, allocator);

        // Add actual data
        rapidjson::Value data;
        std::visit([&allocator, &data](const auto& v) {
            Serializer<std::decay_t<decltype(v)>>::serialize(v, data, allocator);
        }, value);
        json_value.AddMember("data", data, allocator);
    }

    static std::variant<Ts...> deserialize(const rapidjson::Value& json_value) {
        if (!json_value.IsObject() || !json_value.HasMember("type_index") || !json_value.HasMember("data")) {
            return DefaultValueProvider<std::variant<Ts...>>::default_value();
        }

        int type_index = json_value["type_index"].GetInt();
        if (type_index < 0 || type_index >= sizeof...(Ts)) {
            return DefaultValueProvider<std::variant<Ts...>>::default_value();
        }

        return deserialize_variant_helper(type_index, json_value["data"], std::index_sequence_for<Ts...>{});
    }

private:
    template<std::size_t... I>
    static std::variant<Ts...> deserialize_variant_helper(int index, const rapidjson::Value& data, std::index_sequence<I...>) {
        using DeserializerFunc = std::variant<Ts...>(*)(const rapidjson::Value&);
        constexpr DeserializerFunc deserializers[] = {
            [](const rapidjson::Value& d) {
                return std::variant<Ts...>(std::in_place_index<I>, Serializer<std::tuple_element_t<I, std::tuple<Ts...>>>::deserialize(d));
            }...
        };

        return deserializers[index](data);
    }
};

// Serialization for smart pointers
template<typename T>
struct Serializer<std::unique_ptr<T>> : GenericSmartPtrSerializer<std::unique_ptr, T> {};

template<typename T>
struct Serializer<std::shared_ptr<T>> : GenericSmartPtrSerializer<std::shared_ptr, T> {};

// Serialization for additional container types
template<typename K, typename V>
struct Serializer<std::multimap<K, V>> : GenericMultiMapSerializer<std::multimap, K, V> {};

template<typename K, typename V>
struct Serializer<std::unordered_multimap<K, V>> : GenericMultiMapSerializer<std::unordered_multimap, K, V> {};

template<typename T>
struct Serializer<std::unordered_multiset<T>> : GenericContainerSerializer<std::unordered_multiset, T> {};

template<typename T>
struct Serializer<std::unordered_set<T>> : GenericContainerSerializer<std::unordered_set, T> {};

// Serialization for std::queue
template<typename T, typename Container>
struct Serializer<std::queue<T, Container>> : GenericQueueSerializer<std::queue, T, Container> {};

// Serialization for std::priority_queue
template<typename T, typename Container>
struct Serializer<std::priority_queue<T, Container>> : GenericQueueSerializer<std::priority_queue, T, Container> {};

// Serialization for std::stack
template<typename T, typename Container>
struct Serializer<std::stack<T, Container>> : GenericQueueSerializer<std::stack, T, Container> {};

// Serialization for std::pair
template<typename T1, typename T2>
struct Serializer<std::pair<T1, T2>> {
    static void serialize(const std::pair<T1, T2>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        
        rapidjson::Value first_value;
        Serializer<T1>::serialize(value.first, first_value, allocator);
        json_value.PushBack(first_value, allocator);
        
        rapidjson::Value second_value;
        Serializer<T2>::serialize(value.second, second_value, allocator);
        json_value.PushBack(second_value, allocator);
    }

    static std::pair<T1, T2> deserialize(const rapidjson::Value& json_value) {
        if (!json_value.IsArray() || json_value.Size() != 2) {
            return std::pair<T1, T2>{
                DefaultValueProvider<T1>::default_value(),
                DefaultValueProvider<T2>::default_value()
            };
        }
        
        T1 first = Serializer<T1>::deserialize(json_value[0]);
        T2 second = Serializer<T2>::deserialize(json_value[1]);
        
        return std::pair<T1, T2>{first, second};
    }
};

// Default serialization for basic types
template<typename T>
    requires std::is_arithmetic_v<T> || std::is_same_v<T, std::string>
struct Serializer<T> : BasicTypeSerializer<T> {};

// Type trait to check if a class has _json_field_map
template<typename T>
struct HasJsonFieldMap {
private:
    template<typename C>
    static auto test(int) -> decltype(C::_json_field_map(), std::true_type{});

    template<typename>
    static auto test(...) -> std::false_type;

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

// Type trait to check if a class has _json_base_classes
template<typename T>
struct HasJsonBaseClasses {
private:
    template<typename C>
    static auto test(int) -> decltype(std::void_t<typename C::_json_base_class_types_tuple>(), std::true_type{});

    template<typename>
    static auto test(...) -> std::false_type;

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

// Forward declarations for all serialization helper functions
template<typename Obj, typename... Args>
void _serialize_fields(const Obj& obj, rapidjson::Value& json_value,
                       rapidjson::Document::AllocatorType& allocator,
                       const std::tuple<Args...>& fields);

template<typename Obj, typename... Args>
void _deserialize_fields(Obj& obj, const rapidjson::Value& json_value,
                         const std::tuple<Args...>& fields);

template<typename T>
void serializeBaseClasses(const T& obj, rapidjson::Value& json_value,
                          rapidjson::Document::AllocatorType& allocator);

template<typename T>
void deserializeBaseClasses(T& obj, const rapidjson::Value& json_value);

// === IMPLEMENTATION OF ALL SERIALIZATION HELPERS ===

// Generic field serialization/deserialization helper to reduce code duplication
template<typename Obj, typename FieldType>
void serialize_single_field(const Obj& obj, rapidjson::Value& json_value,
                            rapidjson::Document::AllocatorType& allocator,
                            const char* name, FieldType Obj::* member_ptr) {
    if constexpr (is_optional_v<std::decay_t<FieldType>>) {
        if ((obj.*member_ptr).has_value()) {
            rapidjson::Value field_value;
            Serializer<std::decay_t<decltype((obj.*member_ptr).value())>>::serialize((obj.*member_ptr).value(), field_value, allocator);
            json_value.AddMember(rapidjson::Value(name, allocator).Move(), field_value, allocator);
        }
    } else {
        rapidjson::Value field_value;
        Serializer<std::decay_t<FieldType>>::serialize(obj.*member_ptr, field_value, allocator);
        json_value.AddMember(rapidjson::Value(name, allocator).Move(), field_value, allocator);
    }
}

template<typename Obj, typename FieldType>
void deserialize_single_field(Obj& obj, const rapidjson::Value& json_value,
                              const char* name, FieldType Obj::* member_ptr) {
    if (json_value.HasMember(name)) {
        obj.*member_ptr = Serializer<std::decay_t<FieldType>>::deserialize(json_value[name]);
    }
}

template<typename Obj, typename... Args, size_t... I>
void _serialize_fields_tuple(const Obj& obj, rapidjson::Value& json_value,
                             rapidjson::Document::AllocatorType& allocator,
                             const std::tuple<Args...>& fields,
                             std::index_sequence<I...>) {
    (serialize_single_field(obj, json_value, allocator,
                            std::get<I*2>(fields), std::get<I*2+1>(fields)), ...);
}

template<typename Obj, typename... Args>
void _serialize_fields(const Obj& obj, rapidjson::Value& json_value,
                       rapidjson::Document::AllocatorType& allocator,
                       const std::tuple<Args...>& fields) {
    _serialize_fields_tuple(obj, json_value, allocator, fields, std::make_index_sequence<sizeof...(Args) / 2>{});
}

template<typename Obj, typename... Args, size_t... I>
void _deserialize_fields_tuple(Obj& obj, const rapidjson::Value& json_value,
                               const std::tuple<Args...>& fields,
                               std::index_sequence<I...>) {
    (deserialize_single_field(obj, json_value,
                              std::get<I*2>(fields), std::get<I*2+1>(fields)), ...);
}

template<typename Obj, typename... Args>
void _deserialize_fields(Obj& obj, const rapidjson::Value& json_value,
                         const std::tuple<Args...>& fields) {
    _deserialize_fields_tuple(obj, json_value, fields, std::make_index_sequence<sizeof...(Args) / 2>{});
}

template<typename T, typename BaseClass>
void serializeBaseClass(const T& obj, rapidjson::Value& json_value,
                        rapidjson::Document::AllocatorType& allocator) {
    const auto& base_obj = static_cast<const BaseClass&>(obj);
    serializeBaseClasses(base_obj, json_value, allocator);
    if constexpr (HasJsonFieldMap<BaseClass>::value) {
        _serialize_fields(base_obj, json_value, allocator, BaseClass::_json_field_map());
    }
}

template<typename T>
void serializeBaseClasses(const T& obj, rapidjson::Value& json_value,
                          rapidjson::Document::AllocatorType& allocator) {
    if constexpr (HasJsonBaseClasses<T>::value) {
        using BaseTypes = typename T::_json_base_class_types_tuple;
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (serializeBaseClass<T, std::tuple_element_t<Is, BaseTypes>>(obj, json_value, allocator), ...);
        }(std::make_index_sequence<std::tuple_size_v<BaseTypes>>{});
    }
}

template<typename T, typename BaseClass>
void deserializeBaseClass(T& obj, const rapidjson::Value& json_value) {
    auto& base_obj = static_cast<BaseClass&>(obj);
    deserializeBaseClasses(base_obj, json_value);
    if constexpr (HasJsonFieldMap<BaseClass>::value) {
        _deserialize_fields(base_obj, json_value, BaseClass::_json_field_map());
    }
}

template<typename T>
void deserializeBaseClasses(T& obj, const rapidjson::Value& json_value) {
    if constexpr (HasJsonBaseClasses<T>::value) {
        using BaseTypes = typename T::_json_base_class_types_tuple;
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (deserializeBaseClass<T, std::tuple_element_t<Is, BaseTypes>>(obj, json_value), ...);
        }(std::make_index_sequence<std::tuple_size_v<BaseTypes>>{});
    }
}

// Serialization for classes with _json_field_map
template<typename T>
    requires HasJsonFieldMap<T>::value
struct Serializer<T> {
    static void serialize(const T& obj, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();
        serializeBaseClasses(obj, json_value, allocator);
        _serialize_fields(obj, json_value, allocator, T::_json_field_map());
    }

    static T deserialize(const rapidjson::Value& json_value) {
        T obj;
        if (!json_value.IsObject()) {
            return obj;
        }

        deserializeBaseClasses(obj, json_value);
        _deserialize_fields(obj, json_value, T::_json_field_map());

        return obj;
    }
};

// Helper function that works with buffer directly
template<typename T, typename Buffer>
std::string serialize_with_buffer(const T& obj, rapidjson::Document& document, Buffer& buffer) {
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    // Set document type based on object type
    if constexpr (HasJsonFieldMap<T>::value) {
        document.SetObject();
    }

    Serializer<T>::serialize(obj, document, allocator);

    if constexpr (std::is_same_v<Buffer, rapidjson::StringBuffer>) {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        return buffer.GetString();
    } else {
        static_assert(std::is_same_v<Buffer, rapidjson::StringBuffer>, "Unsupported buffer type");
        return "";
    }
}

// Forward declaration for serialize_with_options
template<typename T>
std::string serialize_with_options(const T& obj, const SerializeOptions& options = {});

// Utility functions for serialization
template<typename T>
std::string serialize(const T& obj) {
    return serialize_with_options(obj, {});
}

template<typename T>
T deserialize(const std::string& json_str) {
    rapidjson::Document document;
    document.Parse(json_str.c_str());

    if (document.HasParseError()) {
        return DefaultValueProvider<T>::default_value();
    }

    return Serializer<T>::deserialize(document);
}

template<typename T>
std::string serialize_pretty(const T& obj) {
    SerializeOptions options;
    options.pretty_print = true;
    return serialize_with_options(obj, options);
}

// Generic deserialization helper to reduce code duplication
template<typename T>
std::pair<T, SerializeError> deserialize_with_error_handling(const std::string& json_str) {
    SerializeError error;
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    return {Serializer<T>::deserialize(doc), error};
}

// Enhanced error handling - add type checking functionality
template<typename T>
std::pair<T, SerializeError> deserialize_with_type_check(const std::string& json_str) {
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        SerializeError error;
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    // Type checking
    if constexpr (std::is_same_v<T, std::string>) {
        if (!doc.IsString()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected string value";
            return {T{}, error};
        }
    } else if constexpr (std::is_arithmetic_v<T>) {
        if (!doc.IsNumber()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected numeric value";
            return {T{}, error};
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!doc.IsBool()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected boolean value";
            return {T{}, error};
        }
    } else if constexpr (HasJsonFieldMap<T>::value) {
        if (!doc.IsObject()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected JSON object";
            return {T{}, error};
        }
    }

    return {Serializer<T>::deserialize(doc), SerializeError{}};
}

// Add forward declarations - should be placed before deserialize_with_defaults function
template<typename T, typename... Args>
void deserialize_with_defaults_impl(T& obj, const rapidjson::Value& json_value, const std::tuple<Args...>& fields);

template<typename T, typename... Args, size_t... I>
void deserialize_with_defaults_tuple(T& obj, const rapidjson::Value& json_value,
                                     const std::tuple<Args...>& fields,
                                     std::index_sequence<I...>);

template<typename T>
void deserialize_field_with_defaults(T& obj, const rapidjson::Value& json_value,
                                     const char* field_name, auto T::* field);

// Add implementations of these functions after deserialize_with_defaults function
template<typename T, typename... Args>
void deserialize_with_defaults_impl(T& obj, const rapidjson::Value& json_value, const std::tuple<Args...>& fields) {
    deserialize_with_defaults_tuple(obj, json_value, fields, std::make_index_sequence<sizeof...(Args) / 2>{});
}

template<typename T, typename... Args, size_t... I>
void deserialize_with_defaults_tuple(T& obj, const rapidjson::Value& json_value,
                                     const std::tuple<Args...>& fields,
                                     std::index_sequence<I...>) {
    (deserialize_field_with_defaults(obj, json_value, std::get<I*2>(fields), std::get<I*2+1>(fields)), ...);
}

template<typename T>
void deserialize_field_with_defaults(T& obj, const rapidjson::Value& json_value,
                                     const char* field_name, auto T::* field) {
    if (json_value.HasMember(field_name)) {
        obj.*field = Serializer<std::decay_t<decltype(obj.*field)>>::deserialize(json_value[field_name]);
    }
}

// Deserialization with defaults
template<typename T>
T deserialize_with_defaults(const std::string& json_str, const T& defaults = DefaultValueProvider<T>::default_value()) {
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        return defaults;
    }

    T result = defaults;
    if constexpr (HasJsonFieldMap<T>::value) {
        if (!doc.IsObject()) {
            return result;
        }
        deserialize_with_defaults_impl(result, doc, T::_json_field_map());
    } else {
        result = Serializer<T>::deserialize(doc);
    }

    return result;
}

// Serialization performance optimization - string pre-allocation
template<typename T>
std::string serialize_optimized(const T& obj, std::size_t reserved_size = 1024) {
    SerializeOptions options;
    options.buffer_reserve_size = reserved_size;
    return serialize_with_options(obj, options);
}

// Add deserialize_strict function
template<typename T>
std::pair<T, SerializeError> deserialize_strict(const std::string& json_str,
                                                const std::vector<std::string>& required_fields = {},
                                                const SerializeOptions::ValidationFunction& custom_validator = nullptr) {
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        SerializeError error;
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    if constexpr (HasJsonFieldMap<T>::value) {
        if (!doc.IsObject()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Type mismatch: expected JSON object but got " +
                            std::string(doc.IsArray() ? "array" :
                                            (doc.IsString() ? "string" :
                                                 (doc.IsNumber() ? "number" :
                                                      (doc.IsBool() ? "boolean" : "other type"))));
            return {T{}, error};
        }

        // Check required fields
        for (const auto& field : required_fields) {
            if (!doc.HasMember(field.c_str())) {
                SerializeError error;
                error.code = SerializeError::ErrorCode::MISSING_FIELD;
                error.message = "Missing required field: " + field;
                error.path = "$." + field;
                return {T{}, error};
            }
        }

        // Apply custom validation if provided
        if (custom_validator) {
            SerializeError error = custom_validator(doc);
            if (error.has_error()) {
                return {T{}, error};
            }
        }
    }

    return {Serializer<T>::deserialize(doc), SerializeError{}};
}

// Enhanced serialization with options
// Helper function to serialize with recursion depth tracking

template<typename T>
std::string serialize_with_options(const T& obj, const SerializeOptions& options) {
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    // Set document type based on object type
    if constexpr (HasJsonFieldMap<T>::value) {
        document.SetObject();
    }

    Serializer<T>::serialize(obj, document, allocator);

    // Apply field filtering if specified
    if (!options.included_fields.empty() || !options.excluded_fields.empty()) {
        rapidjson::Value filtered_doc;
        filtered_doc.SetObject();

        for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it) {
            std::string field_name = it->name.GetString();

            // Check inclusion/exclusion lists
            bool should_include = true;
            if (!options.included_fields.empty()) {
                should_include = std::find(options.included_fields.begin(),
                                           options.included_fields.end(), field_name) != options.included_fields.end();
            }
            if (should_include && !options.excluded_fields.empty()) {
                should_include = std::find(options.excluded_fields.begin(),
                                           options.excluded_fields.end(), field_name) == options.excluded_fields.end();
            }

            if (should_include) {
                filtered_doc.AddMember(it->name, it->value, allocator);
            }
        }
        // Create a new document instead of clearing the existing one
        rapidjson::Document doc;
        doc.SetObject();
        for (auto it = filtered_doc.MemberBegin(); it != filtered_doc.MemberEnd(); ++it) {
            doc.AddMember(it->name, it->value, allocator);
        }
        document = std::move(doc);
    }

    rapidjson::StringBuffer buffer;
    buffer.Reserve(options.buffer_reserve_size);

    if (options.pretty_print) {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
    } else {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
    }

    return buffer.GetString();
}

// Enhanced deserialization with options and validation
template<typename T>
std::pair<T, SerializeError> deserialize_with_options(const std::string& json_str,
                                                      const SerializeOptions& options = {}) {
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        SerializeError error;
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    // Apply custom validation if provided
    if (options.custom_validator) {
        SerializeError error = options.custom_validator(doc);
        if (error.has_error()) {
            return {T{}, error};
        }
    }

    // Apply field validations
    if constexpr (HasJsonFieldMap<T>::value) {
        for (const auto& validation : options.field_validations) {
            if (doc.HasMember(validation.field_name.c_str())) {
                if (!validation.validator(doc[validation.field_name.c_str()])) {
                    SerializeError error;
                    error.code = SerializeError::ErrorCode::VALIDATION_ERROR;
                    error.message = validation.error_message.empty() ?
                                        "Validation failed for field: " + validation.field_name :
                                        validation.error_message;
                    error.path = "$." + validation.field_name;
                    return {T{}, error};
                }
            }
        }
    }

    return {Serializer<T>::deserialize(doc), SerializeError{}};
}

// File I/O support
template<typename T>
bool serialize_to_file(const T& obj, const std::string& filename,
                       const SerializeOptions& options = {}) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << serialize_with_options(obj, options);
        return true;
    } catch (...) {
        return false;
    }
}

template<typename T>
std::pair<T, SerializeError> deserialize_from_file(const std::string& filename,
                                                   const SerializeOptions& options = {}) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            SerializeError error;
            error.code = SerializeError::ErrorCode::CUSTOM_ERROR;
            error.message = "Cannot open file: " + filename;
            return {T{}, error};
        }

        std::string json_str((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

        return deserialize_with_options<T>(json_str, options);
    } catch (const std::exception& e) {
        SerializeError error;
        error.code = SerializeError::ErrorCode::CUSTOM_ERROR;
        error.message = "File read error: " + std::string(e.what());
        return {T{}, error};
    }
}

// Stream-based serialization for large objects
template<typename T>
class StreamingSerializer {
private:
    std::ostream& output_;
    SerializeOptions options_;
    bool first_element_ = true;

public:
    inline StreamingSerializer(std::ostream& output, const SerializeOptions& options = {})
        : output_(output), options_(options) {}

    inline void begin_array() {
        output_ << "[";
        first_element_ = true;
    }

    inline void end_array() {
        output_ << "]";
    }

    inline void serialize_element(const T& obj) {
        if (!first_element_) {
            output_ << ",";
        }
        if (options_.pretty_print) {
            output_ << "\n  ";
        }
        output_ << serialize_with_options(obj, options_);
        first_element_ = false;
    }
};

// Generic validation helper to reduce code duplication
namespace validation {
// Generic type checking helper
template<typename T>
bool check_type(const rapidjson::Value& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value.IsString();
    } else if constexpr (std::is_arithmetic_v<T>) {
        return value.IsNumber();
    } else if constexpr (std::is_same_v<T, bool>) {
        return value.IsBool();
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        return value.IsNull();
    }
    return true; // Default case
}

// Range validation for numeric types
template<typename T>
auto in_range(T min, T max) {
    return [min, max](const rapidjson::Value& value) -> bool {
        if (!check_type<T>(value)) return false;
        T val = static_cast<T>(value.GetDouble());
        return val >= min && val <= max;
    };
}

// String length validation
inline auto string_length(size_t min_len, size_t max_len) {
    return [min_len, max_len](const rapidjson::Value& value) -> bool {
        if (!check_type<std::string>(value)) return false;
        size_t len = value.GetStringLength();
        return len >= min_len && len <= max_len;
    };
}

// Pattern validation for strings
inline auto matches_pattern(const std::string& pattern) {
    return [pattern](const rapidjson::Value& value) -> bool {
        if (!check_type<std::string>(value)) return false;
        // Simple pattern matching - can be enhanced with regex
        std::string str = value.GetString();
        return str.find(pattern) != std::string::npos;
    };
}

// Array size validation
inline auto array_size(size_t min_size, size_t max_size) {
    return [min_size, max_size](const rapidjson::Value& value) -> bool {
        if (!value.IsArray()) return false;
        size_t size = value.Size();
        return size >= min_size && size <= max_size;
    };
}

// Required fields validation
inline auto has_required_fields(const std::vector<std::string>& required_fields) {
    return [required_fields](const rapidjson::Value& value) -> bool {
        if (!value.IsObject()) return false;
        for (const auto& field : required_fields) {
            if (!value.HasMember(field.c_str())) {
                return false;
            }
        }
        return true;
    };
}
}

// Performance monitoring
class PerformanceMonitor {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string operation_name_;

public:
    inline PerformanceMonitor(const std::string& name) : operation_name_(name) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    inline ~PerformanceMonitor() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        // Performance monitoring is available but not active by default
        // Uncomment the following line to enable performance logging:
        // std::cout << operation_name_ << " took " << duration.count() << " microseconds" << std::endl;
    }
};

// Generic error formatting helper to reduce code duplication
inline std::string get_error_type_string(SerializeError::ErrorCode code) {
    switch (code) {
    case SerializeError::ErrorCode::PARSE_ERROR:
        return "Parse error";
    case SerializeError::ErrorCode::TYPE_MISMATCH:
        return "Type mismatch";
    case SerializeError::ErrorCode::MISSING_FIELD:
        return "Missing field";
    case SerializeError::ErrorCode::VALIDATION_ERROR:
        return "Validation error";
    case SerializeError::ErrorCode::CUSTOM_ERROR:
        return "Custom error";
    case SerializeError::ErrorCode::RECURSION_DEPTH_EXCEEDED:
        return "Recursion depth exceeded";
    case SerializeError::ErrorCode::MEMORY_ERROR:
        return "Memory error";
    default:
        return "Unknown error";
    }
}

inline std::string format_error(const SerializeError& error) {
    if (!error.has_error()) {
        return "No error";
    }

    std::string result = "Error: " + get_error_type_string(error.code);

    if (!error.message.empty()) {
        result += " - " + error.message;
    }

    if (!error.path.empty()) {
        result += " (at " + error.path + ")";
    }

    return result;
}

// Keep the original JSON_SERIALIZABLE macro for compatibility with existing code
#define JSON_SERIALIZABLE(Type, ...) \
template<> \
    struct json_serializer::Serializer<Type> { \
        static void serialize(const Type& obj, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) { \
            json_value.SetObject(); \
            serializeFields(obj, json_value, allocator, __VA_ARGS__); \
    } \
    \
        static Type deserialize(const rapidjson::Value& json_value) { \
            Type obj; \
            if (!json_value.IsObject()) { \
                return obj; \
        } \
            deserializeFields(obj, json_value, __VA_ARGS__); \
            return obj; \
    } \
    \
    private: \
    template<typename Obj, typename... Fields> \
    static void serializeFields(const Obj& obj, rapidjson::Value& json_value, \
                    rapidjson::Document::AllocatorType& allocator, \
                    const char* field_name, auto Obj::* field, Fields... fields) { \
            if constexpr (json_serializer::is_optional_v<std::decay_t<decltype(obj.*field)>>) { \
                if ((obj.*field).has_value()) { \
                    rapidjson::Value field_value; \
                    Serializer<std::decay_t<decltype((obj.*field).value())>>::serialize((obj.*field).value(), field_value, allocator); \
                    json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator); \
            } \
        } else { \
                rapidjson::Value field_value; \
                Serializer<std::decay_t<decltype(obj.*field)>>::serialize(obj.*field, field_value, allocator); \
                json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator); \
        } \
        \
            if constexpr (sizeof...(fields) > 0) { \
                serializeFields(obj, json_value, allocator, fields...); \
        } \
    } \
    \
        template<typename Obj, typename... Fields> \
        static void deserializeFields(Obj& obj, const rapidjson::Value& json_value, \
                          const char* field_name, auto Obj::* field, Fields... fields) { \
            if (json_value.HasMember(field_name)) { \
                obj.*field = Serializer<std::decay_t<decltype(obj.*field)>>::deserialize(json_value[field_name]); \
        } \
        \
            if constexpr (sizeof...(fields) > 0) { \
                deserializeFields(obj, json_value, fields...); \
        } \
    } \
};

// Internal field registration mechanism - used to declare JSON field mapping in the public section of structs
#define DECLARE_JSON_FIELDS(...) \
static constexpr auto _json_field_map() { \
        return std::make_tuple(__VA_ARGS__); \
} \
    \
    template<typename T> \
    friend struct json_serializer::Serializer;

// Macro to declare base classes for JSON serialization
#define INHERIT_JSON_FIELDS(...) \
using _json_base_class_types_tuple = std::tuple<__VA_ARGS__>; \
    template<typename FriendT> \
    friend struct json_serializer::Serializer;

// Macro to inherit JSON fields from multiple base classes
#define INHERIT_JSON_FIELDS_MULTIPLE(...) \
INHERIT_JSON_FIELDS(__VA_ARGS__)

// Macro for performance monitoring
#define JSON_PERFORMANCE_MONITOR(name) \
json_serializer::PerformanceMonitor monitor(name)

}
