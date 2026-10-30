// SPDX-License-Identifier: MIT
#pragma once
#include "installer-ui.generated.h"
#include <regex>
#include <string>

namespace InstallerUI {
enum class Action { Install, Uninstall };
inline constexpr Action primaryAction=Action::Install;
inline std::string trim(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    return first==std::string::npos ? std::string{} : value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);
}
// Windows embeds semantic metadata; macOS stores the internal BUILD separately.
// Both render the same human-facing internal identifier, never +internal.*.
inline std::string version(std::string value,std::string build={}) {
    value=trim(value); build=trim(build);
    std::smatch match;
    static const std::regex internal(R"(.*\+internal\.(\d{4})(\d{2})(\d{2})\.(\d+)$)");
    static const std::regex dated(R"((\d{4})\.(\d{2})\.(\d{2})\.(\d+))");
    if(std::regex_match(value,match,internal) || std::regex_match(build,match,dated) || std::regex_match(value,match,dated)) {
        auto counter=match[4].str();
        const auto first=counter.find_first_not_of('0');
        counter=first==std::string::npos ? "0" : counter.substr(first);
        if(counter.size()<3) counter.insert(0,3-counter.size(),'0');
        return match[1].str()+"."+match[2].str()+"."+match[3].str()+"."+counter;
    }
    return value.empty() ? text::unknownVersion : value;
}
struct State { bool installed, running, sameBuild; };
struct Presentation {
    std::string install, uninstall;
    const char *status;
    bool installEnabled, uninstallEnabled;
};
inline Presentation present(State s,const std::string &installedVersion,const std::string &bundledVersion) {
    return {std::string(s.installed ? text::update : text::install)+"\n"+bundledVersion,
        std::string(text::uninstall)
            +(s.installed ? "\n"+installedVersion : ""),
        s.running ? text::running : s.installed ? text::installed : text::ready,
        !s.running && !s.sameBuild, !s.running && s.installed};
}
struct Operation {
    bool uninstall, installed;
    const char *progress() const {
        return uninstall ? text::uninstalling
            : installed ? text::updating : text::installing;
    }
    const char *success() const {
        return uninstall ? text::removedSuccess
            : installed ? text::updatedSuccess : text::installedSuccess;
    }
    const char *failure() const {
        return uninstall ? text::uninstallFailed
            : installed ? text::updateFailed : text::installFailed;
    }
    const char *confirmationTitle() const { return text::confirmUninstall; }
    const char *confirmationText() const {
        return text::confirmKeep;
    }
};
}
