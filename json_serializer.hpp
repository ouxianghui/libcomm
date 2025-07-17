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
#include <type_traits>
#include <tuple>
#include <utility>
#include <set>
#include <list>
#include <array>
#include <variant>
#include <functional>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/error/en.h"

namespace json_serializer {

// Error handling structure - defined early for use in function signatures
struct SerializeError {
    enum class ErrorCode {
        NONE,
        PARSE_ERROR,
        MISSING_FIELD,
        TYPE_MISMATCH,
        VALIDATION_ERROR,
        CUSTOM_ERROR
    };

    ErrorCode code = ErrorCode::NONE;
    std::string message;
    std::string path;

    bool has_error() const {
        return code != ErrorCode::NONE;
    }

    // Add method to build nested paths
    static std::string build_path(const std::string& parent, const std::string& child) {
        if (parent.empty()) {
            return child;
        }
        return parent + "." + child;
    }

    // Add method to append to path
    void append_path(const std::string& part) {
        if (path.empty()) {
            path = part;
        } else {
            path += "." + part;
        }
    }
};

// Serialization options structure - defined early for use in function signatures
struct SerializeOptions {
    bool pretty_print = false;
    bool include_null_fields = false;
    std::vector<std::string> included_fields;  // If non-empty, only include these fields
    std::vector<std::string> excluded_fields;  // If non-empty, exclude these fields
    int max_recursion_depth = 32;              // Maximum recursion depth

    using ValidationFunction = std::function<SerializeError(const rapidjson::Value&)>;
    ValidationFunction custom_validator;
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

// Add missing DefaultValueProvider
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

// Specialization for optional
template<typename T>
struct DefaultValueProvider<std::optional<T>> {
    static std::optional<T> default_value() { return std::nullopt; }
};

// Specialization for containers
template<typename T>
struct DefaultValueProvider<std::vector<T>> {
    static std::vector<T> default_value() { return {}; }
};

template<typename K, typename V>
struct DefaultValueProvider<std::map<K, V>> {
    static std::map<K, V> default_value() { return {}; }
};

// Add specialization for unordered_map
template<typename K, typename V>
struct DefaultValueProvider<std::unordered_map<K, V>> {
    static std::unordered_map<K, V> default_value() { return {}; }
};

// Add specialization for set
template<typename T>
struct DefaultValueProvider<std::set<T>> {
    static std::set<T> default_value() { return {}; }
};

// Add specialization for list
template<typename T>
struct DefaultValueProvider<std::list<T>> {
    static std::list<T> default_value() { return {}; }
};

// Add specialization for array
template<typename T, std::size_t N>
struct DefaultValueProvider<std::array<T, N>> {
    static std::array<T, N> default_value() {
        std::array<T, N> result{};
        return result;
    }
};

// Add specialization for tuple
template<typename... Ts>
struct DefaultValueProvider<std::tuple<Ts...>> {
    static std::tuple<Ts...> default_value() { return std::tuple<Ts...>{}; }
};

// Add specialization for variant
template<typename... Ts>
struct DefaultValueProvider<std::variant<Ts...>> {
    static std::variant<Ts...> default_value() {
        // Default initialize to the first type's default value
        return std::variant<Ts...>{std::in_place_index<0>, DefaultValueProvider<std::tuple_element_t<0, std::tuple<Ts...>>>::default_value()};
    }
};

// Add specialization for pair
template<typename T1, typename T2>
struct DefaultValueProvider<std::pair<T1, T2>> {
    static std::pair<T1, T2> default_value() {
        return std::pair<T1, T2>{
            DefaultValueProvider<T1>::default_value(),
            DefaultValueProvider<T2>::default_value()
        };
    }
};

// Add specialization for enum types
template<typename T>
    requires std::is_enum_v<T>
struct DefaultValueProvider<T> {
    static T default_value() { return static_cast<T>(0); }
};

// Forward declaration
template<typename T>
struct Serializer;

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
struct Serializer<std::vector<T>> {
    static void serialize(const std::vector<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        json_value.Reserve(value.size(), allocator);

        for (const auto& item : value) {
            rapidjson::Value item_value;
            Serializer<T>::serialize(item, item_value, allocator);
            json_value.PushBack(item_value, allocator);
        }
    }

    static std::vector<T> deserialize(const rapidjson::Value& json_value) {
        std::vector<T> result;
        if (!json_value.IsArray()) {
            return result;
        }

        result.reserve(json_value.Size());
        for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
            result.push_back(Serializer<T>::deserialize(json_value[i]));
        }
        return result;
    }
};

// Serialization for STL container map
template<typename K, typename V>
struct Serializer<std::map<K, V>> {
    static void serialize(const std::map<K, V>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();
        for (const auto& [key, item] : value) {
            rapidjson::Value item_value;
            Serializer<V>::serialize(item, item_value, allocator);

            if constexpr (std::is_same_v<K, std::string>) {
                json_value.AddMember(rapidjson::Value(key.c_str(), allocator).Move(), item_value, allocator);
            } else {
                // Non-string keys need to be converted to strings
                std::string key_str = std::to_string(key);
                json_value.AddMember(rapidjson::Value(key_str.c_str(), allocator).Move(), item_value, allocator);
            }
        }
    }

    static std::map<K, V> deserialize(const rapidjson::Value& json_value) {
        std::map<K, V> result;
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

// Serialization for unordered_map
template<typename K, typename V>
struct Serializer<std::unordered_map<K, V>> {
    static void serialize(const std::unordered_map<K, V>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
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

    static std::unordered_map<K, V> deserialize(const rapidjson::Value& json_value) {
        std::unordered_map<K, V> result;
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
struct Serializer<std::set<T>> {
    static void serialize(const std::set<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        for (const auto& item : value) {
            rapidjson::Value item_value;
            Serializer<T>::serialize(item, item_value, allocator);
            json_value.PushBack(item_value, allocator);
        }
    }

    static std::set<T> deserialize(const rapidjson::Value& json_value) {
        std::set<T> result;
        if (!json_value.IsArray()) {
            return result;
        }

        for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
            result.insert(Serializer<T>::deserialize(json_value[i]));
        }
        return result;
    }
};

// Serialization for list
template<typename T>
struct Serializer<std::list<T>> {
    static void serialize(const std::list<T>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        for (const auto& item : value) {
            rapidjson::Value item_value;
            Serializer<T>::serialize(item, item_value, allocator);
            json_value.PushBack(item_value, allocator);
        }
    }

    static std::list<T> deserialize(const rapidjson::Value& json_value) {
        std::list<T> result;
        if (!json_value.IsArray()) {
            return result;
        }

        for (rapidjson::SizeType i = 0; i < json_value.Size(); i++) {
            result.push_back(Serializer<T>::deserialize(json_value[i]));
        }
        return result;
    }
};

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

template<typename Obj, typename... Args, size_t... I>
void _serialize_fields_tuple(const Obj& obj, rapidjson::Value& json_value,
                          rapidjson::Document::AllocatorType& allocator,
                          const std::tuple<Args...>& fields,
                          std::index_sequence<I...>) {
    auto serialize_one_field = [&](const char* name, auto member_ptr) {
        if constexpr (is_optional_v<std::decay_t<decltype(obj.*member_ptr)>>) {
            if ((obj.*member_ptr).has_value()) {
                 rapidjson::Value field_value;
                 Serializer<std::decay_t<decltype((obj.*member_ptr).value())>>::serialize((obj.*member_ptr).value(), field_value, allocator);
                 json_value.AddMember(rapidjson::Value(name, allocator).Move(), field_value, allocator);
            }
        } else {
            rapidjson::Value field_value;
            Serializer<std::decay_t<decltype(obj.*member_ptr)>>::serialize(obj.*member_ptr, field_value, allocator);
            json_value.AddMember(rapidjson::Value(name, allocator).Move(), field_value, allocator);
        }
    };
    (serialize_one_field(std::get<I*2>(fields), std::get<I*2+1>(fields)), ...);
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
    auto deserialize_one_field = [&](const char* name, auto member_ptr) {
        if (json_value.HasMember(name)) {
            obj.*member_ptr = Serializer<std::decay_t<decltype(obj.*member_ptr)>>::deserialize(json_value[name]);
        }
    };
    (deserialize_one_field(std::get<I*2>(fields), std::get<I*2+1>(fields)), ...);
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

// Utility functions for serialization
template<typename T>
std::string serialize(const T& obj) {
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    Serializer<T>::serialize(obj, document, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    return buffer.GetString();
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
    rapidjson::Document document;
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    Serializer<T>::serialize(obj, document, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    return buffer.GetString();
}

// Enhanced error handling - add type checking functionality
template<typename T>
std::pair<T, SerializeError> deserialize_with_type_check(const std::string& json_str) {
    SerializeError error;
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    if constexpr (std::is_same_v<T, std::string>) {
        if (!doc.IsString()) {
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected string value";
            return {T{}, error};
        }
    } else if constexpr (std::is_arithmetic_v<T>) {
        if (!doc.IsNumber()) {
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected numeric value";
            return {T{}, error};
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!doc.IsBool()) {
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected boolean value";
            return {T{}, error};
        }
    } else if constexpr (HasJsonFieldMap<T>::value) {
        if (!doc.IsObject()) {
            error.code = SerializeError::ErrorCode::TYPE_MISMATCH;
            error.message = "Expected JSON object";
            return {T{}, error};
        }
    }

    return {Serializer<T>::deserialize(doc), error};
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
    rapidjson::Document doc;
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    // Correctly handle non-object types at the root
    if constexpr (HasJsonFieldMap<T>::value) {
         doc.SetObject();
    }

    Serializer<T>::serialize(obj, doc, allocator);

    rapidjson::StringBuffer buffer;
    buffer.Reserve(reserved_size);
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}


// Add deserialize_strict function
template<typename T>
std::pair<T, SerializeError> deserialize_strict(const std::string& json_str,
                                                const std::vector<std::string>& required_fields = {}) {
    SerializeError error;
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        error.code = SerializeError::ErrorCode::PARSE_ERROR;
        error.message = "JSON parse error at offset " + std::to_string(doc.GetErrorOffset()) +
                        ": " + rapidjson::GetParseError_En(doc.GetParseError());
        return {T{}, error};
    }

    if constexpr (HasJsonFieldMap<T>::value) {
        if (!doc.IsObject()) {
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
                error.code = SerializeError::ErrorCode::MISSING_FIELD;
                error.message = "Missing required field: " + field;
                error.path = "$." + field;
                return {T{}, error};
            }
        }
    }

    return {Serializer<T>::deserialize(doc), error};
}

// Internal field registration mechanism - used to declare JSON field mapping in the public section of structs
#define DECLARE_JSON_FIELDS(...) \
static constexpr auto _json_field_map() { \
        return std::make_tuple(__VA_ARGS__); \
} \
    \
    template<typename T> \
    friend struct json_serializer::Serializer;

// Macro to declare base classes for JSON serialization
#define DECLARE_JSON_BASE_CLASSES(...) \
using _json_base_class_types_tuple = std::tuple<__VA_ARGS__>; \
template<typename FriendT> \
friend struct json_serializer::Serializer;

// Macro to inherit JSON fields from base classes
#define INHERIT_JSON_FIELDS(BaseClass) \
DECLARE_JSON_BASE_CLASSES(BaseClass)

// Macro to inherit JSON fields from multiple base classes
#define INHERIT_JSON_FIELDS_MULTIPLE(...) \
DECLARE_JSON_BASE_CLASSES(__VA_ARGS__)

// Enhanced macro for classes with inheritance (DEPRECATED - use DECLARE_JSON_BASE_CLASSES)
#define JSON_SERIALIZABLE_INHERITED(Type, BaseClasses, ...) \
template<> \
    struct json_serializer::Serializer<Type> { \
        static void serialize(const Type& obj, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) { \
            json_value.SetObject(); \
            json_serializer::serializeBaseClasses(obj, json_value, allocator); \
            serializeFields(obj, json_value, allocator, __VA_ARGS__); \
    } \
    \
        static Type deserialize(const rapidjson::Value& json_value) { \
            Type obj; \
            if (!json_value.IsObject()) { \
                return obj; \
        } \
            json_serializer::deserializeBaseClasses(obj, json_value); \
            deserializeFields(obj, json_value, __VA_ARGS__); \
            return obj; \
    } \
    \
    private: \
    template<typename Obj, typename... Fields> \
    static void serializeFields(const Obj& obj, rapidjson::Value& json_value, \
                    rapidjson::Document::AllocatorType& allocator, \
                    const char* field_name, auto Obj::* field, Fields... fields) { \
            rapidjson::Value field_value; \
            Serializer<std::decay_t<decltype(obj.*field)>>::serialize(obj.*field, field_value, allocator); \
            json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator); \
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

inline std::string format_error(const SerializeError& error) {
    if (!error.has_error()) {
        return "No error";
    }

    std::string result = "Error: ";
    switch (error.code) {
    case SerializeError::ErrorCode::PARSE_ERROR:
        result += "Parse error";
        break;
    case SerializeError::ErrorCode::TYPE_MISMATCH:
        result += "Type mismatch";
        break;
    case SerializeError::ErrorCode::MISSING_FIELD:
        result += "Missing field";
        break;
    case SerializeError::ErrorCode::VALIDATION_ERROR:
        result += "Validation error";
        break;
    case SerializeError::ErrorCode::CUSTOM_ERROR:
        result += "Custom error";
        break;
    default:
        result += "Unknown error";
    }

    if (!error.message.empty()) {
        result += " - " + error.message;
    }

    if (!error.path.empty()) {
        result += " (at " + error.path + ")";
    }

    return result;
}

}
