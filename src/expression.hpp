#pragma once

#include <string>
#include <unordered_map>

class Expression {
public:
    explicit Expression(std::string source);
    double evaluate(const std::unordered_map<std::string, double>& variables) const;
    const std::string& source() const noexcept { return source_; }

private:
    std::string source_;
};

