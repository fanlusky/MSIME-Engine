#include <metasequoia/session.h>
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace metasequoia;
void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
std::size_t candidate(Session &session, const std::string &word)
{
    const auto view = session.snapshot();
    for (std::size_t i = 0; i < view.candidates.size(); ++i)
        if (view.candidates[i].word == word)
            return i;
    throw std::runtime_error("Missing candidate: " + word);
}
void type(Session &session, const std::string &digits)
{
    for (char digit : digits)
        require(session.character(digit).handled, "digit unhandled");
}
int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("msime-nine-key-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{directory};
    sqlite3 *db = nullptr;
    require(sqlite3_open((directory / "msime.db").u8string().c_str(), &db) == SQLITE_OK, "open fixture");
    require(sqlite3_exec(db,
                         "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                         "INSERT INTO tbl_1_n VALUES('ni','n','你',100);"
                         "CREATE TABLE tbl_1_m(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                         "INSERT INTO tbl_1_m VALUES('mi','m','米',50);"
                         "CREATE TABLE tbl_1_h(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                         "INSERT INTO tbl_1_h VALUES('hao','h','好',100);"
                         "CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                         "INSERT INTO tbl_2_n VALUES('ni''hao','nh','你好',1000);",
                         nullptr, nullptr, nullptr) == SQLITE_OK,
            "populate fixture");
    sqlite3_close(db);
    SessionOptions options;
    options.paths = {directory, directory, directory, directory};
    options.learning = false;
    Session session(options);
    require(!session.character('6').handled, "ordinary pinyin swallowed digit");
    session.set_nine_key_enabled(true);
    require(!session.character('0').handled && !session.character('1').handled, "invalid digits accepted");
    type(session, "64");
    candidate(session, "你");
    candidate(session, "米");
    const auto before = session.snapshot().preedit;
    require(!session.select(999).handled && !session.choose_nine_key_spelling(999).handled &&
                session.snapshot().preedit == before,
            "invalid index changed composition");
    auto view = session.snapshot();
    require(view.nine_key_spellings.front() == "ni", "preferred spelling was offscreen");
    const auto ni = std::find(view.nine_key_spellings.begin(), view.nine_key_spellings.end(), "ni");
    require(ni != view.nine_key_spellings.end(), "missing disambiguation");
    require(session.choose_nine_key_spelling(ni - view.nine_key_spellings.begin()).handled, "choose spelling");
    require(session.snapshot().preedit == "ni", "locked preedit");
    for (const auto &item : session.snapshot().candidates)
        require(item.word != "米", "lock failed");
    type(session, "426");
    require(session.snapshot().nine_key_spellings.front() == "hao", "locked suffix spelling order");
    require(session.select(candidate(session, "你好")).commit == "你好" && session.snapshot().preedit.empty(),
            "phrase selection");
    type(session, "64426");
    require(session.select(candidate(session, "你")).commit == "你" && session.snapshot().editing_text == "426",
            "partial selection");
    require(session.finish().commit == "好" && session.snapshot().preedit.empty(), "finish residual");
    type(session, "64");
    session.command(Command::Backspace);
    require(session.snapshot().editing_text == "6", "backspace");
    session.command(Command::Backspace);
    require(!session.command(Command::Backspace).handled, "idle backspace");
    type(session, "64426");
    require(session.punctuation(',').commit == "你好，" && session.snapshot().preedit.empty(), "punctuation commit");
    type(session, "64");
    require(session.command(Command::CommitRaw).commit == "64", "raw commit");
    type(session, "64");
    session.command(Command::Cancel);
    require(session.snapshot().preedit.empty() && session.snapshot().nine_key_spellings.empty(), "cancel");
    type(session, "64");
    session.switch_scheme(SchemeType::Shuangpin);
    require(session.snapshot().preedit.empty() && !session.character('6').handled, "shuangpin isolation");
    session.switch_scheme(SchemeType::Quanpin);
    session.set_nine_key_enabled(false);
    require(session.character('n').handled && session.snapshot().editing_text == "n", "restore qwerty");
    session.command(Command::Cancel);
    session.set_nine_key_enabled(true);
    type(session, std::string(32, '7'));
    require(session.character('7').diagnostic.has_value() && session.snapshot().editing_text.size() == 32,
            "digit limit did not preserve composition");
    session.command(Command::Cancel);
    std::cout << "Nine-key input contract passed\n";
}
