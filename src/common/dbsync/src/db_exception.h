#pragma once

#include <pal.h>
#include <stdexcept>
#include <string>

using DBSyncExceptionType = const std::pair<int, std::string>;

DBSyncExceptionType FACTORY_INSTANTATION {std::make_pair(1, "Unspecified type during factory instantiation")};
DBSyncExceptionType INVALID_HANDLE {std::make_pair(2, "Invalid handle value.")};
DBSyncExceptionType INVALID_TRANSACTION {std::make_pair(3, "Invalid transaction value.")};
DBSyncExceptionType EMPTY_DATABASE_PATH {std::make_pair(5, "Empty database store path.")};
DBSyncExceptionType EMPTY_TABLE_METADATA {std::make_pair(6, "Empty table metadata.")};
DBSyncExceptionType INVALID_PARAMETERS {std::make_pair(7, "Invalid parameters.")};
DBSyncExceptionType DATATYPE_NOT_IMPLEMENTED {std::make_pair(8, "Datatype not implemented.")};
DBSyncExceptionType SQL_STMT_ERROR {std::make_pair(9, "Invalid SQL statement.")};
DBSyncExceptionType INVALID_PK_DATA {std::make_pair(10, "Primary key not found.")};
DBSyncExceptionType INVALID_COLUMN_TYPE {std::make_pair(11, "Invalid column field type.")};
DBSyncExceptionType INVALID_DATA_BIND {std::make_pair(12, "Invalid data to bind.")};
DBSyncExceptionType INVALID_TABLE {std::make_pair(13, "Invalid table.")};
DBSyncExceptionType INVALID_DELETE_INFO {std::make_pair(14, "Invalid information provided for deletion.")};
DBSyncExceptionType BIND_FIELDS_DOES_NOT_MATCH {
    std::make_pair(15, "Invalid information provided for statement creation.")};
DBSyncExceptionType STEP_ERROR_CREATE_STMT {std::make_pair(16, "Error creating table.")};
DBSyncExceptionType STEP_ERROR_ADD_STATUS_FIELD {std::make_pair(17, "Error adding status field.")};
DBSyncExceptionType STEP_ERROR_UPDATE_STATUS_FIELD {std::make_pair(18, "Error updating status field.")};
DBSyncExceptionType STEP_ERROR_DELETE_STATUS_FIELD {std::make_pair(19, "Error deleting status field.")};
DBSyncExceptionType DELETE_OLD_DB_ERROR {std::make_pair(20, "Error deleting old db.")};
DBSyncExceptionType MIN_ROW_LIMIT_BELOW_ZERO {std::make_pair(21, "Invalid row limit, values below 0 not allowed.")};
DBSyncExceptionType ERROR_COUNT_MAX_ROWS {std::make_pair(22, "Count is less than 0.")};
DBSyncExceptionType STEP_ERROR_UPDATE_STMT {std::make_pair(23, "Error upgrading DB.")};

namespace DbSync
{
    /// @brief Base class for all exceptions
    class dbsync_error : public std::exception
    {
    public:
        /// @brief Returns the error message
        /// @returns the error message
        ATTR_RET_NONNULL
        const char* what() const noexcept override
        {
            return m_error.what();
        }

        /// @brief Returns the error id
        /// @return the error id
        int id() const noexcept
        {
            return m_id;
        }

        /// @brief Constructor
        /// @param id id of the error
        /// @param whatArg message of the error
        dbsync_error(const int id, const std::string& whatArg)
            : m_id {id}
            , m_error {whatArg}
        {
        }

        /// @brief Constructor
        /// @param exceptionInfo error information
        explicit dbsync_error(const std::pair<int, std::string>& exceptionInfo)
            : m_id {exceptionInfo.first}
            , m_error {exceptionInfo.second}
        {
        }

    private:
        /// an exception object as storage for error messages
        const int m_id;
        std::runtime_error m_error;
    };

    /// @brief Base class for all exceptions
    class max_rows_error : public std::exception
    {
    public:
        /// @brief Returns the error message
        /// @returns the error message
        ATTR_RET_NONNULL
        const char* what() const noexcept override
        {
            return m_error.what();
        }

        /// @brief Constructor
        /// @param whatArg message of the error
        explicit max_rows_error(const std::string& whatArg)
            : m_error {whatArg}
        {
        }

    private:
        /// an exception object as storage for error messages
        std::runtime_error m_error;
    };
} // namespace DbSync
