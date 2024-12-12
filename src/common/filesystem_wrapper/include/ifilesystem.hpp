#pragma once

#include <filesystem>

/// @brief Interface for file system operations.
///
/// This interface defines a set of file system operations such as checking if a file exists,
/// removing files or directories, creating directories, and renaming files. Any concrete class
/// that implements this interface will be expected to provide the actual functionality for these
/// operations. This allows for abstraction and easier testing or swapping of file system implementations.
class IFileSystem {
public:
    /// @brief Virtual destructor for IFileSystem.
    ///
    /// Ensures that any derived classes with their own resources are correctly cleaned up.
    virtual ~IFileSystem() = default;

    /// @brief Checks if the specified path exists in the file system.
    /// @param path The path to check.
    /// @return Returns true if the path exists, otherwise false.
    virtual bool exists(const std::filesystem::path& path) const = 0;

    /// @brief Checks if the specified path is a directory.
    /// @param path The path to check.
    /// @return Returns true if the path is a directory, otherwise false.
    virtual bool is_directory(const std::filesystem::path& path) const = 0;

    /// @brief Removes all files and subdirectories in the specified directory.
    /// @param path The directory path to remove.
    /// @return Returns the number of files and directories removed.
    virtual std::uintmax_t remove_all(const std::filesystem::path& path) = 0;

    /// @brief Retrieves the system's temporary directory path.
    /// @return Returns the path of the system's temporary directory.
    virtual std::filesystem::path temp_directory_path() const = 0;

    /// @brief Creates a new directory at the specified path.
    /// @param path The path of the directory to create.
    /// @return Returns true if the directory was successfully created, otherwise false.
    virtual bool create_directories(const std::filesystem::path& path) = 0;

    /// @brief Renames a file or directory from the 'from' path to the 'to' path.
    /// @param from The current path of the file or directory.
    /// @param to The new path for the file or directory.
    virtual void rename(const std::filesystem::path& from, const std::filesystem::path& to) = 0;

    /// @brief Removes the specified file or directory.
    /// @param path The file or directory to remove.
    /// @return Returns true if the file or directory was successfully removed, otherwise false.
    virtual bool remove(const std::filesystem::path& path) = 0;
};