#include <communicator.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace
{
    constexpr auto MIN_BATCH_SIZE = 1000ULL;
    constexpr auto MAX_BATCH_SIZE = 100000000ULL;
} // namespace

namespace communicator
{
    constexpr int TOKEN_PRE_EXPIRY_SECS = 2;
    constexpr int A_SECOND_IN_MILLIS = 1000;

    Communicator::Communicator(std::unique_ptr<http_client::IHttpClient> httpClient,
                               std::shared_ptr<configuration::ConfigurationParser> configurationParser,
                               std::string uuid,
                               std::string key,
                               std::function<std::string()> getHeaderInfo)
        : m_httpClient(std::move(httpClient))
        , m_uuid(std::move(uuid))
        , m_key(std::move(key))
        , m_getHeaderInfo(std::move(getHeaderInfo))
        , m_token(std::make_shared<std::string>())
    {
        if (!m_httpClient)
        {
            throw std::runtime_error(std::string("Invalid HTTP Client passed."));
        }

        if (!configurationParser)
        {
            throw std::runtime_error(std::string("Invalid Configuration Parser passed."));
        }

        m_serverUrl = configurationParser->GetConfig<std::string>("agent", "server_url")
                          .value_or(config::agent::DEFAULT_SERVER_URL);

        if (boost::urls::url_view url(m_serverUrl); url.scheme() != "https")
        {
            LogInfo("Using insecure connection.");
        }

        m_retryInterval = configurationParser->GetConfig<std::time_t>("agent", "retry_interval")
                              .value_or(config::agent::DEFAULT_RETRY_INTERVAL);

        if (m_retryInterval < 0)
        {
            LogWarn("retry_interval must be greater than or equal to 0. Using default value.");
            m_retryInterval = config::agent::DEFAULT_RETRY_INTERVAL;
        }

        m_batchSize =
            configurationParser->GetConfig<size_t>("events", "batch_size").value_or(config::agent::DEFAULT_BATCH_SIZE);

        if (m_batchSize < MIN_BATCH_SIZE || m_batchSize > MAX_BATCH_SIZE)
        {
            LogWarn("batch_size must be between 1KB and 100MB. Using default value.");
            m_batchSize = config::agent::DEFAULT_BATCH_SIZE;
        }

        m_verificationMode = configurationParser->GetConfig<std::string>("agent", "verification_mode")
                                 .value_or(config::agent::DEFAULT_VERIFICATION_MODE);

        if (std::find(std::begin(config::agent::VALID_VERIFICATION_MODES),
                      std::end(config::agent::VALID_VERIFICATION_MODES),
                      m_verificationMode) == std::end(config::agent::VALID_VERIFICATION_MODES))
        {
            LogWarn("Incorrect value for 'verification_mode', in case of HTTPS connections the default value '{}' "
                    "is used.",
                    config::agent::DEFAULT_VERIFICATION_MODE);
            m_verificationMode = config::agent::DEFAULT_VERIFICATION_MODE;
        }
    }

    boost::beast::http::status Communicator::SendAuthenticationRequest()
    {
        const auto token = m_httpClient->AuthenticateWithUuidAndKey(
            m_serverUrl, m_getHeaderInfo ? m_getHeaderInfo() : "", m_uuid, m_key, m_verificationMode);

        if (token.has_value())
        {
            *m_token = token.value();
            LogInfo("Successfully authenticated with the manager.");
        }
        else
        {
            LogWarn("Failed to authenticate with the manager. Retrying in {} seconds.",
                    m_retryInterval / A_SECOND_IN_MILLIS);
            return boost::beast::http::status::unauthorized;
        }

        try
        {
            const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(*m_token);

            if (!decoded.has_payload_claim("exp"))
            {
                throw std::runtime_error("Token does not contain an 'exp' claim.");
            }

            const auto exp_claim = decoded.get_payload_claim("exp");
            const auto exp_time = exp_claim.as_date();
            m_tokenExpTimeInSeconds =
                std::chrono::duration_cast<std::chrono::seconds>(exp_time.time_since_epoch()).count();
        }
        catch (const std::exception& e)
        {
            LogError("Failed to decode token: {}", e.what());
            m_token->clear();
            m_tokenExpTimeInSeconds = 1;
            return boost::beast::http::status::unauthorized;
        }

        return boost::beast::http::status::ok;
    }

    long Communicator::GetTokenRemainingSecs() const
    {
        const auto now = std::chrono::system_clock::now();
        const auto now_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return std::max(0L, static_cast<long>(m_tokenExpTimeInSeconds - now_seconds));
    }

    boost::asio::awaitable<void>
    Communicator::GetCommandsFromManager(std::function<void(const int, const std::string&)> onSuccess)
    {
        auto onAuthenticationFailed = [this]()
        {
            TryReAuthenticate();
        };

        auto loopCondition = [this]()
        {
            return m_keepRunning.load();
        };

        const auto reqParams = http_client::HttpRequestParams(boost::beast::http::verb::get,
                                                              m_serverUrl,
                                                              "/api/v1/commands",
                                                              m_getHeaderInfo ? m_getHeaderInfo() : "",
                                                              m_verificationMode);
        co_await m_httpClient->Co_PerformHttpRequest(
            m_token, reqParams, {}, onAuthenticationFailed, m_retryInterval, m_batchSize, onSuccess, loopCondition);
    }

    boost::asio::awaitable<void> Communicator::WaitForTokenExpirationAndAuthenticate()
    {
        using namespace std::chrono_literals;
        const auto executor = co_await boost::asio::this_coro::executor;
        m_tokenExpTimer = std::make_unique<boost::asio::steady_timer>(executor);

        if (auto remainingSecs = GetTokenRemainingSecs(); remainingSecs > TOKEN_PRE_EXPIRY_SECS)
        {
            m_tokenExpTimer->expires_after(
                std::chrono::milliseconds((remainingSecs - TOKEN_PRE_EXPIRY_SECS) * A_SECOND_IN_MILLIS));
            co_await m_tokenExpTimer->async_wait(boost::asio::use_awaitable);
        }

        while (m_keepRunning.load())
        {
            const auto duration = [this]()
            {
                try
                {
                    const auto result = SendAuthenticationRequest();
                    if (result != boost::beast::http::status::ok)
                    {
                        return std::chrono::milliseconds(m_retryInterval);
                    }
                    else
                    {
                        return std::chrono::milliseconds((GetTokenRemainingSecs() - TOKEN_PRE_EXPIRY_SECS) *
                                                         A_SECOND_IN_MILLIS);
                    }
                }
                catch (const std::exception&)
                {
                    return std::chrono::milliseconds(m_retryInterval);
                }
            }();

            m_tokenExpTimer->expires_after(duration);

            boost::system::error_code ec;
            co_await m_tokenExpTimer->async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));

            if (ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                {
                    LogDebug("Token expiration timer was canceled.");
                }
                else
                {
                    LogDebug("Timer wait failed: {}.", ec.message());
                }
            }
        }
    }

    boost::asio::awaitable<void> Communicator::StatefulMessageProcessingTask(
        std::function<boost::asio::awaitable<std::tuple<int, std::string>>(const size_t)> getMessages,
        std::function<void(const int, const std::string&)> onSuccess)
    {
        auto onAuthenticationFailed = [this]()
        {
            TryReAuthenticate();
        };

        auto loopCondition = [this]()
        {
            return m_keepRunning.load();
        };

        const auto reqParams = http_client::HttpRequestParams(boost::beast::http::verb::post,
                                                              m_serverUrl,
                                                              "/api/v1/events/stateful",
                                                              m_getHeaderInfo ? m_getHeaderInfo() : "",
                                                              m_verificationMode);
        co_await m_httpClient->Co_PerformHttpRequest(m_token,
                                                     reqParams,
                                                     getMessages,
                                                     onAuthenticationFailed,
                                                     m_retryInterval,
                                                     m_batchSize,
                                                     onSuccess,
                                                     loopCondition);
    }

    boost::asio::awaitable<void> Communicator::StatelessMessageProcessingTask(
        std::function<boost::asio::awaitable<std::tuple<int, std::string>>(const size_t)> getMessages,
        std::function<void(const int, const std::string&)> onSuccess)
    {
        auto onAuthenticationFailed = [this]()
        {
            TryReAuthenticate();
        };

        auto loopCondition = [this]()
        {
            return m_keepRunning.load();
        };

        const auto reqParams = http_client::HttpRequestParams(boost::beast::http::verb::post,
                                                              m_serverUrl,
                                                              "/api/v1/events/stateless",
                                                              m_getHeaderInfo ? m_getHeaderInfo() : "",
                                                              m_verificationMode);
        co_await m_httpClient->Co_PerformHttpRequest(m_token,
                                                     reqParams,
                                                     getMessages,
                                                     onAuthenticationFailed,
                                                     m_retryInterval,
                                                     m_batchSize,
                                                     onSuccess,
                                                     loopCondition);
    }

    void Communicator::TryReAuthenticate()
    {
        std::unique_lock<std::mutex> lock(m_reAuthMutex, std::try_to_lock);
        if (lock.owns_lock() && !m_isReAuthenticating.exchange(true))
        {
            if (m_tokenExpTimer)
            {
                m_tokenExpTimer->cancel();
            }
            m_isReAuthenticating = false;
        }
        else
        {
            const std::thread::id threadId = std::this_thread::get_id();
            std::ostringstream oss;
            oss << threadId;
            std::string threadIdStr = oss.str();
            LogDebug("Re-authentication attempt by thread {} failed.", threadIdStr);
        }
    }

    boost::asio::awaitable<bool> Communicator::GetGroupConfigurationFromManager(std::string groupName,
                                                                                std::string dstFilePath)
    {
        auto onAuthenticationFailed = [this]()
        {
            TryReAuthenticate();
        };

        bool downloaded = false;

        auto onSuccess = [path = std::move(dstFilePath), &downloaded](const int, const std::string& res)
        {
            std::ofstream file(path, std::ios::binary);
            if (file)
            {
                file << res;
                file.close();
                downloaded = true;
            }
        };

        const auto reqParams = http_client::HttpRequestParams(boost::beast::http::verb::get,
                                                              m_serverUrl,
                                                              "/api/v1/files?file_name=" + groupName +
                                                                  config::DEFAULT_SHARED_FILE_EXTENSION,
                                                              m_getHeaderInfo ? m_getHeaderInfo() : "",
                                                              m_verificationMode,
                                                              *m_token);

        co_await m_httpClient->Co_PerformHttpRequest(
            m_token, reqParams, {}, onAuthenticationFailed, m_retryInterval, m_batchSize, onSuccess, {});

        co_return downloaded;
    }

    void Communicator::Stop()
    {
        m_keepRunning.store(false);
    }
} // namespace communicator
