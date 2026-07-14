#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

using Table = std::vector<std::vector<std::string>>;
using Workbook = std::map<std::string, Table>;

Table readCsv(const std::filesystem::path& path);
void writeCsv(const std::filesystem::path& path, const Table& table);

#ifdef CLASSRANKER_WITH_XLSX
Workbook readXlsx(const std::filesystem::path& path);
void writeXlsx(const std::filesystem::path& path, const Workbook& workbook);
#endif

