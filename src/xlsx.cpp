#include "table_io.hpp"

#include <OpenXLSX.hpp>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string cellText(const OpenXLSX::XLCell& cell) {
    const auto& value = cell.value();
    switch (value.type()) {
    case OpenXLSX::XLValueType::Empty:
        return "";
    case OpenXLSX::XLValueType::Boolean:
        return value.get<bool>() ? "1" : "0";
    case OpenXLSX::XLValueType::Integer:
        return std::to_string(value.get<std::int64_t>());
    case OpenXLSX::XLValueType::Float: {
        std::ostringstream output;
        output << std::setprecision(15) << value.get<double>();
        return output.str();
    }
    case OpenXLSX::XLValueType::String:
        return value.get<std::string>();
    default:
        return "";
    }
}

} // namespace

Workbook readXlsx(const std::filesystem::path& path) {
    OpenXLSX::XLDocument document;
    bool opened = false;
    try {
        document.open(path.u8string());
        opened = true;
        Workbook result;
        const auto names = document.workbook().worksheetNames();
        for (const std::string& name : names) {
            auto sheet = document.workbook().worksheet(name);
            Table table;
            const auto rowCount = sheet.rowCount();
            const auto columnCount = sheet.columnCount();
            for (std::uint32_t row = 1; row <= rowCount; ++row) {
                std::vector<std::string> values;
                values.reserve(columnCount);
                for (std::uint16_t column = 1; column <= columnCount; ++column) {
                    values.push_back(cellText(sheet.cell(OpenXLSX::XLCellReference(row, column))));
                }
                while (!values.empty() && values.back().empty()) values.pop_back();
                if (!values.empty()) table.push_back(std::move(values));
            }
            result.emplace(name, std::move(table));
        }
        document.close();
        opened = false;
        return result;
    } catch (const std::exception& error) {
        if (opened) document.close();
        throw std::runtime_error("读取 XLSX 失败：" + std::string(error.what()));
    }
}

void writeXlsx(const std::filesystem::path& path, const Workbook& workbook) {
    if (workbook.empty()) throw std::runtime_error("不能导出空工作簿");

    if (std::filesystem::exists(path)) {
        std::error_code removeError;
        const bool removed = std::filesystem::remove(path, removeError);
        if (!removed || removeError) {
            throw std::runtime_error("无法覆盖 XLSX 文件，请先在 Excel 中关闭该文件：" + path.u8string());
        }
    }

    OpenXLSX::XLDocument document;
    bool opened = false;
    try {
        document.create(path.u8string());
        opened = true;
        auto book = document.workbook();
        auto iterator = workbook.begin();
        const auto initialSheetNames = book.worksheetNames();
        if (initialSheetNames.empty()) throw std::runtime_error("新建的 XLSX 中没有默认工作表");
        auto sheet = book.worksheet(initialSheetNames.front());
        sheet.setName(iterator->first);

        for (; iterator != workbook.end(); ++iterator) {
            if (iterator != workbook.begin()) {
                book.addWorksheet(iterator->first);
                sheet = book.worksheet(iterator->first);
            }
            const Table& table = iterator->second;
            for (std::size_t row = 0; row < table.size(); ++row) {
                for (std::size_t column = 0; column < table[row].size(); ++column) {
                    sheet.cell(OpenXLSX::XLCellReference(static_cast<std::uint32_t>(row + 1),
                                                         static_cast<std::uint16_t>(column + 1)))
                        .value() = table[row][column];
                }
            }
        }
        document.save();
        document.close();
        opened = false;
    } catch (const std::exception& error) {
        if (opened) document.close();
        throw std::runtime_error("写入 XLSX 失败：" + std::string(error.what()));
    }
}
