#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool ProcessFile(
    ifstream& input,
    ostream& output,
    const path& current_file,
    const vector<path>& include_directories
) 
{
    static const regex local_include(
        R"re(\s*#\s*include\s*"([^"]*)"\s*)re"
    );

    static const regex global_include(
        R"re(\s*#\s*include\s*<([^>]*)>\s*)re"
    );

    string line;
    size_t line_number = 0;

    while (getline(input, line)) {
        ++line_number;

        smatch match;

        bool is_local = regex_match(line, match, local_include);
        bool is_global = false;

        if (!is_local) {
            is_global = regex_match(line, match, global_include);
        }

        if (!is_local && !is_global) {
            output << line << '\n';
            continue;
        }

        path include_path = string(match[1]);
        path found_path;
        bool found = false;

        vector<path> search_paths;

        if (is_local) {
            search_paths.push_back(current_file.parent_path());
        }

        for (const auto& dir : include_directories) {
            search_paths.push_back(dir);
        }

        for (const auto& dir : search_paths) {
            path candidate = dir / include_path;

            ifstream included(candidate);

            if (included.is_open()) {
                found_path = candidate;
                found = true;

                if (!ProcessFile(
                        included,
                        output,
                        found_path,
                        include_directories
                    )) {
                    return false;
                }

                break;
            }
        }

        if (!found) {
            cout << "unknown include file " << include_path.string()
                 << " at file " << current_file.string()
                 << " at line " << line_number;

            return false;
        }
    }

    return true;
}

bool Preprocess(
    const path& in_file,
    const path& out_file,
    const vector<path>& include_directories
) 
{
    ifstream input(in_file);

    if (!input.is_open()) {
        return false;
    }

    ofstream output(out_file);

    if (!output.is_open()) {
        return false;
    }

    return ProcessFile(
        input,
        output,
        in_file,
        include_directories
    );
}

string GetFileContents(string file) {
    ifstream stream(file);

    return {
        istreambuf_iterator<char>(stream),
        istreambuf_iterator<char>()
    };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
