#pragma once

#include "cjsonSmartDeleter.hpp"
#include "db_exception.h"
#include "dbsync.h"
#include "testContext.hpp"
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>

namespace TestDeleters
{
    struct ResultDeleter final
    {
        void operator()(cJSON* json)
        {
            dbsync_free_result(&json);
        }
    };
}; // namespace TestDeleters

struct IAction
{
    virtual void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) = 0;

    virtual ~IAction() = default;
};

struct InsertDataAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value.at("body").dump().c_str())};

        const auto retVal {dbsync_insert_data(ctx->handle, jsInput.get())};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_insert_data", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct UpdateWithSnapshotAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> currentSnapshotPtr {cJSON_Parse(value["body"].dump().c_str())};
        cJSON* snapshotLambda {nullptr};

        if (0 == dbsync_update_with_snapshot(ctx->handle, currentSnapshotPtr.get(), &snapshotLambda))
        {
            // Create and flush snapshot diff data in files like: snapshot_<#idx>.json
            const std::unique_ptr<cJSON, TestDeleters::ResultDeleter> snapshotLambdaPtr(snapshotLambda);

            std::stringstream oFileName;
            oFileName << "action_" << ctx->currentId << ".json";
            const std::string outputFileName {ctx->outputPath + "/" + oFileName.str()};

            std::ofstream outputFile {outputFileName};
            const std::unique_ptr<char, CJsonSmartFree> snapshotDiff {cJSON_Print(snapshotLambdaPtr.get())};
            outputFile << snapshotDiff.get() << '\n';
        }
    }
};

static void dummyCallback(ReturnTypeCallback, const cJSON*, void*) {}

static void txnCallback(ReturnTypeCallback type, const cJSON* json, void* user_data)
{
    if (user_data && json)
    {
        static std::mutex s_mutex;
        std::lock_guard<std::mutex> lock {s_mutex};
        TestContext* ctx {reinterpret_cast<TestContext*>(user_data)};
        std::stringstream oFileName;
        oFileName << "txn_" << reinterpret_cast<size_t>(ctx->txnContext) << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};
        const std::unique_ptr<char, CJsonSmartFree> spJsonBytes {cJSON_PrintUnformatted(json)};
        const auto newJson {nlohmann::json::parse(spJsonBytes.get())};
        nlohmann::json jsonResult;
        jsonResult.push_back(newJson);
        jsonResult.push_back({{"result", type}});

        std::ofstream outputFile {outputFileName, std::ofstream::app};
        outputFile << jsonResult.dump() << '\n';
    }
}

struct CreateTransactionAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsonTables {
            cJSON_Parse(value["body"]["tables"].dump().c_str())};

        callback_data_t callbackData {txnCallback, ctx.get()};

        ctx->txnContext = dbsync_create_txn(ctx->handle, jsonTables.get(), 0, 100, callbackData);

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"txn_context", nullptr != ctx->txnContext}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct CloseTransactionAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& /*value*/) override
    {

        const auto retVal {dbsync_close_txn(ctx->txnContext)};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"txn_context", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SetMaxRowsAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const auto table {value["body"]["table"].get<std::string>()};
        const auto maxRows {value["body"]["max_rows"].get<unsigned int>()};
        const auto retVal {dbsync_set_table_max_rows(ctx->handle, table.c_str(), maxRows)};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_set_table_max_rows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct GetCallbackLogger final
{
    explicit GetCallbackLogger(const std::string& fileName)
        : m_fileName(fileName) {};
    std::string m_fileName;
    std::mutex m_mutex;
};

static void getCallbackCtx(ReturnTypeCallback /*type*/, const cJSON* json, void* user_data)
{
    GetCallbackLogger* loggerContext {reinterpret_cast<GetCallbackLogger*>(user_data)};

    std::lock_guard<std::mutex> lock(loggerContext->m_mutex);
    std::ifstream inputFile {loggerContext->m_fileName};
    nlohmann::json jsonResult {};

    if (inputFile.peek() != std::ifstream::traits_type::eof())
    {
        jsonResult = nlohmann::json::parse(inputFile);
    }

    const std::unique_ptr<char, CJsonSmartFree> spJsonBytes {cJSON_PrintUnformatted(json)};
    const auto& newJson {nlohmann::json::parse(spJsonBytes.get())};
    jsonResult.push_back(newJson);

    std::ofstream outputFile {loggerContext->m_fileName};
    outputFile << jsonResult.dump() << '\n';
};

struct GetDeletedRowsAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& /*value*/) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto& outputFileName {ctx->outputPath + "/" + oFileName.str()};
        const auto& outputFileNameCallback {ctx->outputPath + "/" + "callback." + oFileName.str()};

        const auto& loggerContext {std::make_unique<GetCallbackLogger>(outputFileNameCallback)};
        callback_data_t callbackData {getCallbackCtx, loggerContext.get()};

        const auto retVal {dbsync_get_deleted_rows(ctx->txnContext, callbackData)};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json& jsonResult {{"dbsync_get_deleted_rows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SyncRowAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value["body"].dump().c_str())};

        callback_data_t callbackData {dummyCallback, nullptr};

        const auto retVal {dbsync_sync_row(ctx->handle, jsInput.get(), callbackData)};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_sync_row", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SyncTxnRowsAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value["body"].dump().c_str())};

        const auto retVal {dbsync_sync_txn_row(ctx->txnContext, jsInput.get())};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_sync_txn_row", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct DeleteRowsAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value.at("body").dump().c_str())};

        const auto retVal {dbsync_delete_rows(ctx->handle, jsInput.get())};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_delete_rows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SelectRowsAction final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value.at("body").dump().c_str())};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};
        const auto& outputFileNameCallback {ctx->outputPath + "/" + "callback." + oFileName.str()};

        const auto& loggerContext {std::make_unique<GetCallbackLogger>(outputFileNameCallback)};
        callback_data_t callbackData {getCallbackCtx, loggerContext.get()};

        const auto retVal {dbsync_select_rows(ctx->handle, jsInput.get(), callbackData)};
        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_select_rows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct AddTableRelationship final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const std::unique_ptr<cJSON, CJsonSmartDeleter> jsInput {cJSON_Parse(value.at("body").dump().c_str())};

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        const auto retVal {dbsync_add_table_relationship(ctx->handle, jsInput.get())};
        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"dbsync_add_table_relationship", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct InsertDataCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->insertData(value.at("body"));
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"insertData", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct UpdateWithSnapshotActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        int retVal {0};
        nlohmann::json snapshotLambda {};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->updateWithSnapshot(value.at("body"), snapshotLambda);
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        snapshotLambda.push_back({"updateWithSnapshot", retVal});

        std::ofstream outputFile {outputFileName};
        outputFile << snapshotLambda.dump() << '\n';
    }
};

struct CreateTransactionActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        int retVal {0};

        auto txnCallback = [&ctx](ReturnTypeCallback type, const nlohmann::json& json)
        {
            static std::mutex s_mutex;
            std::lock_guard<std::mutex> lock {s_mutex};

            std::stringstream oFileName;
            oFileName << "txn_" << reinterpret_cast<size_t>(ctx->txnContext) << ".json";
            const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

            nlohmann::json jsonResult {};
            jsonResult.push_back(json);
            jsonResult.push_back({{"result", type}});

            std::ofstream outputFile {outputFileName, std::ofstream::app};
            outputFile << jsonResult.dump() << '\n';
        };

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            std::unique_ptr<DBSyncTxn> dbSyncTxn {
                std::make_unique<DBSyncTxn>(dbSync->handle(), value.at("body").at("tables"), 0, 100, txnCallback)};

            ctx->txnContext = dbSyncTxn->handle();
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"createTransaction", nullptr != ctx->txnContext && 0 == retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SetMaxRowsActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        const auto table {value.at("body").at("table").get<std::string>()};
        const auto maxRows {value.at("body").at("max_rows").get<unsigned int>()};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->setTableMaxRow(table, maxRows);
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"SetMaxRowsAction", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct AddTableRelationshipCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->addTableRelationship(value.at("body"));
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"addTableRelationship", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct GetDeletedRowsActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& /*value*/) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto& outputFileNameCallback {ctx->outputPath + "/" + "callback." + oFileName.str()};
        const auto& loggerContext {std::make_unique<GetCallbackLogger>(outputFileNameCallback)};

        auto callbackDelete {[&loggerContext](ReturnTypeCallback /*result_type*/, const nlohmann::json& json)
                             {
                                 std::lock_guard<std::mutex> lock(loggerContext->m_mutex);

                                 std::ifstream inputFile {loggerContext->m_fileName};

                                 nlohmann::json jsonResult {};

                                 if (inputFile.peek() != std::ifstream::traits_type::eof())
                                 {
                                     jsonResult = nlohmann::json::parse(inputFile);
                                 }

                                 jsonResult.push_back(json);

                                 std::ofstream outputFile {loggerContext->m_fileName};
                                 outputFile << jsonResult.dump() << '\n';
                             }};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSyncTxn> dbSyncTxn {std::make_unique<DBSyncTxn>(ctx->txnContext)};
            dbSyncTxn->getDeletedRows(callbackDelete);
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        const auto& outputFileName {ctx->outputPath + "/" + oFileName.str()};
        std::ofstream outputFile {outputFileName};
        const nlohmann::json& jsonResult {{"getDeletedRows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SyncRowActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto& outputFileNameCallback {ctx->outputPath + "/" + "callback." + oFileName.str()};
        const auto& loggerContext {std::make_unique<GetCallbackLogger>(outputFileNameCallback)};

        auto callbackSync {[&loggerContext](ReturnTypeCallback /*result_type*/, const nlohmann::json& json)
                           {
                               std::lock_guard<std::mutex> lock(loggerContext->m_mutex);
                               std::ifstream inputFile {loggerContext->m_fileName};

                               nlohmann::json jsonResult {};

                               if (inputFile.peek() != std::ifstream::traits_type::eof())
                               {
                                   jsonResult = nlohmann::json::parse(inputFile);
                               }

                               jsonResult.push_back(json);

                               std::ofstream outputFile {loggerContext->m_fileName};
                               outputFile << jsonResult.dump() << '\n';
                           }};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->syncRow(value.at("body"), callbackSync);
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"syncRow", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SyncTxnRowsActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        int retVal {0};

        try
        {
            std::unique_ptr<DBSyncTxn> dbSyncTxn {std::make_unique<DBSyncTxn>(ctx->txnContext)};
            dbSyncTxn->syncTxnRow(value.at("body"));
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"syncTxnRow", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct DeleteRowsActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->deleteRows(value.at("body"));
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto outputFileName {ctx->outputPath + "/" + oFileName.str()};

        std::ofstream outputFile {outputFileName};
        const nlohmann::json jsonResult = {{"deleteRows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};

struct SelectRowsActionCPP final : public IAction
{
    void execute(std::unique_ptr<TestContext>& ctx, const nlohmann::json& value) override
    {
        std::stringstream oFileName;
        oFileName << "action_" << ctx->currentId << ".json";
        const auto& outputFileNameCallback {ctx->outputPath + "/" + "callback." + oFileName.str()};
        const auto& loggerContext {std::make_unique<GetCallbackLogger>(outputFileNameCallback)};

        auto callbackSelect {[&loggerContext](ReturnTypeCallback /*result_type*/, const nlohmann::json& json)
                             {
                                 std::lock_guard<std::mutex> lock(loggerContext->m_mutex);
                                 std::ifstream inputFile {loggerContext->m_fileName};

                                 nlohmann::json jsonResult {};

                                 if (inputFile.peek() != std::ifstream::traits_type::eof())
                                 {
                                     jsonResult = nlohmann::json::parse(inputFile);
                                 }

                                 jsonResult.push_back(json);

                                 std::ofstream outputFile {loggerContext->m_fileName};
                                 outputFile << jsonResult.dump() << '\n';
                             }};

        int retVal {0};

        try
        {
            std::unique_ptr<DBSync> dbSync {std::make_unique<DBSync>(ctx->handle)};
            dbSync->selectRows(value.at("body"), callbackSelect);
        }
        catch (const nlohmann::detail::exception& ex)
        {
            retVal = ex.id;
        }
        catch (const DbSync::dbsync_error& ex)
        {
            retVal = ex.id();
        }
        catch (...) // NOLINT(bugprone-empty-catch)
        {
            retVal = -1;
        }

        std::ofstream outputFile {ctx->outputPath + "/" + oFileName.str()};
        const nlohmann::json jsonResult = {{"selectRows", retVal}};
        outputFile << jsonResult.dump() << '\n';
    }
};
