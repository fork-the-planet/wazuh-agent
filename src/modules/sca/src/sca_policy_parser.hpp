#pragma once

#include <isca_policy.hpp>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <memory>
#include <string>
#include <unordered_map>

/// @brief Compares two strings based on their length and then alphabetical order.
struct StringLengthGreater
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        return a.length() > b.length() || (a.length() == b.length() && a < b);
    }
};

/// @brief A map for storing variables extracted from the YAML file, utilized for variable substitution during policy
/// parsing.
/// The map keys are the variable names, while the values are the corresponding substitutions. The map uses custom
/// comparator that prioritizes longer strings and, if equal in length, sorts the strings alphabetically.
using PolicyVariables = std::map<std::string, std::string, StringLengthGreater>;

/// @brief Type alias for a function that loads a YAML file.
using LoadFileFunc = std::function<YAML::Node(const std::string&)>;

/// @brief Parses and processes SCA policy files defined in YAML format.
///
/// This class is responsible for reading an SCA policy YAML file,
/// resolving variables, and converting it into an internal `SCAPolicy`
/// representation. It also extracts and transforms policy and check
/// data into JSON for further use.
class PolicyParser
{
public:
    /// @brief Constructs a PolicyParser and loads the YAML file.
    /// @param filename Path to the YAML policy file.
    /// @param loadFileFunc Function to load the YAML file.
    explicit PolicyParser(const std::filesystem::path& filename, LoadFileFunc loadFileFunc = {});

    /// @brief Checks if the specified YAML file is valid.
    ///
    /// This function attempts to load the YAML file located at the given path.
    /// If the file can be loaded without throwing an exception, it is considered valid.
    ///
    /// @param filename The path to the YAML file to be validated.
    /// @return `true` if the file is a valid YAML file; `false` otherwise.
    bool IsValidYamlFile(const std::filesystem::path& filename) const;

    /// @brief Parses the loaded policy file and extracts a SCAPolicy object.
    ///
    /// The method also populates the given JSON object with detailed
    /// information on policies and checks for reporting usage.
    ///
    /// @param policiesAndChecks JSON object to be filled with extracted data.
    /// @return A populated SCAPolicy object.
    std::unique_ptr<ISCAPolicy> ParsePolicy(nlohmann::json& policiesAndChecks) const;

private:
    /// @brief Recursively replaces variables in the YAML node with their values.
    /// @param currentNode The YAML node to process.
    void ReplaceVariablesInNode(YAML::Node& currentNode);

    /// @brief Root YAML node loaded from the policy file.
    YAML::Node m_node;

    /// @brief Function to load the YAML file.
    LoadFileFunc m_loadFileFunc;

    /// @brief Map of variables found in the YAML file, used for substitution.
    PolicyVariables m_variablesMap;
};
