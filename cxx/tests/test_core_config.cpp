/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Set & Get Values", "[core][core::config]") {
    Configuration config;

    config.set("bool", true);

    config.set("int64", std::int64_t(63));
    config.set("size", std::size_t(1));
    config.set("uint64", std::uint64_t(64));
    config.set("uint8", std::uint8_t(8));

    config.set("double", double(1.3));
    config.set("float", float(3.14));

    config.set("string", std::string("a"));

    enum MyEnum {
        ONE,
        TWO,
    };
    config.set("myenum", MyEnum::ONE);

    auto tp = std::chrono::system_clock::now();
    config.set("time", tp);

    // Check that keys are unused
    REQUIRE(config.size() == config.getUnusedKeys().size());

    // Read values back
    REQUIRE(config.get<bool>("bool") == true);

    REQUIRE(config.get<std::int64_t>("int64") == 63);
    REQUIRE(config.get<std::size_t>("size") == 1);
    REQUIRE(config.get<std::uint64_t>("uint64") == 64);
    REQUIRE(config.get<std::uint8_t>("uint8") == 8);

    REQUIRE(config.get<double>("double") == 1.3);
    REQUIRE(config.get<float>("float") == 3.14F);

    REQUIRE(config.get<std::string>("string") == "a");

    REQUIRE(config.get<MyEnum>("myenum") == MyEnum::ONE);

    REQUIRE(config.get<std::chrono::system_clock::time_point>("time") == tp);

    // Check that all keys have been marked as used
    REQUIRE(config.getUnusedKeys().empty());
}

TEST_CASE("Set & Get Array Values", "[core][core::config]") {
    Configuration config;

    config.setArray<bool>("bool", {true, false, true});

    config.setArray<std::int64_t>("int64", {63, 62, 61});
    config.setArray<std::size_t>("size", {1, 2, 3});
    config.setArray<std::uint64_t>("uint64", {64, 65, 66});
    config.setArray<std::uint8_t>("uint8", {8, 7, 6});

    config.setArray<double>("double", {1.3, 3.1});
    config.setArray<float>("float", {3.14F, 1.43F});

    config.setArray<std::string>("string", {"a", "b", "c"});

    enum MyEnum {
        ONE,
        TWO,
    };
    config.setArray<MyEnum>("myenum", {MyEnum::ONE, MyEnum::TWO});

    auto tp = std::chrono::system_clock::now();
    config.setArray<std::chrono::system_clock::time_point>("time", {tp, tp, tp});

    // Read values back
    REQUIRE(config.getArray<bool>("bool") == std::vector<bool>({true, false, true}));

    REQUIRE(config.getArray<std::int64_t>("int64") == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(config.getArray<size_t>("size") == std::vector<size_t>({1, 2, 3}));
    REQUIRE(config.getArray<std::uint64_t>("uint64") == std::vector<std::uint64_t>({64, 65, 66}));
    REQUIRE(config.getArray<std::uint8_t>("uint8") == std::vector<std::uint8_t>({8, 7, 6}));

    REQUIRE(config.getArray<double>("double") == std::vector<double>({1.3, 3.1}));
    REQUIRE(config.getArray<float>("float") == std::vector<float>({3.14F, 1.43F}));

    REQUIRE(config.getArray<std::string>("string") == std::vector<std::string>({"a", "b", "c"}));

    REQUIRE(config.getArray<std::chrono::system_clock::time_point>("time") ==
            std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));
}

TEST_CASE("Set & Get Path Values", "[core][core::config]") {
    Configuration config;

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
    config.set("float", float(3.14));
    config.set("string", std::string("a"));

    enum MyEnum {
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
    REQUIRE(config.getText("float") == "3.14");
    REQUIRE(config.getText("string") == "a");
    REQUIRE(config.getText("myenum") == "ONE");
    REQUIRE(config.getText("time") == "1970-01-01 00:00:00.000000000");

    // Get text with default for existing key:
    REQUIRE(config.getText("bool", "false") == "true");
    // Get text with default for non-existent key:
    REQUIRE(config.getText("foo", "false") == "false");
}

TEST_CASE("Access Arrays as Text", "[core][core::config]") {
    Configuration config {};

    config.setArray<bool>("bool", {true, false, true});

    config.setArray<std::int64_t>("int64", {63, 62, 61});
    config.setArray<std::size_t>("size", {1, 2, 3});
    config.setArray<std::uint64_t>("uint64", {64, 65, 66});
    config.setArray<std::uint8_t>("uint8", {8, 7, 6});

    config.setArray<double>("double", {1.3, 3.1});
    config.setArray<float>("float", {3.14F, 1.43F});

    config.setArray<std::string>("string", {"a", "b", "c"});

    enum MyEnum {
        ONE,
        TWO,
    };
    config.setArray<MyEnum>("myenum", {MyEnum::ONE, MyEnum::TWO});

    const std::chrono::system_clock::time_point tp {};
    config.setArray<std::chrono::system_clock::time_point>("time", {tp, tp, tp});

    REQUIRE(config.getText("bool") == "[true,false,true,]");
    REQUIRE(config.getText("int64") == "[63,62,61,]");
    REQUIRE(config.getText("size") == "[1,2,3,]");
    REQUIRE(config.getText("uint64") == "[64,65,66,]");
    REQUIRE(config.getText("uint8") == "[8,7,6,]");
    REQUIRE(config.getText("double") == "[1.3,3.1,]");
    REQUIRE(config.getText("float") == "[3.14,1.43,]");
    REQUIRE(config.getText("string") == "[a,b,c,]");
    REQUIRE(config.getText("time") ==
            "[1970-01-01 00:00:00.000000000,1970-01-01 00:00:00.000000000,1970-01-01 00:00:00.000000000,]");
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
    REQUIRE(config.getUnusedKeys().empty());
    REQUIRE(config.get<double>("myval") == 3.14);
}

TEST_CASE("Get all Values", "[core][core::config]") {
    Configuration config {};

    config.set("myval", 3.14);
    config.set("_internal", 1);

    auto keys = config.getAll();

    // Check that we have "myval"
    REQUIRE(std::get<double>(keys.at("myval")) == 3.14);

    // Check that only one key was returned and the internal withheld:
    REQUIRE(keys.size() == 1);
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
    Configuration config;

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
    enum MyEnum {
        ONE,
        TWO,
    };
    config.set("myenum", "THREE");
    REQUIRE_THROWS_AS(config.get<MyEnum>("myenum"), InvalidValueError);
    REQUIRE_THROWS_MATCHES(config.get<MyEnum>("myenum"),
                           InvalidValueError,
                           Message("Value THREE of key 'myenum' is not valid: possible values are ONE, TWO"));
}

TEST_CASE("Merge Configurations", "[core][core::config]") {
    Configuration config_a {};
    Configuration config_b {};

    config_a.set("bool", true);
    config_a.set("int64", std::int64_t(63));

    config_b.set("bool", false);
    config_b.set("uint64", std::uint64_t(64));

    // Merge configurations
    config_a.merge(config_b);

    // Check that keys from config_b have been transferred:
    REQUIRE(config_a.get<std::uint64_t>("uint64") == 64);

    // Check that existing keys in config_a have been overwritten
    REQUIRE(config_a.get<bool>("bool") == false);
}

TEST_CASE("Copy & Move Configurations", "[core][core::config]") {
    Configuration config {};

    config.set("bool", true);

    const Configuration config_copy = config;
    REQUIRE(config_copy.get<bool>("bool") == true);

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

    // Pack to MsgPack
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, list);

    // Unpack from MsgPack
    auto unpacked = msgpack::unpack(sbuf.data(), sbuf.size());
    auto list_unpacked = unpacked->as<List>();

    REQUIRE(std::get<bool>(list_unpacked.at(0)) == true);
    REQUIRE(std::get<std::int64_t>(list_unpacked.at(1)) == std::int64_t(63));
    REQUIRE(std::get<double>(list_unpacked.at(2)) == double(1.3));
    REQUIRE(std::get<std::string>(list_unpacked.at(3)) == std::string("a"));
    REQUIRE(std::get<std::chrono::system_clock::time_point>(list_unpacked.at(4)) == tp);
    REQUIRE(std::get<std::vector<bool>>(list_unpacked.at(5)) == std::vector<bool>({true, false, true}));
    REQUIRE(std::get<std::vector<std::int64_t>>(list_unpacked.at(6)) == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(std::get<std::vector<double>>(list_unpacked.at(7)) == std::vector<double>({1.3, 3.1}));
    REQUIRE(std::get<std::vector<std::string>>(list_unpacked.at(8)) == std::vector<std::string>({"a", "b", "c"}));
    REQUIRE(std::get<std::vector<std::chrono::system_clock::time_point>>(list_unpacked.at(9)) ==
            std::vector<std::chrono::system_clock::time_point>({tp, tp, tp}));
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
    dict["array_string"] = std::vector<std::string>({"a", "b", "c"});
    dict["array_time"] = std::vector<std::chrono::system_clock::time_point>({tp, tp, tp});

    // Pack to MsgPack
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, dict);

    // Unpack from MsgPack
    auto unpacked = msgpack::unpack(sbuf.data(), sbuf.size());
    auto dict_unpacked = unpacked->as<Dictionary>();

    REQUIRE(std::get<bool>(dict_unpacked["bool"]) == true);
    REQUIRE(std::get<std::int64_t>(dict_unpacked["int64"]) == std::int64_t(63));
    REQUIRE(std::get<double>(dict_unpacked["double"]) == double(1.3));
    REQUIRE(std::get<std::string>(dict_unpacked["string"]) == std::string("a"));
    REQUIRE(std::get<std::chrono::system_clock::time_point>(dict_unpacked["time"]) == tp);
    REQUIRE(std::get<std::vector<bool>>(dict_unpacked["array_bool"]) == std::vector<bool>({true, false, true}));
    REQUIRE(std::get<std::vector<std::int64_t>>(dict_unpacked["array_int64"]) == std::vector<std::int64_t>({63, 62, 61}));
    REQUIRE(std::get<std::vector<double>>(dict_unpacked["array_double"]) == std::vector<double>({1.3, 3.1}));
    REQUIRE(std::get<std::vector<std::string>>(dict_unpacked["array_string"]) == std::vector<std::string>({"a", "b", "c"}));
    REQUIRE(std::get<std::vector<std::chrono::system_clock::time_point>>(dict_unpacked["array_time"]) ==
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

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
