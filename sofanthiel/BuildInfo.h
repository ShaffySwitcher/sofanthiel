#pragma once

#include <cstring>
#include <string>

#ifndef SOFANTHIEL_APP_VERSION
#define SOFANTHIEL_APP_VERSION "unknown"
#endif

#ifndef SOFANTHIEL_BUILD_CHANNEL
#define SOFANTHIEL_BUILD_CHANNEL "n/a"
#endif

#ifndef SOFANTHIEL_GIT_COMMIT
#define SOFANTHIEL_GIT_COMMIT "n/a"
#endif

#ifndef SOFANTHIEL_BUILD_DATE
#define SOFANTHIEL_BUILD_DATE __DATE__ " " __TIME__
#endif

#ifndef SOFANTHIEL_REPOSITORY_URL
#define SOFANTHIEL_REPOSITORY_URL "https://github.com/ShaffySwitcher/sofanthiel"
#endif

namespace BuildInfo {
static const char* const kAppName = "Sofanthiel";
static const char* const kVersion = SOFANTHIEL_APP_VERSION;
static const char* const kBuildChannel = SOFANTHIEL_BUILD_CHANNEL;
static const char* const kGitCommit = SOFANTHIEL_GIT_COMMIT;
static const char* const kBuildDate = SOFANTHIEL_BUILD_DATE;
static const char* const kRepositoryUrl = SOFANTHIEL_REPOSITORY_URL;

inline bool isStableChannel()
{
    return std::strcmp(kBuildChannel, "stable") == 0;
}

inline std::string displayVersion()
{
    if (isStableChannel()) {
        return kVersion;
    }

    std::string display = kVersion;
    display += " [";
    display += kBuildChannel;
    display += "]";
    return display;
}
}