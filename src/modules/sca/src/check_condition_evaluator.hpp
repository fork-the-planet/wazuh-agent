#pragma once

#include <sca_policy_check.hpp>

#include <optional>
#include <string>

enum class ConditionType
{
    All,
    Any,
    None
};

class CheckConditionEvaluator
{
public:
    static CheckConditionEvaluator FromString(const std::string& str);

    explicit CheckConditionEvaluator(ConditionType type);

    void AddResult(RuleResult result);

    bool Result() const;

private:
    ConditionType m_type;
    int m_totalRules {0};
    int m_passedRules {0};
    std::optional<bool> m_result;
};
