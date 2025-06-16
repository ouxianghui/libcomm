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

// 辅助类型特性 is_optional
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

// 添加缺失的 DefaultValueProvider
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

// 为 unordered_map 添加特化
template<typename K, typename V>
struct DefaultValueProvider<std::unordered_map<K, V>> {
    static std::unordered_map<K, V> default_value() { return {}; }
};

// 为 set 添加特化
template<typename T>
struct DefaultValueProvider<std::set<T>> {
    static std::set<T> default_value() { return {}; }
};

// 为 list 添加特化
template<typename T>
struct DefaultValueProvider<std::list<T>> {
    static std::list<T> default_value() { return {}; }
};

// 为 array 添加特化
template<typename T, std::size_t N>
struct DefaultValueProvider<std::array<T, N>> {
    static std::array<T, N> default_value() { 
        std::array<T, N> result{};
        return result;
    }
};

// 为 tuple 添加特化
template<typename... Ts>
struct DefaultValueProvider<std::tuple<Ts...>> {
    static std::tuple<Ts...> default_value() { return std::tuple<Ts...>{}; }
};

// 为 variant 添加特化
template<typename... Ts>
struct DefaultValueProvider<std::variant<Ts...>> {
    static std::variant<Ts...> default_value() { 
        // 默认初始化为第一个类型的默认值
        return std::variant<Ts...>{std::in_place_index<0>, DefaultValueProvider<std::tuple_element_t<0, std::tuple<Ts...>>>::default_value()};
    }
};

// 为 pair 添加特化
template<typename T1, typename T2>
struct DefaultValueProvider<std::pair<T1, T2>> {
    static std::pair<T1, T2> default_value() {
        return std::pair<T1, T2>{
            DefaultValueProvider<T1>::default_value(),
            DefaultValueProvider<T2>::default_value()
        };
    }
};

// 为枚举类型添加特化
template<typename T>
requires std::is_enum_v<T>
struct DefaultValueProvider<T> {
    static T default_value() { return static_cast<T>(0); }
};

// 前置声明
template<typename T>
struct Serializer;

// 基本类型的序列化/反序列化
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

// std::optional 的序列化/反序列化
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

// STL容器 vector 的序列化/反序列化
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

// STL容器 map 的序列化/反序列化
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
                // 非字符串键需要转换为字符串
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
                result[static_cast<K>(std::stoll(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
            } else if constexpr (std::is_floating_point_v<K>) {
                result[static_cast<K>(std::stod(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
            }
        }
        return result;
    }
};

// 同样实现unordered_map
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
                result[static_cast<K>(std::stoll(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
            } else if constexpr (std::is_floating_point_v<K>) {
                result[static_cast<K>(std::stod(it->name.GetString()))] = Serializer<V>::deserialize(it->value);
            }
        }
        return result;
    }
};


// 添加对枚举类型的支持
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

// 添加对std::set的支持
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

// 添加对std::list的支持
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

// 添加对std::array的支持
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

// 添加对 std::tuple 的支持
template<typename... Ts>
struct Serializer<std::tuple<Ts...>> {
    static void serialize(const std::tuple<Ts...>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetArray();
        serialize_tuple(value, json_value, allocator, std::index_sequence_for<Ts...>{});
    }

    static std::tuple<Ts...> deserialize(const rapidjson::Value& json_value) {
        if (!json_value.IsArray() || json_value.Size() != sizeof...(Ts)) {
            return std::tuple<Ts...>{}; // 默认构造的元组
        }

        return deserialize_tuple(json_value, std::index_sequence_for<Ts...>{});
    }

private:
    template<size_t... I>
    static void serialize_tuple(const std::tuple<Ts...>& value, rapidjson::Value& json_value,
                                rapidjson::Document::AllocatorType& allocator, std::index_sequence<I...>) {
        // 展开元组并序列化每个元素
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

// 添加对C++17 std::variant的支持
template<typename... Ts>
struct Serializer<std::variant<Ts...>> {
    static void serialize(const std::variant<Ts...>& value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();

        // 添加类型索引
        rapidjson::Value type_index;
        type_index.SetInt(value.index());
        json_value.AddMember("type_index", type_index, allocator);

        // 添加实际数据
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


// 基本类型的序列化器
template<typename T>
requires std::is_arithmetic_v<T> || std::is_same_v<T, std::string>
struct Serializer<T> : BasicTypeSerializer<T> {};

// 序列化器特化 - 用于处理具有内部字段映射的类型
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

// 通用序列化器 - 针对具有内部字段映射的类型
template<typename T>
requires HasJsonFieldMap<T>::value
struct Serializer<T> {
    static void serialize(const T& obj, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        json_value.SetObject();
        serializeFields(obj, json_value, allocator, T::_json_field_map());
    }

    static T deserialize(const rapidjson::Value& json_value) {
        T obj;
        if (!json_value.IsObject()) {
            return obj;
        }
        deserializeFields(obj, json_value, T::_json_field_map());
        return obj;
    }

private:
    template<typename Obj, typename... Args>
    static void serializeFields(const Obj& obj, rapidjson::Value& json_value,
                               rapidjson::Document::AllocatorType& allocator,
                               const std::tuple<Args...>& fields) {
        serializeFieldsTuple(obj, json_value, allocator, fields, std::make_index_sequence<sizeof...(Args) / 2>{});
    }

    template<typename Obj, typename... Args, size_t... I>
    static void serializeFieldsTuple(const Obj& obj, rapidjson::Value& json_value,
                                    rapidjson::Document::AllocatorType& allocator,
                                    const std::tuple<Args...>& fields,
                                    std::index_sequence<I...>) {
        // 展开参数包进行处理 - 每次处理两个元素（字段名和字段指针）
        (serializeField(obj, json_value, allocator,
                      std::get<I*2>(fields),
                      std::get<I*2+1>(fields)), ...);
    }

    template<typename Obj>
    static void serializeField(const Obj& obj, rapidjson::Value& json_value,
                              rapidjson::Document::AllocatorType& allocator,
                              const char* field_name, auto Obj::* field) {
        rapidjson::Value field_value;
        Serializer<std::decay_t<decltype(obj.*field)>>::serialize(obj.*field, field_value, allocator);
        json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator);
    }

    template<typename Obj, typename... Args>
    static void deserializeFields(Obj& obj, const rapidjson::Value& json_value,
                                 const std::tuple<Args...>& fields) {
        deserializeFieldsTuple(obj, json_value, fields, std::make_index_sequence<sizeof...(Args) / 2>{});
    }

    template<typename Obj, typename... Args, size_t... I>
    static void deserializeFieldsTuple(Obj& obj, const rapidjson::Value& json_value,
                                      const std::tuple<Args...>& fields,
                                      std::index_sequence<I...>) {
        // 展开参数包进行处理 - 每次处理两个元素（字段名和字段指针）
        (deserializeField(obj, json_value,
                        std::get<I*2>(fields),
                        std::get<I*2+1>(fields)), ...);
    }

    template<typename Obj>
    static void deserializeField(Obj& obj, const rapidjson::Value& json_value,
                                const char* field_name, auto Obj::* field) {
        if (json_value.HasMember(field_name)) {
            obj.*field = Serializer<std::decay_t<decltype(obj.*field)>>::deserialize(json_value[field_name]);
        }
    }
};

// 为了兼容现有代码，保留原来的 JSON_SERIALIZABLE 宏
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

// 基本的序列化/反序列化函数
template<typename T>
std::string serialize(const T& obj) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    rapidjson::Value json_value;
    Serializer<T>::serialize(obj, json_value, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_value.Accept(writer);

    return buffer.GetString();
}

template<typename T>
T deserialize(const std::string& json_str) {
    rapidjson::Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        return T{};
    }

    return Serializer<T>::deserialize(doc);
}

// 格式化JSON输出的辅助函数
template<typename T>
std::string serialize_pretty(const T& obj) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    rapidjson::Value json_value;
    Serializer<T>::serialize(obj, json_value, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    json_value.Accept(writer);

    return buffer.GetString();
}


// 改进 SerializeError 结构体
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
    
    // 添加方法来构建嵌套路径
    static std::string build_path(const std::string& parent, const std::string& child) {
        if (parent.empty()) {
            return child;
        }
        return parent + "." + child;
    }
    
    // 添加方法来追加到路径
    void append_path(const std::string& part) {
        if (path.empty()) {
            path = part;
        } else {
            path += "." + part;
        }
    }
};

// 增强错误处理 - 添加类型检查功能
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

    // Improved type checking for different container types
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
    
    // Rest of the implementation...
    return {Serializer<T>::deserialize(doc), error};
}

// 确保 SerializeOptions 结构体定义完整
struct SerializeOptions {
    bool pretty_print = false;
    bool include_null_fields = false;
    std::vector<std::string> included_fields;  // 如果非空，只包含这些字段
    std::vector<std::string> excluded_fields;  // 如果非空，排除这些字段
    int max_recursion_depth = 32;              // 最大递归深度
    
    using ValidationFunction = std::function<SerializeError(const rapidjson::Value&)>;
    ValidationFunction custom_validator;
};

// 添加一个嵌套深度跟踪并支持序列化选项的新函数
template<typename T>
std::string serialize_with_options(const T& obj, const SerializeOptions& options = {}) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    rapidjson::Value json_value;
    serialize_with_options_impl(obj, json_value, allocator, options, 0);

    rapidjson::StringBuffer buffer;

    if (options.pretty_print) {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        json_value.Accept(writer);
    } else {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        json_value.Accept(writer);
    }

    return buffer.GetString();
}

// 实现带选项的序列化辅助函数
template<typename T>
void serialize_with_options_impl(const T& obj, rapidjson::Value& json_value,
                                 rapidjson::Document::AllocatorType& allocator,
                                 const SerializeOptions& options, int depth) {
    // 检查递归深度
    if (depth > options.max_recursion_depth) {
        json_value.SetNull();
        return;
    }

    if constexpr (HasJsonFieldMap<T>::value) {
        json_value.SetObject();
        // 获取字段映射
        auto fields = T::_json_field_map();
        // 使用辅助函数处理字段
        serialize_fields_with_options(obj, json_value, allocator, fields, options, depth + 1);
    } else {
        // 对于非结构体类型，使用标准序列化
        Serializer<T>::serialize(obj, json_value, allocator);
    }
}

// 添加serialize_fields_with_options相关函数
template<typename Obj, typename... Args>
void serialize_fields_with_options(const Obj& obj, rapidjson::Value& json_value,
                                  rapidjson::Document::AllocatorType& allocator,
                                  const std::tuple<Args...>& fields,
                                  const SerializeOptions& options, int depth) {
    serialize_fields_tuple_with_options(obj, json_value, allocator, fields, options, depth,
                                      std::make_index_sequence<sizeof...(Args) / 2>{});
}

template<typename Obj, typename... Args, size_t... I>
void serialize_fields_tuple_with_options(const Obj& obj, rapidjson::Value& json_value,
                                        rapidjson::Document::AllocatorType& allocator,
                                        const std::tuple<Args...>& fields,
                                        const SerializeOptions& options, int depth,
                                        std::index_sequence<I...>) {
    // 展开参数包并处理每个字段
    (serialize_field_with_options(obj, json_value, allocator,
                                std::get<I*2>(fields),
                                std::get<I*2+1>(fields),
                                options, depth), ...);
}

template<typename Obj>
void serialize_field_with_options(const Obj& obj, rapidjson::Value& json_value,
                                 rapidjson::Document::AllocatorType& allocator,
                                 const char* field_name, auto Obj::* field,
                                 const SerializeOptions& options, int depth) {
    // 检查字段是否应该被排除
    if (!options.excluded_fields.empty() &&
        std::find(options.excluded_fields.begin(), options.excluded_fields.end(), field_name) != options.excluded_fields.end()) {
        return;
    }

    // 检查字段是否应该被包含
    if (!options.included_fields.empty() &&
        std::find(options.included_fields.begin(), options.included_fields.end(), field_name) == options.included_fields.end()) {
        return;
    }

    // Improved handling of optional fields
    if constexpr (is_optional_v<std::decay_t<decltype(obj.*field)>>) {
        if (!(obj.*field).has_value()) {
            // If options require including null fields, add null
            if (options.include_null_fields) {
                rapidjson::Value field_value;
                field_value.SetNull();
                json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator);
            }
            return;
        }
        
        // Handle the optional value that exists
        rapidjson::Value field_value;
        serialize_with_options_impl((obj.*field).value(), field_value, allocator, options, depth);
        json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator);
    } else {
        // Handle non-optional field
        rapidjson::Value field_value;
        serialize_with_options_impl(obj.*field, field_value, allocator, options, depth);
        json_value.AddMember(rapidjson::Value(field_name, allocator).Move(), field_value, allocator);
    }
}

// 添加前向声明 - 应该放在 deserialize_with_defaults 函数之前
template<typename T, typename... Args>
void deserialize_with_defaults_impl(T& obj, const rapidjson::Value& json_value, const std::tuple<Args...>& fields);

template<typename T, typename... Args, size_t... I>
void deserialize_with_defaults_tuple(T& obj, const rapidjson::Value& json_value,
                                   const std::tuple<Args...>& fields,
                                   std::index_sequence<I...>);

template<typename T>
void deserialize_field_with_defaults(T& obj, const rapidjson::Value& json_value,
                                   const char* field_name, auto T::* field);

// 在 deserialize_with_defaults 函数之后添加这些函数的实现
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
    // 只有当字段在 JSON 中存在时才覆盖默认值
    if (json_value.HasMember(field_name)) {
        obj.*field = Serializer<std::decay_t<decltype(obj.*field)>>::deserialize(json_value[field_name]);
    }
    // 保留默认值，如果字段不存在
}

// 使用默认值的反序列化
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

// 序列化性能优化 - 字符串预分配
template<typename T>
std::string serialize_optimized(const T& obj, std::size_t reserved_size = 1024) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    rapidjson::Value json_value;
    Serializer<T>::serialize(obj, json_value, allocator);

    rapidjson::StringBuffer buffer;
    buffer.Reserve(reserved_size);  // 预分配内存
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    json_value.Accept(writer);

    return buffer.GetString();
}


// 添加 deserialize_strict 函数 - 应该放在 namespace json_serializer 内
template<typename T>
std::pair<T, SerializeError> deserialize_strict(const std::string& json_str, 
                                              const std::vector<std::string>& required_fields = {},
                                              const SerializeOptions::ValidationFunction& validator = nullptr) {
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

        // 检查必要字段
        for (const auto& field : required_fields) {
            if (!doc.HasMember(field.c_str())) {
                error.code = SerializeError::ErrorCode::MISSING_FIELD;
                error.message = "Missing required field: " + field;
                error.path = "$." + field;
                return {T{}, error};
            }
        }
    }

    // Run the custom validator if provided
    if (validator) {
        error = validator(doc);
        if (error.has_error()) {
            return {T{}, error};
        }
    }
    
    return {Serializer<T>::deserialize(doc), error};
}

// 内部字段注册机制 - 用于在结构体的public部分声明 JSON 字段映射
#define DECLARE_JSON_FIELDS(...) \
    static constexpr auto _json_field_map() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    \
    template<typename T> \
    friend struct json_serializer::Serializer;

std::string format_error(const SerializeError& error) {
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
