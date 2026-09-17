#include "youtube/JsonParser.h"
#include <iostream>
#include <cassert>

void runJsonTests() {
    std::cout << "\n[TEST] Starting JSON Parser Unit Tests..." << std::endl;

    // Test 1: Simple Object & Types
    std::string json1 = R"({
        "title": "Embedded Linux",
        "views": 42000,
        "is_live": false,
        "rating": 4.95,
        "tags": ["c++", "linux", "embedded"]
    })";

    yt::JsonValue val;
    bool ok = yt::JsonParser::parse(json1, val);
    (void)ok;
    assert(ok && "Parsing json1 must succeed");
    assert(val.isObject() && "Root must be object");
    assert(val.getString("title") == "Embedded Linux");
    assert(val.getInt("views") == 42000);
    assert(val.getBool("is_live") == false);
    assert(val.getDouble("rating") > 4.9);

    const auto& tags = val["tags"].asArray();
    (void)tags;
    assert(tags.size() == 3);
    assert(tags[0].asString() == "c++");
    assert(tags[1].asString() == "linux");
    assert(tags[2].asString() == "embedded");

    // Test 2: Nested structures
    std::string json2 = R"({
        "video": {
            "id": "abc123",
            "metadata": {
                "author": "SysAdmin"
            }
        }
    })";

    yt::JsonValue val2;
    bool ok2 = yt::JsonParser::parse(json2, val2);
    (void)ok2;
    assert(ok2 && val2["video"]["metadata"].getString("author") == "SysAdmin");

    // Test 4: Unicode escape sequences (\uXXXX)
    std::string jsonUnicode = R"({
        "turkish": "T\u00fcrk\u00e7e",
        "symbol": "\u2022"
    })";
    yt::JsonValue uVal;
    bool uOk = yt::JsonParser::parse(jsonUnicode, uVal);
    (void)uOk;
    assert(uOk && "Unicode JSON must parse cleanly");
    assert(uVal.getString("turkish") == "Türkçe" && "Must properly decode UTF-8 unicode escapes");

    std::cout << "  -> JSON Parser Tests (including Unicode decoding) Passed!" << std::endl;
}
