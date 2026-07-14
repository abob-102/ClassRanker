#include "expression.hpp"
#include "table_io.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

struct FormulaConfig {
    std::string courseGpa = "grade_point";
    std::string scoreRanking = "weighted_score";
    std::string gpaRanking = "weighted_gpa";
    std::string comprehensiveRanking = "weighted_score*0.8+bonus_total";
};

struct CourseRecord {
    std::string name;
    double score = 0.0;
    double credit = 0.0;
    double enteredGradePoint = std::numeric_limits<double>::quiet_NaN();
};

struct Student {
    std::string id;
    std::string name;
    std::vector<CourseRecord> courses;
    double bonusTotal = 0.0;
    double weightedScore = 0.0;
    double averageScore = 0.0;
    double weightedGpa = 0.0;
    double averageGpa = 0.0;
    double totalCredits = 0.0;
    double scoreValue = 0.0;
    double gpaValue = 0.0;
    double comprehensiveValue = 0.0;
    int scoreRank = 0;
    int gpaRank = 0;
    int comprehensiveRank = 0;
};

std::string trim(std::string value) {
    const auto nonSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), nonSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), nonSpace).base(), value.end());
    return value;
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

double parseNumber(const std::string& text, const std::string& field, std::size_t row) {
    const std::string cleaned = trim(text);
    if (cleaned.empty()) throw std::runtime_error("第 " + std::to_string(row) + " 行的“" + field + "”为空");
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(cleaned, &consumed);
    } catch (...) {
        throw std::runtime_error("第 " + std::to_string(row) + " 行的“" + field + "”不是有效数字：" + cleaned);
    }
    if (consumed != cleaned.size() || !std::isfinite(value)) {
        throw std::runtime_error("第 " + std::to_string(row) + " 行的“" + field + "”不是有效数字：" + cleaned);
    }
    return value;
}

class TableReader {
public:
    TableReader(const Table& table, std::string tableName) : table_(table), tableName_(std::move(tableName)) {
        if (table_.empty()) throw std::runtime_error(tableName_ + " 表为空");
        for (std::size_t column = 0; column < table_[0].size(); ++column) {
            columns_.emplace(lowerAscii(trim(table_[0][column])), column);
        }
    }

    std::size_t rows() const { return table_.size(); }

    std::string get(std::size_t row, std::initializer_list<const char*> aliases, bool required = true) const {
        for (const char* alias : aliases) {
            const auto found = columns_.find(lowerAscii(alias));
            if (found != columns_.end()) {
                const std::size_t column = found->second;
                const std::string value = column < table_[row].size() ? trim(table_[row][column]) : "";
                if (required && value.empty()) {
                    throw std::runtime_error(tableName_ + " 第 " + std::to_string(row + 1) + " 行字段为空：" + alias);
                }
                return value;
            }
        }
        if (required) throw std::runtime_error(tableName_ + " 缺少必要表头：" + std::string(*aliases.begin()));
        return "";
    }

private:
    const Table& table_;
    std::string tableName_;
    std::unordered_map<std::string, std::size_t> columns_;
};

const Table* findSheet(const Workbook& workbook, std::initializer_list<const char*> aliases) {
    for (const auto& [name, table] : workbook) {
        for (const char* alias : aliases) {
            if (lowerAscii(trim(name)) == lowerAscii(alias)) return &table;
        }
    }
    return nullptr;
}

Workbook loadInput(const fs::path& input) {
    if (fs::is_directory(input)) {
        const fs::path students = input / "students.csv";
        const fs::path courses = input / "courses.csv";
        const fs::path bonuses = input / "bonuses.csv";
        if (!fs::exists(students) || !fs::exists(courses)) {
            throw std::runtime_error("CSV 目录必须包含 students.csv 和 courses.csv");
        }
        Workbook workbook{{"Students", readCsv(students)}, {"Courses", readCsv(courses)}};
        if (fs::exists(bonuses)) workbook.emplace("Bonuses", readCsv(bonuses));
        return workbook;
    }

    const std::string extension = lowerAscii(input.extension().u8string());
    if (extension == ".xlsx") {
#ifdef CLASSRANKER_WITH_XLSX
        return readXlsx(input);
#else
        throw std::runtime_error("当前版本未启用 XLSX；请重新编译并设置 CLASSRANKER_WITH_XLSX=ON");
#endif
    }
    throw std::runtime_error("输入应为包含三张 CSV 的目录，或一个 .xlsx 文件");
}

std::map<std::string, Student> parseStudents(const Workbook& workbook) {
    const Table* table = findSheet(workbook, {"Students", "学生", "学生信息"});
    if (!table) throw std::runtime_error("找不到 Students（学生）工作表");
    TableReader reader(*table, "Students");
    std::map<std::string, Student> students;
    for (std::size_t row = 1; row < reader.rows(); ++row) {
        const std::string id = reader.get(row, {"student_id", "学号"});
        const std::string name = reader.get(row, {"name", "姓名"});
        if (!students.emplace(id, Student{id, name}).second) {
            throw std::runtime_error("Students 中存在重复学号：" + id);
        }
    }
    if (students.empty()) throw std::runtime_error("Students 中没有学生数据");
    return students;
}

void parseCourses(const Workbook& workbook, std::map<std::string, Student>& students) {
    const Table* table = findSheet(workbook, {"Courses", "课程", "课程成绩", "成绩"});
    if (!table) throw std::runtime_error("找不到 Courses（课程成绩）工作表");
    TableReader reader(*table, "Courses");
    for (std::size_t row = 1; row < reader.rows(); ++row) {
        const std::string id = reader.get(row, {"student_id", "学号"});
        auto student = students.find(id);
        if (student == students.end()) throw std::runtime_error("Courses 引用了未知学号：" + id);
        CourseRecord course;
        course.name = reader.get(row, {"course", "课程", "课程名称"});
        course.score = parseNumber(reader.get(row, {"score", "成绩"}), "成绩", row + 1);
        course.credit = parseNumber(reader.get(row, {"credit", "学分"}), "学分", row + 1);
        if (course.credit <= 0.0) throw std::runtime_error("Courses 第 " + std::to_string(row + 1) + " 行学分必须大于 0");
        const std::string gradePoint = reader.get(row, {"grade_point", "绩点", "课程绩点"}, false);
        if (!gradePoint.empty()) course.enteredGradePoint = parseNumber(gradePoint, "绩点", row + 1);
        student->second.courses.push_back(std::move(course));
    }
}

void parseBonuses(const Workbook& workbook, std::map<std::string, Student>& students) {
    const Table* table = findSheet(workbook, {"Bonuses", "加分项", "综测加分"});
    if (!table || table->size() <= 1) return;
    TableReader reader(*table, "Bonuses");
    for (std::size_t row = 1; row < reader.rows(); ++row) {
        const std::string id = reader.get(row, {"student_id", "学号"});
        auto student = students.find(id);
        if (student == students.end()) throw std::runtime_error("Bonuses 引用了未知学号：" + id);
        student->second.bonusTotal += parseNumber(reader.get(row, {"points", "分值", "加分"}), "加分", row + 1);
    }
}

FormulaConfig readConfig(const fs::path& path) {
    FormulaConfig config;
    if (path.empty()) return config;
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("无法打开公式配置：" + path.u8string());
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const std::size_t equal = line.find('=');
        if (equal == std::string::npos) throw std::runtime_error("公式配置行缺少 '='：" + line);
        const std::string key = lowerAscii(trim(line.substr(0, equal)));
        const std::string value = trim(line.substr(equal + 1));
        if (key == "course_gpa_formula") config.courseGpa = value;
        else if (key == "score_ranking_formula") config.scoreRanking = value;
        else if (key == "gpa_ranking_formula") config.gpaRanking = value;
        else if (key == "comprehensive_ranking_formula") config.comprehensiveRanking = value;
    }
    return config;
}

std::string promptDefault(const std::string& label, const std::string& defaultValue) {
    std::cout << label << " [" << defaultValue << "]：";
    std::string value;
    std::getline(std::cin, value);
    value = trim(value);
    return value.empty() ? defaultValue : value;
}

void editFormulas(FormulaConfig& config) {
    std::cout << "\n可直接回车采用方括号内的公式。\n";
    config.courseGpa = promptDefault("课程绩点公式", config.courseGpa);
    config.scoreRanking = promptDefault("成绩排名公式", config.scoreRanking);
    config.gpaRanking = promptDefault("绩点排名公式", config.gpaRanking);
    config.comprehensiveRanking = promptDefault("综测排名公式", config.comprehensiveRanking);
}

void calculate(std::map<std::string, Student>& students, const FormulaConfig& config) {
    const Expression courseGpa(config.courseGpa);
    const Expression scoreFormula(config.scoreRanking);
    const Expression gpaFormula(config.gpaRanking);
    const Expression comprehensiveFormula(config.comprehensiveRanking);

    for (auto& [id, student] : students) {
        if (student.courses.empty()) throw std::runtime_error("学生 " + id + " 没有课程成绩");
        double weightedScoreSum = 0.0;
        double scoreSum = 0.0;
        double weightedGpaSum = 0.0;
        double gpaSum = 0.0;
        for (const CourseRecord& course : student.courses) {
            double point = 0.0;
            try {
                point = courseGpa.evaluate({{"score", course.score}, {"credit", course.credit},
                                            {"grade_point", course.enteredGradePoint}});
            } catch (const std::exception& error) {
                throw std::runtime_error("学生 " + id + "、课程“" + course.name + "”的绩点计算失败：" + error.what());
            }
            student.totalCredits += course.credit;
            weightedScoreSum += course.score * course.credit;
            scoreSum += course.score;
            weightedGpaSum += point * course.credit;
            gpaSum += point;
        }
        const double courseCount = static_cast<double>(student.courses.size());
        student.weightedScore = weightedScoreSum / student.totalCredits;
        student.averageScore = scoreSum / courseCount;
        student.weightedGpa = weightedGpaSum / student.totalCredits;
        student.averageGpa = gpaSum / courseCount;

        const std::unordered_map<std::string, double> variables{
            {"weighted_score", student.weightedScore}, {"average_score", student.averageScore},
            {"weighted_gpa", student.weightedGpa}, {"average_gpa", student.averageGpa},
            {"total_credits", student.totalCredits}, {"course_count", courseCount},
            {"bonus_total", student.bonusTotal}};
        try {
            student.scoreValue = scoreFormula.evaluate(variables);
            student.gpaValue = gpaFormula.evaluate(variables);
            student.comprehensiveValue = comprehensiveFormula.evaluate(variables);
        } catch (const std::exception& error) {
            throw std::runtime_error("学生 " + id + " 的排名公式计算失败：" + error.what());
        }
    }
}

using ValueGetter = std::function<double(const Student&)>;
using RankSetter = std::function<void(Student&, int)>;

void assignRanks(std::map<std::string, Student>& students, const ValueGetter& value, const RankSetter& setRank) {
    std::vector<Student*> order;
    for (auto& [id, student] : students) order.push_back(&student);
    std::stable_sort(order.begin(), order.end(), [&](const Student* left, const Student* right) {
        const double difference = value(*left) - value(*right);
        return std::abs(difference) <= 1e-9 ? left->id < right->id : difference > 0.0;
    });
    int rank = 0;
    double previous = 0.0;
    for (std::size_t index = 0; index < order.size(); ++index) {
        const double current = value(*order[index]);
        if (index == 0 || std::abs(current - previous) > 1e-9) rank = static_cast<int>(index + 1);
        setRank(*order[index], rank);
        previous = current;
    }
}

std::string number(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(4) << value;
    return output.str();
}

Table makeSummary(const std::map<std::string, Student>& students) {
    Table result{{"学号", "姓名", "加权平均分", "算术平均分", "加权平均绩点", "算术平均绩点",
                  "总学分", "课程数", "加分合计", "成绩排名值", "成绩排名", "绩点排名值",
                  "绩点排名", "综测成绩", "综测排名"}};
    for (const auto& [id, student] : students) {
        result.push_back({id, student.name, number(student.weightedScore), number(student.averageScore),
                          number(student.weightedGpa), number(student.averageGpa), number(student.totalCredits),
                          std::to_string(student.courses.size()), number(student.bonusTotal), number(student.scoreValue),
                          std::to_string(student.scoreRank), number(student.gpaValue), std::to_string(student.gpaRank),
                          number(student.comprehensiveValue), std::to_string(student.comprehensiveRank)});
    }
    return result;
}

Table makeRanking(const std::map<std::string, Student>& students, const ValueGetter& value,
                  const std::function<int(const Student&)>& rank, const std::string& valueName) {
    std::vector<const Student*> order;
    for (const auto& [id, student] : students) order.push_back(&student);
    std::stable_sort(order.begin(), order.end(), [&](const Student* left, const Student* right) {
        if (rank(*left) != rank(*right)) return rank(*left) < rank(*right);
        return left->id < right->id;
    });
    Table result{{"排名", "学号", "姓名", valueName}};
    for (const Student* student : order) {
        result.push_back({std::to_string(rank(*student)), student->id, student->name, number(value(*student))});
    }
    return result;
}

Workbook makeOutput(const std::map<std::string, Student>& students) {
    Workbook output;
    output["Summary"] = makeSummary(students);
    output["Score Ranking"] = makeRanking(students, [](const Student& s) { return s.scoreValue; },
                                           [](const Student& s) { return s.scoreRank; }, "成绩排名值");
    output["GPA Ranking"] = makeRanking(students, [](const Student& s) { return s.gpaValue; },
                                         [](const Student& s) { return s.gpaRank; }, "绩点排名值");
    output["Comprehensive Ranking"] = makeRanking(students, [](const Student& s) { return s.comprehensiveValue; },
                                                   [](const Student& s) { return s.comprehensiveRank; }, "综测成绩");
    return output;
}

void saveOutput(const fs::path& outputPath, const Workbook& workbook) {
    if (lowerAscii(outputPath.extension().u8string()) == ".xlsx") {
#ifdef CLASSRANKER_WITH_XLSX
        if (!outputPath.parent_path().empty()) fs::create_directories(outputPath.parent_path());
        writeXlsx(outputPath, workbook);
#else
        throw std::runtime_error("当前版本未启用 XLSX 导出");
#endif
        return;
    }
    fs::create_directories(outputPath);
    writeCsv(outputPath / "summary.csv", workbook.at("Summary"));
    writeCsv(outputPath / "score_ranking.csv", workbook.at("Score Ranking"));
    writeCsv(outputPath / "gpa_ranking.csv", workbook.at("GPA Ranking"));
    writeCsv(outputPath / "comprehensive_ranking.csv", workbook.at("Comprehensive Ranking"));
}

Workbook makeInputTemplate() {
    return {
        {"Students", {{"student_id", "name"}, {"2026001", "张三"}, {"2026002", "李四"}, {"2026003", "王五"}}},
        {"Courses", {{"student_id", "course", "score", "credit", "grade_point"},
                     {"2026001", "高等数学", "92", "4", "4.0"},
                     {"2026001", "大学英语", "86", "3", "3.7"},
                     {"2026002", "高等数学", "88", "4", "3.7"},
                     {"2026002", "大学英语", "91", "3", "4.0"},
                     {"2026003", "高等数学", "83", "4", "3.3"},
                     {"2026003", "大学英语", "95", "3", "4.0"}}},
        {"Bonuses", {{"student_id", "item", "points"},
                     {"2026001", "志愿服务", "2.0"}, {"2026001", "学科竞赛", "3.0"},
                     {"2026002", "学生工作", "3.0"}, {"2026003", "学科竞赛", "4.5"}}}
    };
}

void saveInputTemplate(const fs::path& path) {
    const Workbook workbook = makeInputTemplate();
    if (lowerAscii(path.extension().u8string()) == ".xlsx") {
#ifdef CLASSRANKER_WITH_XLSX
        if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
        writeXlsx(path, workbook);
#else
        throw std::runtime_error("当前版本未启用 XLSX 模板生成");
#endif
        return;
    }
    fs::create_directories(path);
    writeCsv(path / "students.csv", workbook.at("Students"));
    writeCsv(path / "courses.csv", workbook.at("Courses"));
    writeCsv(path / "bonuses.csv", workbook.at("Bonuses"));
}

void printHelp() {
    std::cout <<
        "班级成绩、绩点与综测排名工具\n\n"
        "用法：ClassRanker [--input 路径] [--output 路径] [--config formulas.ini] [--no-prompt]\n"
        "模板：ClassRanker --create-template class_data.xlsx\n"
        "输入可为含 students.csv/courses.csv/bonuses.csv 的目录，或含对应工作表的 .xlsx。\n"
        "输出为目录时生成四个 CSV；输出扩展名为 .xlsx 时生成一个 Excel 工作簿。\n";
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try {
        fs::path inputPath;
        fs::path outputPath;
        fs::path configPath;
        fs::path templatePath;
        bool prompt = true;

        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            auto requireValue = [&]() -> std::string {
                if (++index >= argc) throw std::runtime_error("参数 " + argument + " 缺少值");
                return argv[index];
            };
            if (argument == "--input") inputPath = fs::u8path(requireValue());
            else if (argument == "--output") outputPath = fs::u8path(requireValue());
            else if (argument == "--config") configPath = fs::u8path(requireValue());
            else if (argument == "--create-template") templatePath = fs::u8path(requireValue());
            else if (argument == "--no-prompt") prompt = false;
            else if (argument == "--help" || argument == "-h") { printHelp(); return 0; }
            else throw std::runtime_error("未知参数：" + argument);
        }

        if (!templatePath.empty()) {
            saveInputTemplate(templatePath);
            std::cout << "输入模板已生成：" << templatePath.u8string() << "\n";
            return 0;
        }

        if (inputPath.empty()) {
            std::cout << "请输入 CSV 数据目录或 XLSX 文件路径：";
            std::string value;
            std::getline(std::cin, value);
            inputPath = fs::u8path(trim(value));
        }
        if (inputPath.empty()) throw std::runtime_error("未提供输入路径");

        if (outputPath.empty()) {
            std::cout << "请输入输出路径（目录或 .xlsx 文件，回车使用 ranking_results）：";
            std::string value;
            std::getline(std::cin, value);
            outputPath = value.empty() ? fs::path("ranking_results") : fs::u8path(trim(value));
        }

        FormulaConfig formulas = readConfig(configPath);
        if (prompt) editFormulas(formulas);

        Workbook input = loadInput(inputPath);
        std::map<std::string, Student> students = parseStudents(input);
        parseCourses(input, students);
        parseBonuses(input, students);
        calculate(students, formulas);
        assignRanks(students, [](const Student& s) { return s.scoreValue; },
                    [](Student& s, int rank) { s.scoreRank = rank; });
        assignRanks(students, [](const Student& s) { return s.gpaValue; },
                    [](Student& s, int rank) { s.gpaRank = rank; });
        assignRanks(students, [](const Student& s) { return s.comprehensiveValue; },
                    [](Student& s, int rank) { s.comprehensiveRank = rank; });
        saveOutput(outputPath, makeOutput(students));

        std::cout << "\n计算完成，共处理 " << students.size() << " 名学生。\n"
                  << "结果已保存至：" << outputPath.u8string() << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "错误：" << error.what() << "\n";
        return 1;
    }
}
