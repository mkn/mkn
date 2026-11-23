/**
Copyright (c) 2026, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef MAIKEN_ENV_HPP
#define MAIKEN_ENV_HPP

#include "mkn/kul/env.hpp"

#include "maiken/property.hpp"
#include "maiken/defs.hpp"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace maiken {

// env: entries, a list or one per line of a scalar
inline std::vector<YAML::Node> ENV_ENTRIES(YAML::Node const& env) {
  std::vector<YAML::Node> entries;
  if (!env) return entries;
  if (env.IsScalar())
    for (auto const& line : mkn::kul::String::LINES(env.Scalar())) entries.emplace_back(line);
  else
    for (auto const& entry : env) entries.push_back(entry);
  return entries;
}

// the variable an env: entry sets, NAME=value or name:
inline std::string ENV_NAME(YAML::Node const& entry) {
  auto name = entry.IsScalar() ? mkn::kul::String::ESC_SPLIT(entry.Scalar(), '=')[0]
                               : entry[Constants::STR_NAME].Scalar();
  mkn::kul::String::TRIM(name);
#if defined(_WIN32)
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });  // Path is PATH
#endif
  return name;
}

// true if an env: entry builds on its variable's value before it: mode prepend (the default) or
//  append, or NAME=value using $NAME or ${NAME}; else it replaces it
inline bool ENV_EXTENDS(YAML::Node const& entry) {
  if (!entry.IsScalar())
    return !entry[Constants::STR_MODE] ||
           entry[Constants::STR_MODE].Scalar() != Constants::STR_REPLACE;
  auto const bits = mkn::kul::String::ESC_SPLIT(entry.Scalar(), '=');
  if (bits.size() < 2) return false;
  std::string value = bits[1];
  for (std::size_t i = 2; i < bits.size(); ++i) value += "=" + bits[i];
  for (auto const& ref : {"$" + bits[0], "${" + bits[0] + "}"})
    if (Properties::FIND_UNESCAPED(value, ref) != std::string::npos) return true;
  return false;
}

// which env: entries of a chain of files are used, the first file's first: per variable those up
//  to the first that replaces it; within a file the last entry for a variable is the first
inline std::vector<std::vector<bool>> ENV_USED(std::vector<std::vector<YAML::Node>> const& chain) {
  std::vector<std::vector<bool>> used;
  std::unordered_set<std::string> replaced;
  for (auto const& entries : chain) {
    auto& u = used.emplace_back(entries.size(), true);
    for (std::size_t i = entries.size(); i-- > 0;) {
      auto const name = ENV_NAME(entries[i]);
      if (replaced.count(name))
        u[i] = false;
      else if (!ENV_EXTENDS(entries[i]))
        replaced.insert(name);
    }
  }
  return used;
}

template <typename HasProperties>
mkn::kul::env::Var PARSE_ENV_NODE(YAML::Node const& n, HasProperties const& hasProperties,
                                  std::string /*hasProperties_id*/) {
  using namespace mkn::kul;
  if (n.IsScalar()) {
    auto bits = mkn::kul::String::ESC_SPLIT(n.Scalar(), '=');

    for (std::size_t i = 2; i < bits.size(); ++i) bits[1] += "=" + bits[i];

    // every unescaped $NAME (the whole name: not $NAME_X) and ${NAME}, the value set before
    auto replace = [](std::string const& n, std::string& in, std::string const& f,
                      bool const whole) {
      std::string const value = mkn::kul::env::GET(n.c_str());
      for (auto pos = Properties::FIND_UNESCAPED(in, f); pos != std::string::npos;
           pos = Properties::FIND_UNESCAPED(in, f, pos)) {
        if (auto const end = pos + f.size();
            whole && end < in.size() &&
            (std::isalnum(static_cast<unsigned char>(in[end])) || in[end] == '_')) {
          pos = end;
          continue;
        }
        in.replace(pos, f.size(), value);
        pos += value.size();
      }
    };
    replace(bits[0], bits[1], "$" + bits[0], true);
    replace(bits[0], bits[1], "${" + bits[0] + "}", false);

    return env::Var(bits[0], Properties::RESOLVE(hasProperties, bits[1]), env::Var::Mode::REPL);
  }

  env::Var::Mode mode = env::Var::Mode::PREP;
  if (n[Constants::STR_MODE]) {
    if (n[Constants::STR_MODE].Scalar().compare(Constants::STR_APPEND) == 0)
      mode = env::Var::Mode::APPE;
    else if (n[Constants::STR_MODE].Scalar().compare(Constants::STR_PREPEND) == 0)
      mode = env::Var::Mode::PREP;
    else if (n[Constants::STR_MODE].Scalar().compare(Constants::STR_REPLACE) == 0)
      mode = env::Var::Mode::REPL;
  }
  return env::Var(n[Constants::STR_NAME].Scalar(),
                  Properties::RESOLVE(hasProperties, n[Constants::STR_VALUE].Scalar()), mode);
}

}  // namespace maiken

#endif /* MAIKEN_ENV_HPP */
