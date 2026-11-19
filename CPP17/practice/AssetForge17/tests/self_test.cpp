#include "assetforge/report.hpp"
#include "../src/detail.hpp"
#include <algorithm>
#include <future>
#include <iostream>
#include <random>
#include <type_traits>

const void* crc_table_address();

namespace {
using namespace assetforge;
#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__))                                                                        \
            throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #__VA_ARGS__);      \
    } while (false)

template <class F>
void rejects(F action, std::string_view message) {
    try {
        action();
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(message) != std::string_view::npos) {
            return;
        }
        throw;
    }
    throw std::runtime_error("expected error: " + std::string(message));
}

class Fixture {
public:
    Fixture() {
        std::random_device random;
        for (int i = 0; i < 32; ++i) {
            root = fs::temp_directory_path() /
                   ("assetforge-test-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (fs::create_directory(root)) {
                return;
            }
        }
        throw std::runtime_error("cannot reserve test directory");
    }

    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }

    fs::path write(const fs::path& name, std::string_view content) const {
        auto file = root / name;
        fs::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        if (!content.empty()) {
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }
        out.close();
        return file;
    }

    fs::path write(const fs::path& name, const Bytes& bytes) const {
        return write(name,
                     std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    fs::path root;
};

std::vector<std::string> ids(const BuildPlan& plan) {
    std::vector<std::string> result;
    for (const auto& item : plan.assets()) {
        result.push_back(item.spec.id);
    }
    return result;
}

constexpr std::string_view base = "asset\ta\traw\tfile.bin\t-\n"
                                  "asset\tb\ttext-lf\tspace in file.txt\t0\n"
                                  "requires\tb\ta\n"
                                  "profile\tp\tb\n"
                                  "alias\tstart\tb\n";

void manifest_tests() {
    Fixture fixture;
    auto file = fixture.write("source.assets", "\xef\xbb\xbf# comment\r\nalias\tstart\tc\r\n"
                                               "profile\tp\tb,c\r\n"
                                               "requires\tc\ta\r\n"
                                               "requires\tb\ta\r\n"
                                               "asset\tc\traw\tc.bin\t-\r\n"
                                               "asset\tb\ttext-lf\tb.txt\t0\n"
                                               "\nasset\ta\traw\ta.bin\t-");
    auto catalog = load_catalog(file);
    auto package = plan(catalog, Profile{"p"});
    CHECK(ids(package) == std::vector<std::string>{"a", "b", "c"});
    CHECK(ids(plan(catalog, Root{"start"})) == std::vector<std::string>{"a", "c"});
    CHECK(!catalog.find("absent") && catalog.find("c") == 0);
    CHECK(package.assets()[1].dependencies == std::vector<Index>{0});
    CHECK(package.assets()[1].spec.limit == 0 && !package.assets()[0].spec.limit);
    CHECK(package.aliases().at("start") == 2);
    fs::remove(file);
    CHECK(package.assets()[1].spec.path == "b.txt");
    rejects([&] { (void)plan(catalog, Profile{"absent"}); }, "unknown profile");
    rejects([&] { (void)plan(catalog, Root{"absent"}); }, "unknown asset or alias");

    auto malformed = [&](std::string_view declaration, std::string_view message) {
        auto invalid =
            fixture.write("invalid.assets", std::string(base) + std::string(declaration));
        rejects([&] { (void)load_catalog(invalid); }, message);
    };
    malformed("asset\ta\traw\tz\t-\n", "line 6: duplicate asset");
    malformed("asset\tc\tgarbage\t../up\t2oops", "line 6: unknown asset mode");
    malformed("asset\tc\traw\t../up\t0", "invalid resource path component");
    malformed("asset\tc\traw\tC:\\rooted\t0", "relative resource path");
    malformed("asset\tc\traw\tpath\t2x", "invalid byte limit");
    malformed("asset\tc\traw\tpath\t18446744073709551616", "invalid byte limit");
    malformed("asset\tc\traw\tpath\t", "empty byte limit");
    malformed("asset\tc\traw\tbad\xed\xa0\x80\t-", "invalid UTF-8");
    malformed("alias\ta\tb", "duplicate name");
    malformed("alias\tother\tstart", "unknown asset: start");
    malformed("requires\ta\tmissing", "unknown asset: missing");
    malformed("requires\tb\ta", "duplicate dependency");
    malformed("requires\ta\tb", "dependency cycle:");
    malformed("asset\tlonely\traw\tx\t-\nrequires\tlonely\tlonely", "lonely -> lonely");
    malformed("profile\tq\ta,", "invalid profile root");
    malformed("profile\tq\ta,a", "duplicate profile root");
    malformed("profile\tp\ta", "duplicate profile");
}

Bytes unhex(std::string_view text) {
    auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    Bytes result;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        result.push_back(static_cast<std::byte>(16 * nibble(text[i]) + nibble(text[i + 1])));
    }
    return result;
}

void set_word(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.at(offset + i) = static_cast<std::byte>((value >> (8 * i)) & 255);
    }
}

void bundle_tests() {
    static_assert(!std::is_copy_constructible_v<BundleReader> &&
                  !std::is_move_constructible_v<BundleReader>);
    CHECK(crc_table_address() == &detail::crc_table);
    Fixture fixture;
    auto manifest = fixture.write("minimal.assets", "asset\tx\traw\tdata.bin\t-\n");
    fixture.write("data.bin", "123456789");
    auto package = fixture.root / "generated.afp";
    const auto job = plan(load_catalog(manifest), Root{"x"});

    // 32-byte header, one 27-byte index row, then 9 bytes of content.
    const auto golden = unhex("414650310100000001000000000000001b000000452fb6c40900000000000000"
                              "010078000000000000000009000000000000002639f4cb00000000"
                              "313233343536373839");
    CHECK(std::holds_alternative<BuildSummary>(build(job, package)));
    CHECK(detail::read_file(package, golden.size()) == golden);
    CHECK(std::get<BuildError>(build(job, package)).message.find("already exists") !=
          std::string::npos);
    CHECK(detail::read_file(package, golden.size()) == golden);

    const auto reference = fixture.write("reference.afp", golden);
    {
        auto reader = BundleReader::open(reference);
        CHECK(reader.find("x") == 0 && !reader.find("unknown"));
        CHECK(reader.entries()[0].offset == 0 && reader.entries()[0].size == 9);
        CHECK(reader.entries()[0].crc == 0xcbf43926u);
        CHECK(!reader.read("unknown"));
        CHECK(reader.read("x") == unhex("313233343536373839"));
        CHECK(reader.verify(Policy::seq).empty() && reader.verify(Policy::par).empty());
    }
    auto corrupt = [&](Bytes altered, std::string_view reason) {
        auto file = fixture.write("bad.afp", altered);
        rejects([&] { (void)BundleReader::open(file); }, reason);
    };
    auto altered = golden;
    altered[32] ^= std::byte{1};
    corrupt(altered, "index CRC");
    altered = golden;
    altered[35] = std::byte{1}; // change first resource offset and refresh index CRC
    set_word(altered, 20, detail::crc32({altered.data() + 32, 27}));
    corrupt(altered, "entry bounds");
    for (std::size_t size = 1; size < golden.size(); ++size) {
        auto file = fixture.write("truncated.afp", Bytes(golden.begin(), golden.begin() + size));
        rejects([&] { (void)BundleReader::open(file); },
                size < header_size ? "truncated header" : "file size");
    }
    altered = golden;
    altered.back() ^= std::byte{1};
    {
        auto reader = BundleReader::open(fixture.write("bad.afp", altered));
        CHECK(reader.verify(Policy::par) == std::vector<std::string>{"x"});
        rejects([&] { (void)reader.read("x"); }, "payload CRC");
    }

    auto input = fixture.write("with-empty.assets", base);
    fixture.write("file.bin", std::string_view("\0\x7f\xff", 3));
    fixture.write("space in file.txt", "z\r\n");
    auto plan_with_empty = plan(load_catalog(input), Profile{"p"});
    auto failed = fixture.root / "failure.afp";
    auto result = build(plan_with_empty, failed);
    CHECK(std::get<BuildError>(result).message.find(
              "b (space in file.txt): processed byte limit") != std::string::npos);
    CHECK(!fs::exists(failed));
    fixture.write("space in file.txt", "bad\rnewline");
    CHECK(std::get<BuildError>(build(plan_with_empty, failed)).message.find("bare CR") !=
          std::string::npos);
    CHECK(!fs::exists(failed));

    fixture.write("space in file.txt", "");
    auto empty = fixture.root / "with-empty.afp";
    auto success = std::get<BuildSummary>(build(plan_with_empty, empty, Policy::par));
    auto reader = BundleReader::open(empty);
    CHECK(reader.entries()[1].offset == 3 && reader.entries()[1].size == 0);
    CHECK(reader.read("start") == Bytes{} && reader.verify().empty());
    CHECK(default_reports().render("sizes", success) ==
          "size_histogram empty=1 small_1_1024=1 medium_1025_65536=0 large=0");

    auto utf8_source = fixture.write(fs::u8path(u8"素材.bin"), "");
    auto empty_manifest =
        fixture.write(fs::u8path(u8"空包.assets"),
                      "asset\tempty\traw\t" + utf8_source.filename().u8string() + "\t0\n");
    auto empty_package = fixture.root / fs::u8path(u8"输出/空包.afp");
    auto only_empty = plan(load_catalog(empty_manifest), Root{"empty"});
    CHECK(std::get<BuildSummary>(build(only_empty, empty_package)).content_size == 0);
    auto zero_reader = BundleReader::open(empty_package);
    CHECK(zero_reader.verify(Policy::par).empty() && zero_reader.read("empty") == Bytes{});

    for (const auto& entry : fs::directory_iterator(fixture.root)) {
        CHECK(entry.path().filename().u8string().find(".assetforge-") != 0);
    }
}

void workspace_tests() {
    Fixture fixture;
    auto first =
        fixture.write("first.assets", std::string(base) + "alias\tbackup\ta\nprofile\tother\ta");
    auto second = fixture.write("second.assets", "asset\tnew\traw\tx\t-\nprofile\tp\tnew\n");
    auto missing = fixture.write("missing.assets", "asset\tnew\traw\tx\t-\nprofile\tq\tnew\n");
    Workspace workspace(load_catalog(first), Profile{"p"});
    auto frozen = workspace.snapshot();

    rejects([&] { workspace.rename_alias("start", "backup"); }, "alias already exists");
    CHECK(workspace.find("start").value().id == "b" && workspace.find("backup").value().id == "a");
    rejects([&] { workspace.rename_alias("start", "a"); }, "name is an asset ID");
    workspace.rename_alias("start", "start");
    workspace.rename_alias("start", "launch");
    CHECK(workspace.find("launch").value().id == "b" && !workspace.find("start"));
    CHECK(frozen.catalog.find("start") == 1);
    rejects([&] { workspace.reload(missing); }, "unknown profile: p");
    auto retained = workspace.find("launch");
    CHECK(workspace.selected().name == "p" && retained.value().id == "b");
    workspace.select(Profile{"other"});
    CHECK(workspace.selected().name == "other");
    rejects([&] { workspace.select(Profile{"bad"}); }, "unknown profile");
    rejects([&] { workspace.reload(second); }, "unknown profile: other");
    workspace.select(Profile{"p"});

    std::promise<void> started, resume;
    auto wait_for_reload = resume.get_future();
    auto reader = std::async(std::launch::async, [&] {
        auto snapshot = workspace.snapshot();
        started.set_value();
        wait_for_reload.wait();
        return ids(plan(snapshot.catalog, snapshot.profile));
    });
    started.get_future().wait();
    try {
        workspace.reload(second);
    } catch (...) {
        resume.set_value();
        throw;
    }
    resume.set_value();
    auto newest = workspace.snapshot();
    CHECK(reader.get() == std::vector<std::string>{"a", "b"});
    CHECK(ids(plan(newest.catalog, newest.profile)) == std::vector<std::string>{"new"});
    CHECK(!workspace.find("launch") && retained.value().id == "b");

    std::promise<void> start;
    auto ready = start.get_future().share();
    auto cycle = [&] {
        ready.wait();
        for (int i = 0; i < 100; ++i) {
            auto snapshot = workspace.snapshot();
            auto order = ids(plan(snapshot.catalog, snapshot.profile));
            CHECK((order == std::vector<std::string>{"a", "b"} ||
                   order == std::vector<std::string>{"new"}));
        }
    };
    auto left = std::async(std::launch::async, cycle);
    start.set_value();
    auto right = std::async(std::launch::async, cycle);
    for (int i = 0; i < 60; ++i) {
        workspace.reload(i % 2 ? first : second);
    }
    left.get();
    right.get();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::invalid_argument("expected test group");
        }
        std::string_view group(argv[1]);
        if (group == "manifest") {
            manifest_tests();
        } else if (group == "bundle") {
            bundle_tests();
        } else if (group == "workspace") {
            workspace_tests();
        } else {
            throw std::invalid_argument("unknown test group");
        }
        std::cout << group << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
