#include "expression.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class Parser {
public:
    Parser(std::string_view input, const std::unordered_map<std::string, double>& variables)
        : input_(input), variables_(variables) {}

    double parse() {
        const double result = parseOr();
        skipSpaces();
        if (position_ != input_.size()) fail("无法识别的字符");
        if (!std::isfinite(result)) fail("计算结果不是有效数字");
        return result;
    }

private:
    double parseOr() {
        double left = parseAnd();
        while (match("||")) {
            const double right = parseAnd();
            left = truthy(left) || truthy(right) ? 1.0 : 0.0;
        }
        return left;
    }

    double parseAnd() {
        double left = parseEquality();
        while (match("&&")) {
            const double right = parseEquality();
            left = truthy(left) && truthy(right) ? 1.0 : 0.0;
        }
        return left;
    }

    double parseEquality() {
        double left = parseComparison();
        while (true) {
            if (match("==")) left = left == parseComparison() ? 1.0 : 0.0;
            else if (match("!=")) left = left != parseComparison() ? 1.0 : 0.0;
            else return left;
        }
    }

    double parseComparison() {
        double left = parseAdditive();
        while (true) {
            if (match(">=")) left = left >= parseAdditive() ? 1.0 : 0.0;
            else if (match("<=")) left = left <= parseAdditive() ? 1.0 : 0.0;
            else if (match(">")) left = left > parseAdditive() ? 1.0 : 0.0;
            else if (match("<")) left = left < parseAdditive() ? 1.0 : 0.0;
            else return left;
        }
    }

    double parseAdditive() {
        double left = parseMultiplicative();
        while (true) {
            if (match("+")) left += parseMultiplicative();
            else if (match("-")) left -= parseMultiplicative();
            else return left;
        }
    }

    double parseMultiplicative() {
        double left = parsePower();
        while (true) {
            if (match("*")) left *= parsePower();
            else if (match("/")) {
                const double divisor = parsePower();
                if (divisor == 0.0) fail("除数不能为 0");
                left /= divisor;
            } else if (match("%")) {
                const double divisor = parsePower();
                if (divisor == 0.0) fail("取余数的除数不能为 0");
                left = std::fmod(left, divisor);
            } else return left;
        }
    }

    double parsePower() {
        double left = parseUnary();
        if (match("^")) left = std::pow(left, parsePower());
        return left;
    }

    double parseUnary() {
        if (match("+")) return parseUnary();
        if (match("-")) return -parseUnary();
        if (match("!")) return truthy(parseUnary()) ? 0.0 : 1.0;
        return parsePrimary();
    }

    double parsePrimary() {
        skipSpaces();
        if (match("(")) {
            const double value = parseOr();
            expect(")");
            return value;
        }
        if (position_ < input_.size() &&
            (std::isdigit(static_cast<unsigned char>(input_[position_])) || input_[position_] == '.')) {
            return parseNumber();
        }
        if (position_ < input_.size() &&
            (std::isalpha(static_cast<unsigned char>(input_[position_])) || input_[position_] == '_')) {
            std::string name = parseIdentifier();
            if (match("(")) {
                std::vector<double> arguments;
                if (!peek(")")) {
                    do arguments.push_back(parseOr()); while (match(","));
                }
                expect(")");
                return callFunction(std::move(name), arguments);
            }
            toLower(name);
            if (name == "true") return 1.0;
            if (name == "false") return 0.0;
            const auto found = variables_.find(name);
            if (found == variables_.end()) fail("未知变量：" + name);
            if (!std::isfinite(found->second)) fail("变量 " + name + " 没有有效数据");
            return found->second;
        }
        fail("此处应为数字、变量或括号");
    }

    double parseNumber() {
        const char* begin = input_.data() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin) fail("数字格式错误");
        position_ += static_cast<std::size_t>(end - begin);
        return value;
    }

    std::string parseIdentifier() {
        const std::size_t start = position_;
        while (position_ < input_.size()) {
            const unsigned char current = static_cast<unsigned char>(input_[position_]);
            if (!std::isalnum(current) && current != '_') break;
            ++position_;
        }
        return std::string(input_.substr(start, position_ - start));
    }

    double callFunction(std::string name, const std::vector<double>& args) {
        toLower(name);
        if (name == "if") {
            requireCount(name, args, 3);
            return truthy(args[0]) ? args[1] : args[2];
        }
        if (name == "min" || name == "max") {
            if (args.empty()) fail(name + " 至少需要一个参数");
            return name == "min" ? *std::min_element(args.begin(), args.end())
                                 : *std::max_element(args.begin(), args.end());
        }
        if (name == "abs") { requireCount(name, args, 1); return std::abs(args[0]); }
        if (name == "sqrt") {
            requireCount(name, args, 1);
            if (args[0] < 0.0) fail("sqrt 的参数不能小于 0");
            return std::sqrt(args[0]);
        }
        if (name == "floor" || name == "ceil") {
            requireCount(name, args, 1);
            return name == "floor" ? std::floor(args[0]) : std::ceil(args[0]);
        }
        if (name == "round") {
            if (args.size() != 1 && args.size() != 2) fail("round 需要 1 或 2 个参数");
            if (args.size() == 1) return std::round(args[0]);
            const double factor = std::pow(10.0, args[1]);
            return std::round(args[0] * factor) / factor;
        }
        if (name == "pow") { requireCount(name, args, 2); return std::pow(args[0], args[1]); }
        if (name == "clamp") {
            requireCount(name, args, 3);
            if (args[1] > args[2]) fail("clamp 的下限不能大于上限");
            return std::clamp(args[0], args[1], args[2]);
        }
        fail("未知函数：" + name);
    }

    void requireCount(const std::string& name, const std::vector<double>& args, std::size_t count) {
        if (args.size() != count) fail(name + " 需要 " + std::to_string(count) + " 个参数");
    }
    static bool truthy(double value) { return value != 0.0; }
    void skipSpaces() {
        while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
    }
    bool peek(std::string_view token) {
        skipSpaces();
        return input_.substr(position_, token.size()) == token;
    }
    bool match(std::string_view token) {
        if (!peek(token)) return false;
        position_ += token.size();
        return true;
    }
    void expect(std::string_view token) {
        if (!match(token)) fail("缺少 '" + std::string(token) + "'");
    }
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("公式错误（位置 " + std::to_string(position_ + 1) + "）：" + message);
    }
    static void toLower(std::string& value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }

    std::string_view input_;
    const std::unordered_map<std::string, double>& variables_;
    std::size_t position_ = 0;
};

} // namespace

Expression::Expression(std::string source) : source_(std::move(source)) {
    if (source_.empty()) throw std::invalid_argument("公式不能为空");
}

double Expression::evaluate(const std::unordered_map<std::string, double>& variables) const {
    std::unordered_map<std::string, double> normalized;
    normalized.reserve(variables.size());
    for (const auto& [name, value] : variables) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        normalized.emplace(std::move(lowerName), value);
    }
    return Parser(source_, normalized).parse();
}
