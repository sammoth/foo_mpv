#pragma once

#include <regex>
#include "../acfu.h"

namespace acfu {

class version_error: public std::logic_error {
 public:
  version_error(): std::logic_error("invalid version string format") {}
};

bool is_newer(const char* available, const char* installed);
pfc::list_t<int> parse_version(std::string version);

inline bool is_newer(const char* available, const char* installed) {
  if (available && installed) {
    try {
      auto available_parts = parse_version(available);
      auto installed_parts = parse_version(installed);
      for (size_t i = 0; i < available_parts.get_size(); i ++) {
        if (int diff = available_parts[i] - installed_parts[i]) {
          return diff > 0;
        }
      }
    }
    catch (version_error&) {
    }
  }
  return false;
}

inline pfc::list_t<int> parse_version(std::string version) {
  const char* rule = "\\s*v?(\\d+)\\.(\\d+)(?:\\.(\\d+)(?:\\.(\\d+))?)?(?:[\\s-](alpha|beta|rc)[\\s\\.-]?(\\d+)?)?\\s*";
  std::regex version_regex(rule, std::regex::icase);
  std::smatch match;
  if (!std::regex_match(version, match, version_regex)) {
    throw version_error();
  }

  pfc::list_t<int> parts;
  parts.set_count(6);

  parts[0] = atoi(match[1].str().c_str());
  parts[1] = atoi(match[2].str().c_str());
  parts[2] = atoi(match[3].str().c_str());
  parts[3] = atoi(match[4].str().c_str());

  auto prerelease = match[5].str();
  if (0 == pfc::stricmp_ascii("rc", prerelease.c_str())) {
    parts[4] = -1;
  }
  else if (0 == pfc::stricmp_ascii("beta", prerelease.c_str())) {
    parts[4] = -2;
  }
  else if (0 == pfc::stricmp_ascii("alpha", prerelease.c_str())) {
    parts[4] = -3;
  }
  else {
    parts[4] = 0;
  }

  parts[5] = atoi(match[6].str().c_str());

  return parts;
}

} // namespace acfu
