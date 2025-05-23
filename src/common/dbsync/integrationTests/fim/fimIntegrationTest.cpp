#include "fimIntegrationTest.hpp"
#include "cjsonSmartDeleter.hpp"
#include "dbsync.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using ::testing::_; // NOLINT(bugprone-reserved-identifier)

constexpr auto DATABASE_TEMP {"FIM_TEMP.db"};

class CallbackMock
{
public:
    CallbackMock() = default;
    MOCK_METHOD(void, callbackMock, (ReturnTypeCallback result_type, const nlohmann::json&), ());
};

static void Callback(const ReturnTypeCallback type, const cJSON* json, void* ctx)
{
    CallbackMock* wrapper {static_cast<CallbackMock*>(ctx)};
    const std::unique_ptr<char, CJsonSmartFree> spJsonBytes {cJSON_PrintUnformatted(json)};
    wrapper->callbackMock(type, nlohmann::json::parse(spJsonBytes.get()));
}

static std::string LoadDbQueriesDump()
{
    const std::string inputFile = std::string(TEST_DB_INPUTS_DIR) + "/" + "fim_sql_db_dump.sql";
    const std::ifstream file(inputFile);
    if (!file.is_open())
    {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Remove the blank lines at the end so that dbsync_create does not fail.
    const size_t pos = content.find_last_not_of("\n\r");
    if (pos != std::string::npos)
    {
        content = content.substr(0, pos + 1);
    }

    return content;
}

DBSyncFimIntegrationTest::DBSyncFimIntegrationTest()
    : m_dbHandle {dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, LoadDbQueriesDump().c_str())}

{
}

DBSyncFimIntegrationTest::~DBSyncFimIntegrationTest() // NOLINT(bugprone-exception-escape)
{
    EXPECT_NO_THROW(dbsync_teardown());
    std::remove(DATABASE_TEMP);
}

void DBSyncFimIntegrationTest::SetUp() {};

void DBSyncFimIntegrationTest::TearDown() {};

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_PATH)
{
    const auto expectedResult {
        R"({"checksum":"a2fbef8f81af27155dcee5e3927ff6243593b91a",
            "dev":2051,
            "entry_type":0,
            "gid":0,
            "group_name":"root",
            "hash_md5":"4b531524aa13c8a54614100b570b3dc7",
            "hash_sha1":"7902feb66d0bcbe4eb88e1bfacf28befc38bd58b",
            "hash_sha256":"e403b83dd73a41b286f8db2ee36d6b0ea6e80b49f02c476e0a20b4181a3a062a",
            "inode":18277083,
            "inode_id":1877,
            "last_event":1596489275,
            "mode":0,
            "mtime":1578075431,
            "options":131583,
            "path":"/etc/wgetrc",
            "perm":"rw-r--r--",
            "scanned":0,
            "size":4925,
            "uid":0,
            "user_name":"root"}
            )"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["path, inode_id, mode, last_event, entry_type, scanned, options, checksum, dev, inode, size, perm, attributes, uid, gid, user_name, group_name, hash_md5, hash_sha1, hash_sha256, mtime"],
           "row_filter":"INNER JOIN entry_data ON path ='/etc/wgetrc' AND entry_data.rowid = entry_path.inode_id",
           "distinct_opt":false,
           "order_by_opt":"",
           "count_opt":100}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_LAST_PATH)
{
    const auto expectedResult {R"({"path":"/sbin"})"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["path"],
           "row_filter":"",
           "distinct_opt":false,
           "order_by_opt":"path DESC",
           "count_opt":1}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_FIRST_PATH)
{
    const auto expectedResult {R"({"path":"/bin"})"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["path"],
           "row_filter":"",
           "distinct_opt":false,
           "order_by_opt":"path ASC",
           "count_opt":1}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_ALL_ENTRIES)
{
    const auto selectSql {
        R"({"table":"entry_data",
           "query":{"column_list":["path,
                                    inode_id,
                                    mode,
                                    last_event,
                                    entry_type,
                                    scanned,
                                    options,
                                    checksum,
                                    dev,
                                    inode,
                                    size,
                                    perm,
                                    attributes,
                                    uid,
                                    gid,
                                    user_name,
                                    group_name,
                                    hash_md5,
                                    hash_sha1,
                                    hash_sha256,
                                    mtime"],
           "row_filter":"INNER JOIN entry_path ON inode_id = entry_data.rowid",
           "distinct_opt":false,
           "order_by_opt":"path ASC"}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, _)).Times(1904); // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_COUNT_RANGE)
{
    const auto expectedResult {"{\"count(*)\":13}"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["count(*) "],
           "row_filter":"INNER JOIN entry_data ON entry_data.rowid = entry_path.inode_id WHERE path BETWEEN '/etc/yum.conf' and '/etc/yum.repos.d/CentOS-centosplus.repo' ORDER BY path",
           "distinct_opt":false,
           "order_by_opt":""}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_PATH_RANGE)
{
    const auto expectedResult1 {
        R"({"checksum":"e100589b9f75b293ea2fc718fb39ecedddf1f381",
            "dev":2051,"entry_type":0,"gid":0,"group_name":"root",
            "hash_md5":"d41d8cd98f00b204e9800998ecf8427e",
            "hash_sha1":"da39a3ee5e6b4b0d3255bfef95601890afd80709",
            "hash_sha256":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "inode":18277013,"inode_id":1871,"last_event":1596489275,"mode":0,
            "mtime":1587758264,"options":131583,"path":"/etc/yum.conf",
            "perm":"rwxrwxrwx","scanned":0,"size":12,"uid":0,"user_name":"root"})"};
    const auto expectedResult2 {
        R"({"checksum":"eeb46d0e85f635cd8595afc3447b21686c8fedb3",
           "dev":2051,"entry_type":0,"gid":0,"group_name":"root",
           "hash_md5":"349e00330684b1b1443904956aa0b241",
           "hash_sha1":"f945fe1ad48aa9c367d2a131a4f7a659db6c1967",
           "hash_sha256":"0e3a78178a75c13d71cfc2fafb3072a009733414a90802b2b67ccc7279e050cd",
           "inode":2078,"inode_id":604,"last_event":1596489275,"mode":0,
           "mtime":1591146169,"options":131583,"path":"/etc/yum.repos.d/CentOS-AppStream.repo",
           "perm":"rw-r--r--","scanned":1,"size":731,"uid":0,"user_name":"root"})"};
    const auto expectedResult3 {
        R"({"checksum":"e24f1dfcba64d3dea78c6840893c77539f44638f",
           "dev":2051,"entry_type":0,"gid":0,"group_name":"root",
           "hash_md5":"7449031222431c7cbac19313af55aca4",
           "hash_sha1":"640746d2388b9500b300e2a45878e81e5473aa83",
           "hash_sha256":"ee7da6f7be6623cc6da7613777def9c9801073d725f686eb4e3812584e3e417d",
           "inode":2079,"inode_id":605,"last_event":1596489275,"mode":0,
           "mtime":1591146169,"options":131583,"path":"/etc/yum.repos.d/CentOS-Base.repo",
           "perm":"rw-r--r--","scanned":1,"size":712,"uid":0,"user_name":"root"})"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["path,
                                    inode_id,
                                    mode,
                                    last_event,
                                    entry_type,
                                    scanned,
                                    options,
                                    checksum,
                                    dev,
                                    inode,
                                    size,
                                    perm,
                                    attributes,
                                    uid,
                                    gid,
                                    user_name,
                                    group_name,
                                    hash_md5,
                                    hash_sha1,
                                    hash_sha256,
                                    mtime"],
           "row_filter":"INNER JOIN entry_data ON entry_data.rowid = entry_path.inode_id WHERE path BETWEEN '/etc/yum.conf' and '/etc/yum.repos.d/CentOS-Base.repo' ORDER BY path",
           "distinct_opt":false,
           "order_by_opt":""}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult1))).Times(1);
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult2))).Times(1);
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult3))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_PATHS_INODE)
{
    const auto expectedResult {R"({"path":"/etc/yum.repos.d/CentOS-Base.repo"})"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["path"],
           "row_filter":"INNER JOIN entry_data ON entry_data.rowid=entry_path.inode_id WHERE entry_data.inode=2079 AND entry_data.dev=2051",
           "distinct_opt":false,
           "order_by_opt":""}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_PATHS_INODE_COUNT)
{
    const auto expectedResult {"{\"count(*)\":1}"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["count(*) "],
           "row_filter":"INNER JOIN entry_data ON entry_data.rowid=entry_path.inode_id WHERE entry_data.inode=2078 AND entry_data.dev=2051",
           "distinct_opt":false,
           "order_by_opt":""}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}

TEST_F(DBSyncFimIntegrationTest, FIMDB_STMT_GET_COUNT_PATH)
{
    const auto expectedResult {"{\"count(*)\":1906}"};
    const auto selectSql {
        R"({"table":"entry_path",
           "query":{"column_list":["count(*) "],
           "row_filter":"",
           "distinct_opt":false,
           "order_by_opt":""}})"};
    const std::unique_ptr<cJSON, CJsonSmartDeleter> jsSelect {cJSON_Parse(selectSql)};
    CallbackMock wrapper;
    const callback_data_t callbackData {Callback, &wrapper};
    EXPECT_CALL(wrapper, callbackMock(SELECTED, nlohmann::json::parse(expectedResult))).Times(1);
    EXPECT_EQ(0, dbsync_select_rows(m_dbHandle, jsSelect.get(), callbackData));
}
