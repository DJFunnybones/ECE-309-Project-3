// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}
// ------------------------------------------------------------- your tests

namespace {

// Counts how many instances are alive, so tests can prove the registry
// destroys every tool exactly once (no leak, no double delete).
class CountedTool : public NamedTool {
public:
    CountedTool(std::string n, int* alive) : NamedTool(std::move(n)), alive_(alive) { ++*alive_; }
    ~CountedTool() override { --*alive_; }
private:
    int* alive_;
};

// Resolve helper: returns the result and fills out/reason.
bool resolve_in(const Workspace& ws, const std::string& p, fs::path* out, std::string* why) {
    return ws.resolve(p, out, why);
}

}  // namespace

// ---------------------------------------------------------------- registry

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.find("anything") == nullptr);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    int visited = 0;
    r.for_each([&](const Tool&) { ++visited; });
    CHECK(visited == 0);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}

TEST(reg_size_tracks_adds_and_rejects) {
    ToolRegistry r;
    CHECK(r.add(std::make_unique<NamedTool>("a")));
    CHECK(r.size() == 1);
    CHECK(!r.add(std::make_unique<NamedTool>("a")));    // duplicate
    CHECK(r.size() == 1);
    CHECK(!r.add(nullptr));                              // null
    CHECK(r.size() == 1);
    CHECK(r.add(std::make_unique<NamedTool>("b")));
    CHECK(r.size() == 2);
    CHECK(r.find("a") != nullptr && r.find("b") != nullptr);
    CHECK(r.find("c") == nullptr);
}

TEST(reg_find_is_case_and_prefix_sensitive) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("read_file")));
    CHECK(r.find("Read_File") == nullptr);
    CHECK(r.find("read") == nullptr);
    CHECK(r.find("read_file2") == nullptr);
    CHECK(r.find("") == nullptr);
    CHECK(r.find("read_file") != nullptr);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    const std::vector<std::string> names = {"read_file", "write_file", "list_dir",
                                            "calculator", "teleport"};
    for (const auto& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    std::vector<std::string> seen;
    r.for_each([&](const Tool& t) { seen.push_back(t.name()); });
    CHECK(seen.size() == names.size());
    for (const auto& n : names) {
        int count = 0;
        for (const auto& s : seen) if (s == n) ++count;
        CHECK(count == 1);
    }
}

TEST(reg_for_each_after_resize) {
    ToolRegistry r;
    for (int i = 0; i < 200; ++i) REQUIRE(r.add(std::make_unique<NamedTool>("t" + std::to_string(i))));
    int visited = 0;
    r.for_each([&](const Tool&) { ++visited; });
    CHECK(visited == 200);
}

TEST(reg_owns_and_destroys_tools_once) {
    int alive = 0;
    {
        ToolRegistry r;
        for (int i = 0; i < 100; ++i)
            REQUIRE(r.add(std::make_unique<CountedTool>("c" + std::to_string(i), &alive)));
        CHECK(alive == 100);            // resizes happened; nothing was lost or copied
    }
    CHECK(alive == 0);                  // destructor freed every tool exactly once
}

TEST(reg_duplicate_destroys_new_tool) {
    int alive = 0;
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<CountedTool>("dup", &alive)));
    CHECK(alive == 1);
    CHECK(!r.add(std::make_unique<CountedTool>("dup", &alive)));
    CHECK(alive == 1);                  // the rejected newcomer was destroyed
}

TEST(reg_hash_is_order_sensitive_and_full_width) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("abc") != ToolRegistry::hash("abd"));   // last char matters
    CHECK(ToolRegistry::hash("abc") != ToolRegistry::hash("xbc"));   // first char matters
    CHECK(ToolRegistry::hash("") == ToolRegistry::hash(""));         // deterministic
}

TEST(reg_pointer_stable_across_resizes) {
    ToolRegistry r;
    std::vector<Tool*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        auto t = std::make_unique<NamedTool>("p" + std::to_string(i));
        ptrs.push_back(t.get());
        REQUIRE(r.add(std::move(t)));
    }
    for (int i = 0; i < 1000; ++i) CHECK(r.find("p" + std::to_string(i)) == ptrs[i]);
}

// REQUIRED (spec §6 item 3): the scale test.
TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;
    std::set<std::size_t> distinct_hashes;
    bool load_ok = true;
    for (int i = 0; i < 10000; ++i) {
        std::string name = "tool_" + std::to_string(i);
        distinct_hashes.insert(ToolRegistry::hash(name));
        auto t = std::make_unique<NamedTool>(name);
        Tool* raw = t.get();
        REQUIRE(r.add(std::move(t)));
        if (i == 0) first = raw;
        if (r.load_factor() > 1.0) load_ok = false;
    }
    CHECK(load_ok);                                   // load factor <= 1.0 after every add
    CHECK(r.size() == 10000);
    bool all_found = true;
    for (int i = 0; i < 10000; ++i)
        if (r.find("tool_" + std::to_string(i)) == nullptr) all_found = false;
    CHECK(all_found);
    CHECK(r.find("tool_0") == first);                 // pointer stable from first to last insert
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.max_probe_length() <= 64);
    CHECK(distinct_hashes.size() >= 9990);

    std::printf("    [measure] FNV-1a + chaining, 10000 tools: buckets=%zu load_factor=%.3f "
                "max_probe_length=%zu distinct_hashes=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), distinct_hashes.size());
}

// REQUIRED (spec §6 item 4): what a weak hash WOULD do at the same table size.
TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    const std::size_t buckets = r.bucket_count();

    std::vector<int> weak(buckets, 0), djb2(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = "tool_" + std::to_string(i);
        std::size_t weak_h = static_cast<unsigned char>(s[0]) + s.size();     // s[0] + s.size()
        ++weak[weak_h % buckets];
        std::uint64_t d = 5381;                                               // djb2
        for (unsigned char c : s) d = d * 33 + c;
        ++djb2[static_cast<std::size_t>(d) % buckets];
    }
    int weak_max = *std::max_element(weak.begin(), weak.end());
    int djb2_max = *std::max_element(djb2.begin(), djb2.end());

    CHECK(weak_max > 1000);                                   // the weak hash is terrible
    CHECK(r.max_probe_length() < static_cast<std::size_t>(weak_max));
    std::printf("    [measure] longest chain at %zu buckets: FNV-1a=%zu  djb2=%d  s[0]+size=%d\n",
                buckets, r.max_probe_length(), djb2_max, weak_max);
}

// --------------------------------------------------------------- workspace

TEST(ws_accepts_normal_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, "notes/todo.txt", &out, &why));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(resolve_in(ws, "new/dir/file.txt", &out, &why));   // need not exist
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
}

TEST(ws_normalizes_dot_and_double_slash) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, "./notes//todo.txt", &out, &why));
    CHECK(out == ws.root() / "notes" / "todo.txt");
}

TEST(ws_dot_is_root) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, ".", &out, &why));
    CHECK(out == ws.root());
}

TEST(ws_dotdot_that_stays_inside_is_ok) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, "a/../b", &out, &why));
    CHECK(out == ws.root() / "b");
    REQUIRE(resolve_in(ws, "a/..", &out, &why));
    CHECK(out == ws.root());
}

TEST(ws_allows_double_dot_inside_a_name) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, "my..notes.txt", &out, &why));      // not a ".." component
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_rejects_empty) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!resolve_in(ws, "", &out, &why));
    CHECK(!why.empty());
}

TEST(ws_rejects_embedded_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    const std::string p = std::string("notes") + '\0' + "/x";   // literal would be cut at the NUL
    CHECK(!resolve_in(ws, p, &out, &why));
    CHECK(!why.empty());
}

TEST(ws_rejects_absolute_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    CHECK(!resolve_in(ws, "/etc/passwd", &out, &why));
    CHECK(!why.empty());
    // Even an absolute path that points INSIDE the root is refused.
    why.clear();
    CHECK(!resolve_in(ws, (ws.root() / "notes" / "todo.txt").string(), &out, &why));
    CHECK(!why.empty());
}

TEST(ws_rejects_escapes) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    for (const char* p : {"..", "../etc/passwd", "a/../../b", "../../..", "./../x"}) {
        std::string why;
        CHECK(!resolve_in(ws, p, &out, &why));
        CHECK(!why.empty());
    }
}

TEST(ws_rejects_reentry_through_parent) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    // Climbs out of the root and walks back in: still refused.
    const std::string p = "notes/../../" + ws.root().filename().string() + "/notes/todo.txt";
    CHECK(!resolve_in(ws, p, &out, &why));
    CHECK(!why.empty());
}

TEST(ws_resolve_does_not_touch_filesystem) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string why;
    REQUIRE(resolve_in(ws, "made/up/path.txt", &out, &why));
    CHECK(!fs::exists(ws.root() / "made"));                     // nothing was created
    CHECK(fs::is_empty(ws.root()));
}

// -------------------------------------------------------------- calculator

namespace {
void expect_value(const std::string& expr, double want) {
    double v = 0;
    std::string err;
    CHECK(eval(expr, &v, &err));
    CHECK(v == want);
}
}  // namespace

TEST(calc_spec_table) {
    expect_value("2 + 3 * 4", 14);
    expect_value("10 - 4 - 3", 3);
    expect_value("100 / 10 / 5", 2);
    expect_value("(2 + 3) * 4", 20);
    expect_value("2 * (3 + 4) - 5", 9);
    expect_value("-3 * -3", 9);
    expect_value("7 / 2", 3.5);
}

TEST(calc_left_to_right_subtraction_and_division) {
    expect_value("10 - 4 - 3", 3);          // (10-4)-3, not 10-(4-3)
    expect_value("20 / 4 / 5", 1);          // (20/4)/5, not 20/(4/5)
    expect_value("10 - 4 + 3", 9);
    expect_value("8 / 2 * 3", 12);          // equal precedence goes left to right
}

TEST(calc_operand_order) {
    expect_value("10 - 4", 6);              // right operand popped first
    expect_value("9 / 3", 3);
}

TEST(calc_parentheses_and_nesting) {
    expect_value("((2))", 2);
    expect_value("(1 + 2) * (3 + 4)", 21);
    expect_value("2 * (3 + (4 - 1)) * 2", 24);
    expect_value("((((5))))", 5);
}

TEST(calc_negatives_and_decimals) {
    expect_value("-3 + 5", 2);
    expect_value("2 * -3", -6);
    expect_value("(-1) * 4", -4);
    expect_value("0.5 + 0.25", 0.75);
    expect_value("1.5 * 4", 6);
}

TEST(calc_division_by_zero_in_subexpression) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
    err.clear();
    CHECK(!eval("5 + 3 / 0", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_very_long_expression) {
    std::string e = "1";
    for (int i = 0; i < 20000; ++i) e += "+1";
    double v = 0;
    std::string err;
    REQUIRE(eval(e, &v, &err));
    CHECK(v == 20001);
}

TEST(calc_deeply_nested_parentheses) {
    std::string e(5000, '(');
    e += "1";
    e += std::string(5000, ')');
    double v = 0;
    std::string err;
    REQUIRE(eval(e, &v, &err));
    CHECK(v == 1);
}

TEST(calc_never_crashes_on_unexpected_tokens) {
    // calc_evaluate assumes well-formed input, but must fail gracefully if not.
    double v = 0;
    std::string err;
    std::vector<CalcToken> toks;
    CHECK(!calc_evaluate(toks, &v, &err));                       // empty list
    CalcToken plus;  plus.kind = CalcToken::Kind::Plus;
    toks = {plus};
    CHECK(!calc_evaluate(toks, &v, &err));                       // lone operator
    CalcToken lp;    lp.kind = CalcToken::Kind::LParen;
    toks = {lp};
    CHECK(!calc_evaluate(toks, &v, &err));                       // unbalanced
    CHECK(!err.empty());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
