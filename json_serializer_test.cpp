#include "gtest/gtest.h"
#include "json_serializer.hpp"
#include <iostream>
#include <string>

// Helper to remove whitespace for json string comparison
std::string remove_whitespace(const std::string& s) {
    std::string result;
    result.reserve(s.length());
    for (char c : s) {
        if (!isspace(c)) {
            result += c;
        }
    }
    return result;
}

// 1. Test basic types
TEST(JsonSerializerTest, BasicTypes) {
    // Integer
    int a = 123;
    std::string json_a = json_serializer::serialize(a);
    EXPECT_EQ(json_a, "123");
    int deserialized_a = json_serializer::deserialize<int>(json_a);
    EXPECT_EQ(deserialized_a, a);

    // Double
    double b = 45.67;
    std::string json_b = json_serializer::serialize(b);
    EXPECT_DOUBLE_EQ(json_serializer::deserialize<double>(json_b), b);

    // Bool
    bool c = true;
    std::string json_c = json_serializer::serialize(c);
    EXPECT_EQ(json_c, "true");
    EXPECT_EQ(json_serializer::deserialize<bool>(json_c), c);

    // String
    std::string d = "hello world";
    std::string json_d = json_serializer::serialize(d);
    EXPECT_EQ(json_d, "\"hello world\"");
    EXPECT_EQ(json_serializer::deserialize<std::string>(json_d), d);
}

// 2. Test STL Containers
TEST(JsonSerializerTest, Vector) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string json = json_serializer::serialize(vec);
    EXPECT_EQ(json, "[1,2,3,4,5]");
    std::vector<int> deserialized_vec = json_serializer::deserialize<std::vector<int>>(json);
    EXPECT_EQ(deserialized_vec, vec);
}

TEST(JsonSerializerTest, List) {
    std::list<std::string> l = {"a", "b", "c"};
    std::string json = json_serializer::serialize(l);
    EXPECT_EQ(json, "[\"a\",\"b\",\"c\"]");
    auto deserialized_l = json_serializer::deserialize<std::list<std::string>>(json);
    EXPECT_EQ(deserialized_l, l);
}

TEST(JsonSerializerTest, Set) {
    std::set<int> s = {1, 2, 3};
    std::string json = json_serializer::serialize(s);
    EXPECT_EQ(json, "[1,2,3]");
    auto deserialized_s = json_serializer::deserialize<std::set<int>>(json);
    EXPECT_EQ(deserialized_s, s);
}

TEST(JsonSerializerTest, Array) {
    std::array<int, 3> arr = {10, 20, 30};
    std::string json = json_serializer::serialize(arr);
    EXPECT_EQ(json, "[10,20,30]");
    auto deserialized_arr = json_serializer::deserialize<std::array<int, 3>>(json);
    EXPECT_EQ(deserialized_arr, arr);
}

TEST(JsonSerializerTest, Map) {
    std::map<std::string, int> m = {{"one", 1}, {"two", 2}};
    std::string json = json_serializer::serialize(m);
    EXPECT_EQ(remove_whitespace(json), remove_whitespace("{\"one\":1,\"two\":2}"));
    auto deserialized_m = json_serializer::deserialize<std::map<std::string, int>>(json);
    EXPECT_EQ(deserialized_m, m);
}

TEST(JsonSerializerTest, MapWithIntKey) {
    std::map<int, std::string> m = {{1, "one"}, {2, "two"}};
    std::string json = json_serializer::serialize(m);
    EXPECT_EQ(remove_whitespace(json), remove_whitespace("{\"1\":\"one\",\"2\":\"two\"}"));
    auto deserialized_m = json_serializer::deserialize<std::map<int, std::string>>(json);
    EXPECT_EQ(deserialized_m, m);
}

TEST(JsonSerializerTest, UnorderedMap) {
    std::unordered_map<std::string, int> m = {{"one", 1}, {"two", 2}};
    std::string json = json_serializer::serialize(m);
    auto deserialized_m = json_serializer::deserialize<std::unordered_map<std::string, int>>(json);
    EXPECT_EQ(deserialized_m, m);
}

// 3. Test C++17 Features
TEST(JsonSerializerTest, Optional) {
    std::optional<int> opt_val = 42;
    std::string json_val = json_serializer::serialize(opt_val);
    EXPECT_EQ(json_val, "42");
    EXPECT_EQ(json_serializer::deserialize<std::optional<int>>(json_val), opt_val);

    std::optional<int> opt_null = std::nullopt;
    std::string json_null = json_serializer::serialize(opt_null);
    EXPECT_EQ(json_null, "null");
    EXPECT_EQ(json_serializer::deserialize<std::optional<int>>(json_null), opt_null);
}

TEST(JsonSerializerTest, Tuple) {
    std::tuple<int, std::string, bool> t = {1, "test", true};
    std::string json = json_serializer::serialize(t);
    EXPECT_EQ(json, "[1,\"test\",true]");
    auto deserialized_t = json_serializer::deserialize<std::tuple<int, std::string, bool>>(json);
    EXPECT_EQ(deserialized_t, t);
}

TEST(JsonSerializerTest, Variant) {
    std::variant<int, std::string> v1 = 123;
    std::string json1 = json_serializer::serialize(v1);
    EXPECT_EQ(remove_whitespace(json1), "{\"type_index\":0,\"data\":123}");
    auto deserialized_v1 = json_serializer::deserialize<std::variant<int, std::string>>(json1);
    EXPECT_EQ(v1, deserialized_v1);

    std::variant<int, std::string> v2 = "hello";
    std::string json2 = json_serializer::serialize(v2);
    EXPECT_EQ(remove_whitespace(json2), "{\"type_index\":1,\"data\":\"hello\"}");
    auto deserialized_v2 = json_serializer::deserialize<std::variant<int, std::string>>(json2);
    EXPECT_EQ(v2, deserialized_v2);
}

// 4. Test Enums
enum class MyEnum {
    Val1,
    Val2,
    Val3 = 10
};

TEST(JsonSerializerTest, EnumWithRequires) {
    MyEnum e = MyEnum::Val2;
    std::string json = json_serializer::serialize(e);
    EXPECT_EQ(json, "1");
    MyEnum de = json_serializer::deserialize<MyEnum>(json);
    EXPECT_EQ(e, de);
    
    e = MyEnum::Val3;
    json = json_serializer::serialize(e);
    EXPECT_EQ(json, "10");
    de = json_serializer::deserialize<MyEnum>(json);
    EXPECT_EQ(e, de);
}

// 5. Test Custom Structs
struct Address {
    std::string city;
    int zip_code;

    DECLARE_JSON_FIELDS(
        "city", &Address::city,
        "zip_code", &Address::zip_code
    )

    bool operator==(const Address& other) const {
        return city == other.city && zip_code == other.zip_code;
    }
};

struct Person {
    std::string name;
    int age;
    std::vector<std::string> hobbies;
    Address address;
    std::optional<std::string> nickname;

    DECLARE_JSON_FIELDS(
        "name", &Person::name,
        "age", &Person::age,
        "hobbies", &Person::hobbies,
        "address", &Person::address,
        "nickname", &Person::nickname
    )
     bool operator==(const Person& other) const {
        return name == other.name && age == other.age && hobbies == other.hobbies && address == other.address && nickname == other.nickname;
    }
};

TEST(JsonSerializerTest, CustomStruct) {
    Person p;
    p.name = "John Doe";
    p.age = 30;
    p.hobbies = {"coding", "reading"};
    p.address = {"New York", 10001};
    p.nickname = "Johnny";

    std::string json = json_serializer::serialize(p);
    Person deserialized_p = json_serializer::deserialize<Person>(json);

    EXPECT_EQ(p, deserialized_p);
}

// 6. Test legacy JSON_SERIALIZABLE macro
struct LegacyStruct {
    int id;
    std::string data;

    bool operator==(const LegacyStruct& other) const {
        return id == other.id && data == other.data;
    }
};

JSON_SERIALIZABLE(LegacyStruct, "id", &LegacyStruct::id, "data", &LegacyStruct::data)

TEST(JsonSerializerTest, LegacyMacro) {
    LegacyStruct ls{42, "legacy data"};
    std::string json = json_serializer::serialize(ls);
    LegacyStruct deserialized_ls = json_serializer::deserialize<LegacyStruct>(json);
    EXPECT_EQ(ls, deserialized_ls);
}

// 7. Test Serialization Options
TEST(JsonSerializerTest, SerializeWithOptions) {
    Person p;
    p.name = "Jane Doe";
    p.age = 28;
    // p.nickname is nullopt

    json_serializer::SerializeOptions options;

    // Test pretty print
    options.pretty_print = true;
    std::string pretty_json = json_serializer::serialize_with_options(p, options);
    EXPECT_TRUE(pretty_json.find('\n') != std::string::npos);
    EXPECT_TRUE(pretty_json.find("  ") != std::string::npos);
    options.pretty_print = false;

    // Test include_null_fields
    options.include_null_fields = true;
    std::string json_with_null = json_serializer::serialize_with_options(p, options);
    EXPECT_TRUE(json_with_null.find("\"nickname\":null") != std::string::npos);
    options.include_null_fields = false;
    
    // Test without null fields (default)
    std::string json_without_null = json_serializer::serialize_with_options(p, options);
    EXPECT_TRUE(json_without_null.find("nickname") == std::string::npos);

    // Test included_fields
    options.included_fields = {"name", "age"};
    std::string included_json = json_serializer::serialize_with_options(p, options);
    EXPECT_TRUE(included_json.find("name") != std::string::npos);
    EXPECT_TRUE(included_json.find("age") != std::string::npos);
    EXPECT_TRUE(included_json.find("hobbies") == std::string::npos);
    EXPECT_TRUE(included_json.find("address") == std::string::npos);
    options.included_fields.clear();

    // Test excluded_fields
    options.excluded_fields = {"hobbies", "address"};
    std::string excluded_json = json_serializer::serialize_with_options(p, options);
    EXPECT_TRUE(excluded_json.find("name") != std::string::npos);
    EXPECT_TRUE(excluded_json.find("age") != std::string::npos);
    EXPECT_TRUE(excluded_json.find("hobbies") == std::string::npos);
    EXPECT_TRUE(excluded_json.find("address") == std::string::npos);
    options.excluded_fields.clear();
}

// Forward declare for pointer serialization test
struct RecursiveStruct;

// Pointer serialization must be specialized
namespace json_serializer {
template<typename T>
struct Serializer<T*>{
    static void serialize(const T* value, rapidjson::Value& json_value, rapidjson::Document::AllocatorType& allocator) {
        if (value) {
            Serializer<T>::serialize(*value, json_value, allocator);
        } else {
            json_value.SetNull();
        }
    }
};

template<typename T>
void serialize_with_options_impl(T* const& obj, rapidjson::Value& json_value,
                                 rapidjson::Document::AllocatorType& allocator,
                                 const SerializeOptions& options, int depth);

template<typename T>
void serialize_with_options_impl(T* const& obj, rapidjson::Value& json_value,
                                 rapidjson::Document::AllocatorType& allocator,
                                 const SerializeOptions& options, int depth) {
    if (depth > options.max_recursion_depth) {
        json_value.SetNull();
        return;
    }
    if (obj) {
        serialize_with_options_impl(*obj, json_value, allocator, options, depth);
    } else {
        json_value.SetNull();
    }
}
}

struct RecursiveStruct {
    int id;
    std::optional<RecursiveStruct*> child; 

     DECLARE_JSON_FIELDS(
        "id", &RecursiveStruct::id,
        "child", &RecursiveStruct::child
    )
};

TEST(JsonSerializerTest, MaxRecursionDepth) {
    RecursiveStruct r3 {3, std::nullopt};
    RecursiveStruct r2 {2, &r3};
    RecursiveStruct r1 {1, &r2};

    json_serializer::SerializeOptions options;
    options.max_recursion_depth = 1;

    std::string json = json_serializer::serialize_with_options(r1, options);
    
    // r1 (depth 0) -> r2 (depth 1) -> r3 (depth 2).
    // The serialization of r3 will be triggered at depth 2.
    // The check is `depth > options.max_recursion_depth`, so `2 > 1` is true.
    // The `child` of r2 should be serialized as `null`.
    rapidjson::Document deserialized;
    deserialized.Parse(json.c_str());
    ASSERT_FALSE(deserialized.HasParseError());
    
    ASSERT_TRUE(deserialized.IsObject());
    ASSERT_TRUE(deserialized.HasMember("child"));
    ASSERT_TRUE(deserialized["child"].IsObject());
    ASSERT_TRUE(deserialized["child"].HasMember("child"));
    EXPECT_TRUE(deserialized["child"]["child"].IsNull());
}


// 8. Test Error Handling & Strict Deserialization
TEST(JsonSerializerTest, DeserializeStrict) {
    std::string good_json = R"({"name": "Test", "age": 99})";
    std::string missing_field_json = R"({"name": "Test"})";
    std::string bad_type_json = R"({"name": "Test", "age": "99"})";
    
    // Good case
    auto [p1, err1] = json_serializer::deserialize_strict<Person>(good_json, {"name", "age"});
    EXPECT_FALSE(err1.has_error());
    EXPECT_EQ(p1.name, "Test");
    EXPECT_EQ(p1.age, 99);

    // Missing field
    auto [p2, err2] = json_serializer::deserialize_strict<Person>(missing_field_json, {"name", "age"});
    EXPECT_TRUE(err2.has_error());
    EXPECT_EQ(err2.code, json_serializer::SerializeError::ErrorCode::MISSING_FIELD);

    // Bad type (will be caught by rapidjson, deserialize will fail)
    auto [p3, err3] = json_serializer::deserialize_strict<Person>(bad_type_json, {"name", "age"});
    EXPECT_FALSE(err3.has_error()); // deserialize returns default object on member type error.
    EXPECT_EQ(p3.age, 0); // age fails to parse, gets default value
    EXPECT_EQ(p3.name, "Test");

    // Custom validator
    auto validator = [](const rapidjson::Value& doc) -> json_serializer::SerializeError {
        if (doc["age"].GetInt() < 18) {
            return {json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR, "User is underage"};
        }
        return {};
    };
    std::string underage_json = R"({"name": "Kid", "age": 10})";
    auto [p4, err4] = json_serializer::deserialize_strict<Person>(underage_json, {"name", "age"}, validator);
    EXPECT_TRUE(err4.has_error());
    EXPECT_EQ(err4.code, json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR);
}

// 9. Test Deserialization with Defaults
TEST(JsonSerializerTest, DeserializeWithDefaults) {
    Person defaults;
    defaults.name = "Default Name";
    defaults.age = 25;

    std::string partial_json = R"({"name": "Provided Name"})";
    Person p = json_serializer::deserialize_with_defaults(partial_json, defaults);

    EXPECT_EQ(p.name, "Provided Name");
    EXPECT_EQ(p.age, 25); // from defaults
}

// 10. Test Optimized Serialization
TEST(JsonSerializerTest, SerializeOptimized) {
    Person p;
    p.name = "John Doe";
    p.age = 30;
    
    std::string json1 = json_serializer::serialize(p);
    std::string json2 = json_serializer::serialize_optimized(p);
    EXPECT_EQ(remove_whitespace(json1), remove_whitespace(json2));
}

// 11. Format Error Test
TEST(JsonSerializerTest, FormatError) {
    json_serializer::SerializeError err;
    err.code = json_serializer::SerializeError::ErrorCode::MISSING_FIELD;
    err.message = "Field 'id' is required";
    err.path = "user.profile";
    
    std::string formatted_error = json_serializer::format_error(err);
    EXPECT_EQ(formatted_error, "Error: Missing field - Field 'id' is required (at user.profile)");
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
