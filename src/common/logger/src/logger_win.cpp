#include <logger.hpp>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/win_eventlog_sink.h>

#include <memory>

Logger::Logger()
{
    auto sink = std::make_shared<spdlog::sinks::win_eventlog_sink_st>("Wazuh-Agent");
    auto logger = std::make_shared<spdlog::logger>("wazuh-agent", sink);

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::cfg::load_env_levels();
}
