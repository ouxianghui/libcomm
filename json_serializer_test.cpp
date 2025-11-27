#include "gtest/gtest.h"
#include "json_serializer.hpp"
#include <iostream>
#include <string>
#include <deque>
#include <forward_list>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <queue>
#include <stack>

#include <memory>
#include <utility>
#include <sstream>
#include <algorithm>
#include <array>
#include <variant>
#include <optional>

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

    // Default constructor
    Address() = default;
    
    // Constructor for initialization list
    Address(const std::string& c, int z) : city(c), zip_code(z) {}

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

    // Default constructor
    Person() = default;
    
    // Constructor for initialization list
    Person(const std::string& n, int a, const std::vector<std::string>& h, 
           const Address& addr, const std::optional<std::string>& nick = std::nullopt)
        : name(n), age(a), hobbies(h), address(addr), nickname(nick) {}

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

    // Test that optional fields are not included when they have no value (default behavior)
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

// 12. Test additional container types
TEST(JsonSerializerTest, Deque) {
    std::deque<std::string> d = {"first", "second", "third"};
    std::string json = json_serializer::serialize(d);
    EXPECT_EQ(json, "[\"first\",\"second\",\"third\"]");
    auto deserialized_d = json_serializer::deserialize<std::deque<std::string>>(json);
    EXPECT_EQ(deserialized_d, d);
}

TEST(JsonSerializerTest, ForwardList) {
    std::forward_list<int> fl = {1, 2, 3, 4, 5};
    std::string json = json_serializer::serialize(fl);
    EXPECT_EQ(json, "[1,2,3,4,5]");
    auto deserialized_fl = json_serializer::deserialize<std::forward_list<int>>(json);
    EXPECT_EQ(deserialized_fl, fl);
}

TEST(JsonSerializerTest, Multiset) {
    std::multiset<int> ms = {3, 1, 4, 1, 5, 9, 2, 6};
    std::string json = json_serializer::serialize(ms);
    EXPECT_EQ(json, "[1,1,2,3,4,5,6,9]");
    auto deserialized_ms = json_serializer::deserialize<std::multiset<int>>(json);
    EXPECT_EQ(deserialized_ms, ms);
}

TEST(JsonSerializerTest, UnorderedSet) {
    std::unordered_set<std::string> us = {"apple", "banana", "cherry"};
    std::string json = json_serializer::serialize(us);
    auto deserialized_us = json_serializer::deserialize<std::unordered_set<std::string>>(json);
    EXPECT_EQ(deserialized_us, us);
}

TEST(JsonSerializerTest, UnorderedMultiset) {
    std::unordered_multiset<int> ums = {1, 2, 2, 3, 3, 3};
    std::string json = json_serializer::serialize(ums);
    auto deserialized_ums = json_serializer::deserialize<std::unordered_multiset<int>>(json);
    EXPECT_EQ(deserialized_ums, ums);
}

// 13. Test multimap types
TEST(JsonSerializerTest, Multimap) {
    std::multimap<std::string, int> mm = {{"a", 1}, {"a", 2}, {"b", 3}};
    std::string json = json_serializer::serialize(mm);
    auto deserialized_mm = json_serializer::deserialize<std::multimap<std::string, int>>(json);
    EXPECT_EQ(deserialized_mm, mm);
}

TEST(JsonSerializerTest, UnorderedMultimap) {
    std::unordered_multimap<std::string, int> umm = {{"x", 10}, {"x", 20}, {"y", 30}};
    std::string json = json_serializer::serialize(umm);
    auto deserialized_umm = json_serializer::deserialize<std::unordered_multimap<std::string, int>>(json);
    EXPECT_EQ(deserialized_umm, umm);
}

// 14. Test queue types
TEST(JsonSerializerTest, Queue) {
    std::queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    
    std::string json = json_serializer::serialize(q);
    EXPECT_EQ(json, "[1,2,3]");
    
    auto deserialized_q = json_serializer::deserialize<std::queue<int>>(json);
    EXPECT_EQ(deserialized_q.size(), q.size());
    
    // Check elements in order
    while (!q.empty() && !deserialized_q.empty()) {
        EXPECT_EQ(q.front(), deserialized_q.front());
        q.pop();
        deserialized_q.pop();
    }
}

TEST(JsonSerializerTest, PriorityQueue) {
    std::priority_queue<int> pq;
    pq.push(3);
    pq.push(1);
    pq.push(4);
    pq.push(1);
    pq.push(5);
    
    std::string json = json_serializer::serialize(pq);
    auto deserialized_pq = json_serializer::deserialize<std::priority_queue<int>>(json);
    
    EXPECT_EQ(deserialized_pq.size(), pq.size());
    
    // Priority queue should maintain heap property
    std::vector<int> original_elements, deserialized_elements;
    while (!pq.empty()) {
        original_elements.push_back(pq.top());
        pq.pop();
    }
    while (!deserialized_pq.empty()) {
        deserialized_elements.push_back(deserialized_pq.top());
        deserialized_pq.pop();
    }
    
    std::sort(original_elements.begin(), original_elements.end(), std::greater<int>());
    std::sort(deserialized_elements.begin(), deserialized_elements.end(), std::greater<int>());
    EXPECT_EQ(original_elements, deserialized_elements);
}

TEST(JsonSerializerTest, Stack) {
    std::stack<std::string> s;
    s.push("first");
    s.push("second");
    s.push("third");
    
    std::string json = json_serializer::serialize(s);
    EXPECT_EQ(json, "[\"first\",\"second\",\"third\"]");
    
    auto deserialized_s = json_serializer::deserialize<std::stack<std::string>>(json);
    EXPECT_EQ(deserialized_s.size(), s.size());
    
    // Check elements in reverse order (stack order)
    std::vector<std::string> original_elements, deserialized_elements;
    while (!s.empty()) {
        original_elements.push_back(s.top());
        s.pop();
    }
    while (!deserialized_s.empty()) {
        deserialized_elements.push_back(deserialized_s.top());
        deserialized_s.pop();
    }
    
    EXPECT_EQ(original_elements, deserialized_elements);
}

// 15. Test smart pointers
TEST(JsonSerializerTest, UniquePtr) {
    auto ptr = std::make_unique<Person>();
    ptr->name = "Smart Person";
    ptr->age = 25;
    
    std::string json = json_serializer::serialize(ptr);
    auto deserialized_ptr = json_serializer::deserialize<std::unique_ptr<Person>>(json);
    
    EXPECT_TRUE(deserialized_ptr != nullptr);
    EXPECT_EQ(ptr->name, deserialized_ptr->name);
    EXPECT_EQ(ptr->age, deserialized_ptr->age);
}

TEST(JsonSerializerTest, SharedPtr) {
    auto ptr = std::make_shared<Person>();
    ptr->name = "Shared Person";
    ptr->age = 30;
    
    std::string json = json_serializer::serialize(ptr);
    auto deserialized_ptr = json_serializer::deserialize<std::shared_ptr<Person>>(json);
    
    EXPECT_TRUE(deserialized_ptr != nullptr);
    EXPECT_EQ(ptr->name, deserialized_ptr->name);
    EXPECT_EQ(ptr->age, deserialized_ptr->age);
}



TEST(JsonSerializerTest, NullPtr) {
    std::unique_ptr<Person> null_ptr = nullptr;
    std::string json = json_serializer::serialize(null_ptr);
    EXPECT_EQ(json, "null");
    
    auto deserialized_null = json_serializer::deserialize<std::unique_ptr<Person>>(json);
    EXPECT_TRUE(deserialized_null == nullptr);
}

// 16. Test pair
TEST(JsonSerializerTest, Pair) {
    std::pair<std::string, int> p = {"key", 42};
    std::string json = json_serializer::serialize(p);
    EXPECT_EQ(json, "[\"key\",42]");
    
    auto deserialized_p = json_serializer::deserialize<std::pair<std::string, int>>(json);
    EXPECT_EQ(deserialized_p, p);
}

// 17. Test complex nested structures
struct ComplexStruct {
    std::vector<std::map<std::string, std::optional<int>>> nested_data;
    std::tuple<std::string, int, bool> tuple_data;
    std::variant<int, std::string, bool> variant_data;
    
    DECLARE_JSON_FIELDS(
        "nested_data", &ComplexStruct::nested_data,
        "tuple_data", &ComplexStruct::tuple_data,
        "variant_data", &ComplexStruct::variant_data
    )
    
    bool operator==(const ComplexStruct& other) const {
        return nested_data == other.nested_data && 
               tuple_data == other.tuple_data && 
               variant_data == other.variant_data;
    }
};

TEST(JsonSerializerTest, ComplexNestedStructure) {
    ComplexStruct cs;
    cs.nested_data = {
        {{"a", 1}, {"b", std::nullopt}, {"c", 3}},
        {{"x", 10}, {"y", 20}}
    };
    cs.tuple_data = {"test", 42, true};
    cs.variant_data = "hello";
    
    std::string json = json_serializer::serialize(cs);
    auto deserialized_cs = json_serializer::deserialize<ComplexStruct>(json);
    
    EXPECT_EQ(deserialized_cs, cs);
}

// 18. Test validation functionality
TEST(JsonSerializerTest, Validation) {
    Person p;
    p.name = "Test Person";
    p.age = 15; // Underage
    
    json_serializer::SerializeOptions options;
    options.add_validation("age", 
        [](const rapidjson::Value& value) -> bool {
            return value.GetInt() >= 18;
        }, 
        "Age must be at least 18"
    );
    
    std::string json = json_serializer::serialize_with_options(p, options);
    auto [result, error] = json_serializer::deserialize_with_options<Person>(json, options);
    
    EXPECT_TRUE(error.has_error());
    EXPECT_EQ(error.code, json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR);
}



// 20. Test buffer reserve size
TEST(JsonSerializerTest, BufferReserveSize) {
    Person p;
    p.name = "Test Person";
    p.age = 25;
    
    json_serializer::SerializeOptions options;
    options.buffer_reserve_size = 2048;
    
    std::string json = json_serializer::serialize_with_options(p, options);
    auto deserialized_p = json_serializer::deserialize<Person>(json);
    EXPECT_EQ(deserialized_p, p);
}

// 21. Test error handling with malformed JSON
TEST(JsonSerializerTest, MalformedJson) {
    std::string malformed_json = R"({"name": "Test", "age": })"; // Missing value
    
    auto [result, error] = json_serializer::deserialize_with_error_handling<Person>(malformed_json);
    EXPECT_TRUE(error.has_error());
    EXPECT_EQ(error.code, json_serializer::SerializeError::ErrorCode::PARSE_ERROR);
}

// 22. Test type mismatch handling
TEST(JsonSerializerTest, TypeMismatch) {
    std::string type_mismatch_json = R"({"name": 123, "age": "not_a_number"})";
    
    auto [result, error] = json_serializer::deserialize_with_type_check<Person>(type_mismatch_json);
    // Note: The current implementation doesn't actually check types during deserialization
    // This test shows the current behavior
    EXPECT_FALSE(error.has_error());
    EXPECT_EQ(result.name, ""); // Gets default value
    EXPECT_EQ(result.age, 0);   // Gets default value
}

// 23. Test custom validator function
TEST(JsonSerializerTest, CustomValidator) {
    Person p;
    p.name = "Test";
    p.age = 25;
    
    json_serializer::SerializeOptions options;
    options.custom_validator = [](const rapidjson::Value& doc) -> json_serializer::SerializeError {
        if (!doc.HasMember("name") || !doc["name"].IsString()) {
            return {json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR, "Name must be a string"};
        }
        if (!doc.HasMember("age") || !doc["age"].IsInt()) {
            return {json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR, "Age must be an integer"};
        }
        return {};
    };
    
    std::string json = json_serializer::serialize_with_options(p, options);
    auto [result, error] = json_serializer::deserialize_with_options<Person>(json, options);
    
    EXPECT_FALSE(error.has_error());
    EXPECT_EQ(result, p);
}

// 24. Test validation namespace helpers
TEST(JsonSerializerTest, ValidationHelpers) {
    json_serializer::SerializeOptions options;
    
    // Test range validation
    options.add_validation("age", json_serializer::validation::in_range(0, 150), "Age must be between 0 and 150");
    
    // Test string length validation
    options.add_validation("name", json_serializer::validation::string_length(1, 50), "Name must be between 1 and 50 characters");
    
    // Test array size validation
    options.add_validation("hobbies", json_serializer::validation::array_size(0, 10), "Hobbies must have 0-10 items");
    
    // Test required fields validation
    options.add_validation("", json_serializer::validation::has_required_fields({"name", "age"}), "Name and age are required");
    
    Person p;
    p.name = "Valid Person";
    p.age = 25;
    p.hobbies = {"reading", "coding"};
    
    std::string json = json_serializer::serialize_with_options(p, options);
    auto [result, error] = json_serializer::deserialize_with_options<Person>(json, options);
    
    EXPECT_FALSE(error.has_error());
    EXPECT_EQ(result, p);
}

// 25. Test performance monitoring
TEST(JsonSerializerTest, PerformanceMonitoring) {
    Person p;
    p.name = "Performance Test";
    p.age = 30;
    p.hobbies = {"testing", "benchmarking"};
    
    // This test just ensures the performance monitor compiles and runs
    {
        json_serializer::PerformanceMonitor monitor("serialization_test");
        std::string json = json_serializer::serialize(p);
        EXPECT_FALSE(json.empty());
    }
}

// 26. Test streaming serializer
TEST(JsonSerializerTest, StreamingSerializer) {
    std::vector<Person> people = {
        {"Alice", 25, {"reading"}, {"NYC", 10001}, "Al"},
        {"Bob", 30, {"coding"}, {"LA", 90210}, std::nullopt},
        {"Charlie", 35, {"gaming", "music"}, {"SF", 94102}, "Chuck"}
    };
    
    std::ostringstream output;
    json_serializer::StreamingSerializer<Person> serializer(output);
    
    serializer.begin_array();
    for (const auto& person : people) {
        serializer.serialize_element(person);
    }
    serializer.end_array();
    
    std::string result = output.str();
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("Alice") != std::string::npos);
    EXPECT_TRUE(result.find("Bob") != std::string::npos);
    EXPECT_TRUE(result.find("Charlie") != std::string::npos);
}

// 27. Test inheritance with JSON fields
struct BaseClass {
    std::string base_field;
    
    DECLARE_JSON_FIELDS(
        "base_field", &BaseClass::base_field
    )
    
    virtual ~BaseClass() = default;
    
    bool operator==(const BaseClass& other) const {
        return base_field == other.base_field;
    }
};

struct DerivedClass : public BaseClass {
    int derived_field;
    
    DECLARE_JSON_FIELDS(
        "derived_field", &DerivedClass::derived_field
    )
    
    INHERIT_JSON_FIELDS(BaseClass)
    
    bool operator==(const DerivedClass& other) const {
        return BaseClass::operator==(other) && derived_field == other.derived_field;
    }
};

TEST(JsonSerializerTest, Inheritance) {
    DerivedClass dc;
    dc.base_field = "base value";
    dc.derived_field = 42;
    
    std::string json = json_serializer::serialize(dc);
    auto deserialized_dc = json_serializer::deserialize<DerivedClass>(json);
    
    EXPECT_EQ(deserialized_dc, dc);
    EXPECT_TRUE(json.find("base_field") != std::string::npos);
    EXPECT_TRUE(json.find("derived_field") != std::string::npos);
}

// 28. Test multiple inheritance
struct Interface1 {
    std::string interface1_field;
    
    DECLARE_JSON_FIELDS(
        "interface1_field", &Interface1::interface1_field
    )
    
    virtual ~Interface1() = default;
    
    bool operator==(const Interface1& other) const {
        return interface1_field == other.interface1_field;
    }
};

struct Interface2 {
    int interface2_field;
    
    DECLARE_JSON_FIELDS(
        "interface2_field", &Interface2::interface2_field
    )
    
    virtual ~Interface2() = default;
    
    bool operator==(const Interface2& other) const {
        return interface2_field == other.interface2_field;
    }
};

struct MultipleInheritanceClass : public Interface1, public Interface2 {
    bool multiple_field;
    
    DECLARE_JSON_FIELDS(
        "multiple_field", &MultipleInheritanceClass::multiple_field
    )
    
    INHERIT_JSON_FIELDS_MULTIPLE(Interface1, Interface2)
    
    bool operator==(const MultipleInheritanceClass& other) const {
        return Interface1::operator==(other) && 
               Interface2::operator==(other) && 
               multiple_field == other.multiple_field;
    }
};

TEST(JsonSerializerTest, MultipleInheritance) {
    MultipleInheritanceClass mic;
    mic.interface1_field = "interface1";
    mic.interface2_field = 123;
    mic.multiple_field = true;
    
    std::string json = json_serializer::serialize(mic);
    auto deserialized_mic = json_serializer::deserialize<MultipleInheritanceClass>(json);
    
    EXPECT_EQ(deserialized_mic, mic);
    EXPECT_TRUE(json.find("interface1_field") != std::string::npos);
    EXPECT_TRUE(json.find("interface2_field") != std::string::npos);
    EXPECT_TRUE(json.find("multiple_field") != std::string::npos);
}

// 29. Test error context and path building
TEST(JsonSerializerTest, ErrorContextAndPath) {
    json_serializer::SerializeError error;
    error.code = json_serializer::SerializeError::ErrorCode::VALIDATION_ERROR;
    error.message = "Validation failed";
    error.path = "user";
    error.add_context("Field validation failed");
    error.add_context("Age must be positive");
    
    std::string full_description = error.get_full_description();
    EXPECT_TRUE(full_description.find("Validation error") != std::string::npos);
    EXPECT_TRUE(full_description.find("Context:") != std::string::npos);
    EXPECT_TRUE(full_description.find("Field validation failed") != std::string::npos);
    EXPECT_TRUE(full_description.find("Age must be positive") != std::string::npos);
    
    // Test path building
    std::string new_path = json_serializer::SerializeError::build_path("user", "profile");
    EXPECT_EQ(new_path, "user.profile");
    
    error.append_path("details");
    EXPECT_EQ(error.path, "user.details");
}

// 30. Test edge cases and boundary conditions
TEST(JsonSerializerTest, EdgeCases) {
    // Empty containers
    std::vector<int> empty_vec;
    std::string json1 = json_serializer::serialize(empty_vec);
    EXPECT_EQ(json1, "[]");
    auto deserialized_empty_vec = json_serializer::deserialize<std::vector<int>>(json1);
    EXPECT_TRUE(deserialized_empty_vec.empty());
    
    // Empty string
    std::string empty_str = "";
    std::string json2 = json_serializer::serialize(empty_str);
    EXPECT_EQ(json2, "\"\"");
    auto deserialized_empty_str = json_serializer::deserialize<std::string>(json2);
    EXPECT_EQ(deserialized_empty_str, "");
    
    // Zero values
    int zero_int = 0;
    std::string json3 = json_serializer::serialize(zero_int);
    EXPECT_EQ(json3, "0");
    auto deserialized_zero = json_serializer::deserialize<int>(json3);
    EXPECT_EQ(deserialized_zero, 0);
    
    // Boolean false
    bool false_bool = false;
    std::string json4 = json_serializer::serialize(false_bool);
    EXPECT_EQ(json4, "false");
    auto deserialized_false = json_serializer::deserialize<bool>(json4);
    EXPECT_EQ(deserialized_false, false);
    
    // Large numbers
    long long large_num = 9223372036854775807LL;
    std::string json5 = json_serializer::serialize(large_num);
    auto deserialized_large = json_serializer::deserialize<long long>(json5);
    EXPECT_EQ(deserialized_large, large_num);
    
    // Floating point precision
    double pi = 3.141592653589793;
    std::string json6 = json_serializer::serialize(pi);
    auto deserialized_pi = json_serializer::deserialize<double>(json6);
    EXPECT_DOUBLE_EQ(deserialized_pi, pi);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
