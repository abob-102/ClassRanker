#include "table_io.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("无法打开文件：" + path.u8string());
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::string escapeCsv(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    escaped += '"';
    return escaped;
}

} // namespace

Table readCsv(const std::filesystem::path& path) {
    std::string content = readFile(path);
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }

    Table table;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false;

    for (std::size_t index = 0; index < content.size(); ++index) {
        const char character = content[index];
        if (quoted) {
            if (character == '"') {
                if (index + 1 < content.size() && content[index + 1] == '"') {
                    field += '"';
                    ++index;
                } else {
                    quoted = false;
                }
            } else field += character;
        } else if (character == '"' && field.empty()) {
            quoted = true;
        } else if (character == ',') {
            row.push_back(std::move(field));
            field.clear();
        } else if (character == '\r' || character == '\n') {
            if (character == '\r' && index + 1 < content.size() && content[index + 1] == '\n') ++index;
            row.push_back(std::move(field));
            field.clear();
            if (!(row.size() == 1 && row.front().empty())) table.push_back(std::move(row));
            row.clear();
        } else field += character;
    }

    if (quoted) throw std::runtime_error("CSV 格式错误：存在未闭合的双引号：" + path.u8string());
    if (!field.empty() || !row.empty()) {
        row.push_back(std::move(field));
        table.push_back(std::move(row));
    }
    return table;
}

void writeCsv(const std::filesystem::path& path, const Table& table) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("无法写入文件：" + path.u8string());
    stream << "\xEF\xBB\xBF";
    for (const auto& row : table) {
        for (std::size_t column = 0; column < row.size(); ++column) {
            if (column != 0) stream << ',';
            stream << escapeCsv(row[column]);
        }
        stream << "\r\n";
    }
}

