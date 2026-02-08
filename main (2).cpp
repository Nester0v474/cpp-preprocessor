#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintIncludeError(const string& filename, const path& original_file, int line_number) {
    cout << "unknown include file " << filename << " at file " << original_file.string() << " at line " << line_number << endl;
}

bool ProcessFile(const path& file_path, ofstream& out_file, const vector<path>& include_directories, int& line_number, const path& original_file) {
    ifstream file_stream(file_path);
    if (!file_stream) {
        PrintIncludeError(file_path.filename().string(), original_file, line_number);
        return false;  
    }

    string line;
    static const regex include_regex(R"(\s*#\s*include\s*\"([^\"]+)\"\s*)");
    static const regex angle_include_regex(R"(\s*#\s*include\s*<([^>]+)>\s*)");

    while (getline(file_stream, line)) {
        smatch match;
        if (regex_match(line, match, include_regex)) {
            path include_path = file_path.parent_path() / match[1].str();
            if (!filesystem::exists(include_path)) {
                for (const auto& dir : include_directories) {
                    include_path = dir / match[1].str();
                    if (filesystem::exists(include_path)) break;
                }
            }
            if (!filesystem::exists(include_path)) {
                PrintIncludeError(match[1].str(), original_file, line_number);
                return false;
            }
            int include_line_number = 1;
            if (!ProcessFile(include_path, out_file, include_directories, include_line_number, original_file)) return false;
        } else if (regex_match(line, match, angle_include_regex)) {
            bool found = false;
            for (const auto& dir : include_directories) {
                path include_path = dir / match[1].str();
                if (filesystem::exists(include_path)) {
                    found = true;
                    int include_line_number = 1;
                    if (!ProcessFile(include_path, out_file, include_directories, include_line_number, original_file)) return false;
                    break;
                }
            }
            if (!found) {
                PrintIncludeError(match[1].str(), original_file, line_number);
                return false;
            }
        } else {
            out_file << line << endl;  
        }
        line_number++;
    }
    return true;  
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in_stream(in_file);
    if (!in_stream) return false;

    ofstream out_stream(out_file);
    if (!out_stream) return false;

    int line_number = 1;
    return ProcessFile(in_file, out_stream, include_directories, line_number, in_file);
}

string GetFileContents(const string& file) {
    ifstream stream(file);
    return {istreambuf_iterator<char>(stream), istreambuf_iterator<char>()}; 
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
                "}\n";
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include\n";
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n";
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n";
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n";
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n";
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
                "    cout << \"hello, world!\" << endl;\n";

    assert(GetFileContents("sources/a.in") == test_out.str());
}

int main() {
    Test();
}
