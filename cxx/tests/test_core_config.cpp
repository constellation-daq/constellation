/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/Value.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::utils;
using namespace std::string_view_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)
// NOLINTBEGIN(google-readability-casting,readability-redundant-casting)

TEST_CASE("Set & Get Values", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);

    config.set("int64", std::int64_t(63));
    config.set("size", std::size_t(1));
    config.set("uint64", std::uint64_t(64));
    config.set("uint8", std::uint8_t(8));

    config.set("double", double(1.3));
    config.set("float", float(3.14));

    config.set("string", std::string("a"));
    config.set("string_view", std::string_view("b"));
    config.set("char_array", "c");

    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.set("myenum", MyEnum::ONE);

    auto tp = std::chrono::system_clock::now();
    config.set("time", tp);

    // Check that keys are unused
    REQUIRE(config.size() == config.size(Configuration::Group::ALL, Configuration::Usage::UNUSED));

    // Read values back
    REQUIRE(config.get<bool>("bool") == true);

    REQUIRE(config.get<std::int64_t>("int64") == 63);
    REQUIRE(config.get<std::size_t>("size") == 1);
    REQUIRE(config.get<std::uint64_t>("uint64") == 64);
    REQUIRE(config.get<std::uint8_t>("uint8") == 8);

    REQUIRE(config.get<double>("double") == 1.3);
    REQUIRE(config.get<float>("float") == 3.14F);

    REQUIRE(config.get<std::string>("string") == "a");
    REQUIRE(config.get<std::string>("string_view") == "b");
    REQUIRE(config.get<std::string>("char_array") == "c");

    REQUIRE(config.get<MyEnum>("myenum") == MyEnum::ONE);

    REQUIRE(config.get<std::chrono::system_clock::time_point>("time") == tp);

    // Check that all keys have been marked as used
    REQUIRE(config.size(Configuration::Group::ALL, Configuration::Usage::UNUSED) == 0);
}

TEST_CASE("Keys Are Case-Insensitive", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);
    config.set("INT64", std::int64_t(63));

    REQUIRE(config.get<bool>("BOOL") == true);
    REQUIRE(config.get<bool>("bool") == true);

    REQUIRE(config.get<std::int64_t>("int64") == 63);
    REQUIRE(config.get<std::int64_t>("INT64") == 63);
}

TEST_CASE("Enum Values Are Case-Insensitive", "[core][core::config]") {
    Configuration config {};

    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.set("myenum", MyEnum::ONE);
    // Enum from string
    config.set("myenumstr", "ONE");
    // Enum case-insensitive from string
    config.set("myenumstr2", "oNe");

    REQUIRE(config.get<MyEnum>("myenum") == MyEnum::ONE);
    REQUIRE(config.get<MyEnum>("myenumstr") == MyEnum::ONE);
    REQUIRE(config.get<MyEnum>("myenumstr2") == MyEnum::ONE);
}

TEST_CASE("Set & Get Array Values", "[core][core::config]") {
    Configuration config {};

    config.setArray<bool>("bool", {true, false, true});

    config.setArray<std::int64_t>("int64", {63, 62, 61});
    config.setArray<std::size_t>("size", {1, 2, 3});
    config.setArray<std::uint64_t>("uint64", {64, 65, 66});
    config.setArray<std::uint8_t>("uint8", {8, 7, 6});

    config.setArray<double>("double", {1.3, 3.1});
    config.setArray<float>("float", {3.14F, 1.43F});

    config.setArray<char>("binary", {0x1, 0x2, 0x3});

    config.setArray<std::string>("string", {"a", "b", "c"});

    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.setArray<MyEnum>("myenum", {MyEnum::ONE, MyEnum::TWO});

    auto tp = std::chrono::system_clock::now();
    config.setArray<std::chrono::system_clock::time_point>("time", {tp, tp, tp});

    // Empty vector:
    config.setArray<double>("empty", {});

    // Read values back
    REQUIRE(config.getArray<bool>("bool") == std::vector<bool>({true, false, true}));

    REQUIRE(config.getArray<std::int64_t>("int64") == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(config.getArray<size_t>("size") == std::vector<size_t>({1, 2, 3}));
    REQUIRE(config.getArray<std::uint64_t>("uint64") == std::vector<std::uint64_t>({64, 65, 66}));
    REQUIRE(config.getArray<std::uint8_t>("uint8") == std::vector<std::uint8_t>({8, 7, 6}));

    REQUIRE(config.getArray<double>("double") == std::vector<double>({1.3, 3.1}));
    REQUIRE(config.getArray<float>("float") == std::vector<float>({3.14F, 1.43F}));

    REQUIRE(config.getArray<char>("binary") == std::vector<char>({0x1, 0x2, 0x3}));

    REQUIRE(config.getArray<std::string>("string") == std::vector<std::string>({"a", "b", "c"}));

    REQUIRE(config.getArray<std::chrono::system_clock::time_point>("time") ==
            std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));

    REQUIRE(config.getArray<double>("empty").empty());
}

TEST_CASE("Handle monostate", "[core][core::config]") {
    Configuration config {};

    config.set<std::monostate>("monostate", {});

    REQUIRE(config.get<std::monostate>("monostate") == std::monostate {});
    REQUIRE(config.get<std::vector<double>>("monostate").empty());

    REQUIRE_THROWS_AS(config.get<double>("monostate"), InvalidTypeError);
    REQUIRE_THROWS_MATCHES(config.get<double>("monostate"),
                           InvalidTypeError,
                           Message("Could not convert value of type 'std::monostate' to type 'double' for key 'monostate'"));

    REQUIRE(config.getText("monostate") == "NIL");
}

TEST_CASE("Set & Get Path Values", "[core][core::config]") {
    Configuration config {};

    config.set<std::string>("path", "/tmp/somefile.txt");
    config.set<double>("tisnotapath", 16.5);
    config.setArray<std::string>("patharray", {"/tmp/somefile.txt", "/tmp/someotherfile.txt"});
    config.setArray<double>("tisnotapatharray", {16.5, 17.5});
    config.set<std::string>("relpath", "somefile.txt");

    // Read path without canonicalization
    REQUIRE(config.getPath("path") == std::filesystem::path("/tmp/somefile.txt"));

    // Attempt to read value that is not a string:
    REQUIRE_THROWS_AS(config.getPath("tisnotapath"), InvalidTypeError);

    // Read path without canonicalization, setting an extension
    REQUIRE(config.getPathWithExtension("path", "ini") == std::filesystem::path("/tmp/somefile.ini"));
    REQUIRE_THROWS_AS(config.getPathWithExtension("path", "ini", true), InvalidValueError);

    // Read path with check for existence
    REQUIRE_THROWS_AS(config.getPath("path", true), InvalidValueError);
    REQUIRE_THROWS_MATCHES(config.getPath("path", true),
                           InvalidValueError,
                           Message("Value /tmp/somefile.txt of key 'path' is not valid: path /tmp/somefile.txt not found"));

    // Read relative path
    auto relpath = config.getPath("relpath");
    REQUIRE(!std::filesystem::relative(relpath, std::filesystem::current_path()).empty());

    // Read path array without canonicalization
    REQUIRE(config.getPathArray("patharray") ==
            std::vector<std::filesystem::path>({"/tmp/somefile.txt", "/tmp/someotherfile.txt"}));

    // Read path array with check for existence
    REQUIRE_THROWS_MATCHES(config.getPathArray("patharray", true),
                           InvalidValueError,
                           Message("Value [/tmp/somefile.txt, /tmp/someotherfile.txt] of key 'patharray' is not valid: path "
                                   "/tmp/somefile.txt not found"));

    // Attempt to read value that is not a string:
    REQUIRE_THROWS_AS(config.getPathArray("tisnotapatharray"), InvalidTypeError);

    // config.setArray<size_t>("my_size_t_array", {1, 2, 3});
    // REQUIRE(config.getArray<size_t>("my_size_t_array") == std::vector<size_t>({1, 2, 3}));
}

TEST_CASE("Access Values as Text", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);
    config.set("int64", std::int64_t(63));
    config.set("size", std::size_t(1));
    config.set("uint64", std::uint64_t(64));
    config.set("uint8", std::uint8_t(8));
    config.set("double", double(1.3));
    config.set("float", float(7.5));
    config.set("string", std::string("a"));

    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.set("myenum", MyEnum::ONE);

    const std::chrono::time_point<std::chrono::system_clock> tp {};
    config.set("time", tp);

    // Compare text representation
    REQUIRE(config.getText("bool") == "true");
    REQUIRE(config.getText("int64") == "63");
    REQUIRE(config.getText("size") == "1");
    REQUIRE(config.getText("uint64") == "64");
    REQUIRE(config.getText("uint8") == "8");
    REQUIRE(config.getText("double") == "1.3");
    REQUIRE(config.getText("float") == "7.5");
    REQUIRE(config.getText("string") == "a");
    REQUIRE(config.getText("myenum") == "ONE");
    REQUIRE_THAT(config.getText("time"), ContainsSubstring("1970-01-01 00:00:00.000000"));
}

TEST_CASE("Access Arrays as Text", "[core][core::config]") {
    Configuration config {};

    config.setArray<bool>("bool", {true, false, true});

    config.setArray<std::int64_t>("int64", {63, 62, 61});
    config.setArray<std::size_t>("size", {1, 2, 3});
    config.setArray<std::uint64_t>("uint64", {64, 65, 66});
    config.setArray<std::uint8_t>("uint8", {8, 7, 6});

    config.setArray<double>("double", {1.3, 3.1});
    config.setArray<float>("float", {1.0F, 7.5F});

    config.setArray<char>("binary", {0x1, 0xA, 0x1F});

    config.setArray<std::string>("string", {"a", "b", "c"});

    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.setArray<MyEnum>("myenum", {MyEnum::ONE, MyEnum::TWO});

    const std::chrono::system_clock::time_point tp {};
    config.setArray<std::chrono::system_clock::time_point>("time", {tp, tp, tp});

    REQUIRE(config.getText("bool") == "[true, false, true]");
    REQUIRE(config.getText("int64") == "[63, 62, 61]");
    REQUIRE(config.getText("size") == "[1, 2, 3]");
    REQUIRE(config.getText("uint64") == "[64, 65, 66]");
    REQUIRE(config.getText("uint8") == "[8, 7, 6]");
    REQUIRE(config.getText("double") == "[1.3, 3.1]");
    REQUIRE(config.getText("float") == "[1, 7.5]");
    REQUIRE(config.getText("binary") == "[ 0x01 0x0A 0x1F ]");
    REQUIRE(config.getText("string") == "[a, b, c]");
    REQUIRE_THAT(config.getText("time"),
                 RegexMatcher("\\[1970-01-01 00:00:00.0{6,}, 1970-01-01 00:00:00.0{6,}, 1970-01-01 00:00:00.0{6,}\\]", {}));
}

TEST_CASE("Count Key Appearances", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);
    config.set("int64", std::int64_t(63));

    REQUIRE(config.count({"nokey", "otherkey"}) == 0);
    REQUIRE(config.count({"bool", "notbool"}) == 1);
    REQUIRE(config.count({"bool", "int64"}) == 2);

    REQUIRE_THROWS_AS(config.count({}), std::invalid_argument);
    REQUIRE_THROWS_MATCHES(config.count({}), std::invalid_argument, Message("list of keys cannot be empty"));
}

TEST_CASE("Set Value & Mark Used", "[core][core::config]") {
    Configuration config {};

    config.set("myval", 3.14, true);

    // Check that the key is marked as used
    REQUIRE(config.size(Configuration::Group::ALL, Configuration::Usage::UNUSED) == 0);
    REQUIRE(config.get<double>("myval") == 3.14);
}

TEST_CASE("Configuration size", "[core][core::config]") {
    using enum Configuration::Group;
    using enum Configuration::Usage;

    Configuration config {};

    config.set("myval1", 1);
    config.set("myval2", 2);
    config.set("myval3", 3);
    config.set("_internal1", 4);
    config.set("_internal2", 5);
    config.set("_internal3", 6);
    config.set("_internal4", 7);

    REQUIRE(config.size(ALL, ANY) == 7);
    REQUIRE(config.size(ALL, USED) == 0);
    REQUIRE(config.size(ALL, UNUSED) == 7);
    REQUIRE(config.size(USER, ANY) == 3);
    REQUIRE(config.size(USER, USED) == 0);
    REQUIRE(config.size(USER, UNUSED) == 3);
    REQUIRE(config.size(INTERNAL, ANY) == 4);
    REQUIRE(config.size(INTERNAL, USED) == 0);
    REQUIRE(config.size(INTERNAL, UNUSED) == 4);

    config.get<int>("myval1");
    config.get<int>("myval2");
    config.get<int>("_internal1");
    config.get<int>("_internal2");
    config.get<int>("_internal3");

    REQUIRE(config.size(ALL, ANY) == 7);
    REQUIRE(config.size(ALL, USED) == 5);
    REQUIRE(config.size(ALL, UNUSED) == 2);
    REQUIRE(config.size(USER, ANY) == 3);
    REQUIRE(config.size(USER, USED) == 2);
    REQUIRE(config.size(USER, UNUSED) == 1);
    REQUIRE(config.size(INTERNAL, ANY) == 4);
    REQUIRE(config.size(INTERNAL, USED) == 3);
    REQUIRE(config.size(INTERNAL, UNUSED) == 1);
}

TEST_CASE("Get key-value pairs", "[core][core::config]") {
    using enum Configuration::Group;
    using enum Configuration::Usage;

    Configuration config {};

    config.set("myval1", 3.14);
    config.set("myval2", 1234);
    config.set("myval3", false);
    config.set("_internal1", true);
    config.set("_internal2", 0.739);
    config.set("_internal3", 6);

    // Test that value are kept
    REQUIRE(std::get<double>(config.getDictionary().at("myval1")) == 3.14);
    REQUIRE(std::get<bool>(config.getDictionary().at("_internal1")) == true);
    REQUIRE(std::get<std::int64_t>(config.getDictionary(USER).at("myval2")) == 1234);
    REQUIRE_FALSE(config.getDictionary(USER).contains("_internal2"));
    REQUIRE_FALSE(config.getDictionary(INTERNAL).contains("myval3"));
    REQUIRE(std::get<std::int64_t>(config.getDictionary(INTERNAL).at("_internal3")) == 6);

    config.get<double>("myval1");
    config.get<bool>("_internal1");

    // Test that used / unused keys are filtered properly
    REQUIRE(config.getDictionary(ALL, USED).size() == 2);
    REQUIRE(config.getDictionary(USER, UNUSED).size() == 2);
    REQUIRE(config.getDictionary(INTERNAL, USED).size() == 1);
}

TEST_CASE("Set Default Value", "[core][core::config]") {
    Configuration config {};

    // Check that a default does not overwrite existing values
    config.set("myval", true);
    config.setDefault("myval", false);
    REQUIRE(config.get<bool>("myval") == true);

    // Check that a default is set when the value does not exist
    config.setDefault("mydefault", false);
    REQUIRE(config.get<bool>("mydefault") == false);
}

TEST_CASE("Set & Use Aliases", "[core][core::config]") {
    Configuration config {};

    // Alias set before key exists
    config.setAlias("thisisnotset", "mykey");

    // Set key
    config.set("mykey", 99);

    // Set alias to key
    config.setAlias("thisisset", "mykey");

    // Check that the alias set before the key existed is not set:
    REQUIRE(config.has("thisisnotset") == false);

    // Check that the new key is accessible
    REQUIRE(config.get<std::size_t>("thisisset") == 99);

    // Set second key
    config.set("myotherkey", 77);
    // Attempt to set an alias for second key
    config.setAlias("mykey", "myotherkey");

    // Check that the alias would not overwrite another existing key:
    REQUIRE(config.get<std::size_t>("mykey") == 99);
}

TEST_CASE("Invalid Key Access", "[core][core::config]") {
    Configuration config {};

    // Check for invalid key to be detected
    REQUIRE_THROWS_AS(config.get<bool>("invalidkey"), MissingKeyError);
    REQUIRE_THROWS_MATCHES(config.get<bool>("invalidkey"), MissingKeyError, Message("Key 'invalidkey' does not exist"));

    // Check for invalid key to be detected when querying text representation
    REQUIRE_THROWS_AS(config.getText("invalidkey"), MissingKeyError);
    REQUIRE_THROWS_MATCHES(config.getText("invalidkey"), MissingKeyError, Message("Key 'invalidkey' does not exist"));

    // Check for invalid type conversion
    config.set("key", true);
    REQUIRE_THROWS_AS(config.get<double>("key"), InvalidTypeError);
    REQUIRE_THROWS_MATCHES(config.get<double>("key"),
                           InvalidTypeError,
                           Message("Could not convert value of type 'bool' to type 'double' for key 'key'"));

    // Check for invalid enum value conversion:
    enum MyEnum : std::uint8_t {
        ONE,
        TWO,
    };
    config.set("myenum", "THREE");
    REQUIRE_THROWS_AS(config.get<MyEnum>("myenum"), InvalidValueError);
    REQUIRE_THROWS_MATCHES(config.get<MyEnum>("myenum"),
                           InvalidValueError,
                           Message("Value THREE of key 'myenum' is not valid: possible values are ONE, TWO"));

    // Check for setting of invalid types
    REQUIRE_THROWS_AS(config.set("key", std::array<int, 5> {1, 2, 3, 4, 5}), InvalidTypeError);
}

TEST_CASE("Value Overflow", "[core][core::config]") {

    const std::size_t val = std::numeric_limits<std::size_t>::max();
    REQUIRE_THROWS_AS(Value::set(val), std::overflow_error);
    REQUIRE_THROWS_AS(Value::set(std::vector<std::size_t>({val})), std::overflow_error);

    Configuration config {};
    REQUIRE_THROWS_AS(config.set("size", val), InvalidValueError);
}

TEST_CASE("Update Configuration", "[core][core::config]") {
    Configuration config_base {};
    Configuration config_update {};

    config_base.set("bool", true);
    config_base.set("int64", std::int64_t(63));
    config_base.set("string", std::string("unchanged"));

    config_update.set("bool", false, false);                      // exists but not used
    config_update.set("uint64", std::uint64_t(64), true);         // exists and used
    config_update.set("string2", std::string("new_value"), true); // new and used

    // Update configuration
    config_base.update(config_update);

    // Check that used keys from config_update were updated in config_base
    REQUIRE(config_base.get<std::uint64_t>("uint64") == 64);
    REQUIRE(config_base.get<std::string>("string2") == "new_value");

    // Check that unused key from config_update were not updated in config_base
    REQUIRE(config_base.get<bool>("bool") == true);
}

TEST_CASE("Move Configuration", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);

    const Configuration config_move = std::move(config);
    REQUIRE(config_move.get<bool>("bool") == true);
}

TEST_CASE("Pack & Unpack List to MsgPack", "[core][core::config]") {
    // Create dictionary
    List list {};
    auto tp = std::chrono::system_clock::now();
    list.push_back(true);
    list.push_back(std::int64_t(63));
    list.push_back(double(1.3));
    list.push_back(std::string("a"));
    list.push_back(tp);
    list.push_back(std::vector<bool>({true, false, true}));
    list.push_back(std::vector<std::int64_t>({63, 62, 61}));
    list.push_back(std::vector<double>({1.3, 3.1}));
    list.push_back(std::vector<std::string>({"a", "b", "c"}));
    list.push_back(std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));

    // Empty vector
    list.push_back(std::vector<double> {});

    // MsgPack NIL
    list.push_back(std::monostate {});

    // Pack to MsgPack
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, list);

    // Unpack from MsgPack
    auto unpacked = msgpack::unpack(sbuf.data(), sbuf.size());
    auto list_unpacked = unpacked->as<List>();

    REQUIRE(list_unpacked.at(0).get<bool>() == true);
    REQUIRE(list_unpacked.at(1).get<std::int64_t>() == std::int64_t(63));
    REQUIRE(list_unpacked.at(2).get<double>() == double(1.3));
    REQUIRE(list_unpacked.at(3).get<std::string>() == std::string("a"));
    REQUIRE(list_unpacked.at(4).get<std::chrono::system_clock::time_point>() == tp);
    REQUIRE(list_unpacked.at(5).get<std::vector<bool>>() == std::vector<bool>({true, false, true}));
    REQUIRE(list_unpacked.at(6).get<std::vector<std::int64_t>>() == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(list_unpacked.at(7).get<std::vector<double>>() == std::vector<double>({1.3, 3.1}));
    REQUIRE(list_unpacked.at(8).get<std::vector<std::string>>() == std::vector<std::string>({"a", "b", "c"}));
    REQUIRE(list_unpacked.at(9).get<std::vector<std::chrono::system_clock::time_point>>() ==
            std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));
    REQUIRE(list_unpacked.at(10).get<std::vector<double>>().empty());
    REQUIRE(list_unpacked.at(11).get<std::monostate>() == std::monostate());
}

TEST_CASE("Pack & Unpack Dictionary to MsgPack", "[core][core::config]") {
    // Create dictionary
    Dictionary dict {};
    auto tp = std::chrono::system_clock::now();
    dict["bool"] = true;
    dict["int64"] = std::int64_t(63);
    dict["double"] = double(1.3);
    dict["string"] = std::string("a");
    dict["time"] = tp;

    dict["array_bool"] = std::vector<bool>({true, false, true});
    dict["array_int64"] = std::vector<std::int64_t>({63, 62, 61});
    dict["array_double"] = std::vector<double>({1.3, 3.1});
    dict["array_binary"] = std::vector<char>({0x1, 0x2, 0x3});
    dict["array_string"] = std::vector<std::string>({"a", "b", "c"});
    dict["array_time"] = std::vector<std::chrono::system_clock::time_point>({tp, tp, tp});

    // Pack to MsgPack
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, dict);

    // Unpack from MsgPack
    auto unpacked = msgpack::unpack(sbuf.data(), sbuf.size());
    auto dict_unpacked = unpacked->as<Dictionary>();

    REQUIRE(dict_unpacked["bool"].get<bool>() == true);
    REQUIRE(dict_unpacked["int64"].get<std::int64_t>() == std::int64_t(63));
    REQUIRE(dict_unpacked["double"].get<double>() == double(1.3));
    REQUIRE(dict_unpacked["string"].get<std::string>() == std::string("a"));
    REQUIRE(dict_unpacked["time"].get<std::chrono::system_clock::time_point>() == tp);
    REQUIRE(dict_unpacked["array_bool"].get<std::vector<bool>>() == std::vector<bool>({true, false, true}));
    REQUIRE(dict_unpacked["array_int64"].get<std::vector<std::int64_t>>() == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(dict_unpacked["array_double"].get<std::vector<double>>() == std::vector<double>({1.3, 3.1}));
    REQUIRE(dict_unpacked["array_binary"].get<std::vector<char>>() == std::vector<char>({0x1, 0x2, 0x3}));
    REQUIRE(dict_unpacked["array_string"].get<std::vector<std::string>>() == std::vector<std::string>({"a", "b", "c"}));
    REQUIRE(dict_unpacked["array_time"].get<std::vector<std::chrono::system_clock::time_point>>() ==
            std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));
}

TEST_CASE("Generate Configurations from Dictionary", "[core][core::config]") {
    // Create dictionary
    Dictionary dict {};
    dict["key"] = 3.12;
    dict["array"] = std::vector<std::string>({"one", "two", "three"});

    const Configuration config {dict};

    REQUIRE(config.get<double>("key") == 3.12);
    REQUIRE(config.getArray<std::string>("array") == std::vector<std::string>({"one", "two", "three"}));
}

TEST_CASE("Assemble ZMQ Message from Configuration", "[core][core::config]") {
    Configuration config {};
    config.set("bool", true);
    config.set("int64", std::int64_t(63));
    config.set("size", std::size_t(1));

    // Mark one key as used:
    REQUIRE(config.get<std::int64_t>("int64") == std::int64_t(63));

    // Assemble & disassemble with all used keys:
    const auto config_zmq = config.getDictionary(Configuration::Group::ALL, Configuration::Usage::USED).assemble();
    const auto config_unpacked = Configuration(Dictionary::disassemble(config_zmq));
    REQUIRE(config_unpacked.size() == 1);
    REQUIRE(config_unpacked.get<std::int64_t>("int64") == 63);
}

// NOLINTEND(google-readability-casting,readability-redundant-casting)
// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
