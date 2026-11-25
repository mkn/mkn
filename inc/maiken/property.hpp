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
#ifndef MAIKEN_PROPERTY_HPP
#define MAIKEN_PROPERTY_HPP

#include "maiken/app.hpp"
#include "maiken/settings.hpp"
#include "maiken/defs.hpp"
#include "mkn/kul/except.hpp"
#include "mkn/kul/map.hpp"
#include "yaml-cpp/yaml.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

namespace maiken {
class Application;
class Settings;

class Properties : public Constants {
 private:
  static std::shared_ptr<std::tuple<std::string, int, int>> KEY(mkn::kul::hash::map::S2S const& ps,
                                                                std::string const& s)
      KTHROW(mkn::kul::Exception);

 public:
  static std::string RESOLVE(Application const& app, std::string const& s)
      KTHROW(mkn::kul::Exception);
  static std::string RESOLVE(Settings const& app, std::string const& s) KTHROW(mkn::kul::Exception);

  // the first f in s from pos not preceded by a backslash, or npos
  static std::size_t FIND_UNESCAPED(std::string const& s, std::string const& f,
                                    std::size_t pos = 0);
  // the value of a property maiken sets itself (mkn.bin.key, mkn.with.bin, HOME, OS, ...), which
  //  a property: using its own name extends when nothing before it sets it
  static std::optional<std::string> BUILTIN(std::string const& key);
  // mkn.sys.* and mkn.user.*: built-ins nothing sets, HOME, OS, ARCH, MKN_HOME, MKN_REPO and
  //  MKN_MOD_REPO default to them and can be set
  static bool FIXED(std::string const& key);
  // an error naming where if key is FIXED
  static void NOT_FIXED(std::string const& key, std::string const& where)
      KTHROW(mkn::kul::Exception);
  // what ${k} is in a project: -P, the project's own (its super:'s and the settings' with them),
  //  then a built-in; maiken's settings keys as the settings set them
  static std::optional<std::string> PROJECT_LOOKUP(Application const& app, std::string const& k);
  // true if value uses ${key}, so extends the value of key it overrides
  static bool EXTENDS(std::string const& key, std::string const& value);
  // value with each ${key} replaced by the value key has before it, in ps: from an earlier
  //  file, a super: file or the settings, else its BUILTIN value; an error if there is none
  static std::string EXTEND(std::string const& key, std::string value,
                            mkn::kul::hash::map::S2S const& ps, std::string const& file)
      KTHROW(mkn::kul::Exception);
  // maiken's own settings file properties (mkn.with.bin, mkn.flags.<part>, ...): set with -X, not
  // -P
  static bool SETTINGS_KEY(std::string const& key);
  // a property: node's own entries over ps, which holds those it extends; one in over (-P, -X)
  //  is its value there as it is, the file's never read
  static void RESOLVE_OWN(YAML::Node const& node, mkn::kul::hash::map::S2S& ps,
                          mkn::kul::hash::map::S2S const& over, std::string const& file,
                          auto const& resolve) KTHROW(mkn::kul::Exception);
};

void Properties::RESOLVE_OWN(YAML::Node const& node, mkn::kul::hash::map::S2S& ps,
                             mkn::kul::hash::map::S2S const& over, std::string const& file,
                             auto const& resolve) KTHROW(mkn::kul::Exception) {
  ps.setDeletedKey("--DELETED--");
  for (auto it = node.begin(); it != node.end(); ++it) {
    auto const k = it->first.as<std::string>();
    ps[k] = over.count(k) ? over.at(k) : EXTEND(k, it->second.as<std::string>(), ps, file);
  }
  for (auto it = node.begin(); it != node.end(); ++it) {
    auto const k = it->first.as<std::string>();
    if (over.count(k)) continue;
    auto const s = resolve(ps.at(k));
    ps.erase(k);
    ps[k] = s;
  }
}

}  // namespace maiken

#endif  // MAIKEN_PROPERTY_HPP