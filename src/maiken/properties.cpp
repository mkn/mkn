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

#include "maiken/app.hpp"
#include "maiken/defs.hpp"
#include "maiken/global.hpp"
#include "maiken/project.hpp"
#include "maiken/property.hpp"
#include "maiken/settings.hpp"
#include "mkn/kul/except.hpp"
#include "mkn/kul/map.hpp"
#include "mkn/kul/string.hpp"
#include "yaml-cpp/yaml.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <variant>

maiken::Application& maiken::Application::resolveProperties() KTHROW(maiken::Exception) {
  auto const& node = project().root()[STR_PROPERTY];
  static std::unordered_set<std::string> warned;  // once per project file and property
  for (auto it = node.begin(); it != node.end(); ++it) {
    auto const k = it->first.as<std::string>();
    Properties::NOT_FIXED(k, project().file());
    // maiken's own settings file properties are a settings file's, -P says so, this warns
    if (Properties::SETTINGS_KEY(k) && warned.insert(project().file() + ":" + k).second)
      KERR << "WARNING: " << project().file() << ": property " << k
           << " is a settings file property, its value here is not used";  // not on -G's stdout
  }
  Properties::RESOLVE_OWN(node, ps, AppVars::INSTANCE().cliProperkeys(), project().file(),
                          [&](std::string const& s) { return Properties::RESOLVE(*this, s); });
  return *this;
}

std::size_t maiken::Properties::FIND_UNESCAPED(std::string const& s, std::string const& f,
                                               std::size_t pos) {
  for (pos = s.find(f, pos); pos != std::string::npos; pos = s.find(f, pos + 1))
    if (pos == 0 || s[pos - 1] != '\\') return pos;
  return std::string::npos;
}

std::optional<std::string> maiken::Properties::BUILTIN(std::string const& key) {
  if (key == "mkn.bin.key") return std::string{defaultBinKey()};
  if (key == "mkn.with.bin") return "0";
  if (auto const& pks = AppVars::INSTANCE().properkeys(); !SETTINGS_KEY(key) && pks.count(key))
    return pks.at(key);
  return std::nullopt;
}

bool maiken::Properties::FIXED(std::string const& key) {
  return key.rfind("mkn.sys.", 0) == 0 || key.rfind("mkn.user.", 0) == 0;
}

void maiken::Properties::NOT_FIXED(std::string const& key, std::string const& where)
    KTHROW(mkn::kul::Exception) {
  if (FIXED(key))
    KEXIT(1, "Property : '" + key + "' in " + where +
                 ": mkn.sys.* and mkn.user.* are maiken's, they cannot be set");
}

bool maiken::Properties::SETTINGS_KEY(std::string const& key) {
  for (auto const* k : {"mkn.bin.key", "mkn.with.bin", "mkn.link.rpathing",
                        "mkn.compile.visibility_hidden", "mkn.env.automatic"})
    if (key == k) return true;
  for (auto const* prefix : {"mkn.flags.", "mkn.compile.flags.", "mkn.link.flags."})
    if (key.rfind(prefix, 0) == 0) return true;
  return false;
}

bool maiken::Properties::EXTENDS(std::string const& key, std::string const& value) {
  return FIND_UNESCAPED(value, "${" + key + "}") != std::string::npos;
}

std::string maiken::Properties::EXTEND(std::string const& key, std::string value,
                                       mkn::kul::hash::map::S2S const& ps, std::string const& file)
    KTHROW(mkn::kul::Exception) {
  if (!EXTENDS(key, value)) return value;
  auto const builtin = ps.count(key) ? std::nullopt : BUILTIN(key);
  if (!ps.count(key) && !builtin)
    KEXIT(1, "Property : '" + key + "' in " + file +
                 " uses itself, but nothing before it (a super: or settings file) defines it");
  auto const ref = "${" + key + "}";
  auto const& before = builtin ? *builtin : ps.at(key);
  for (auto pos = FIND_UNESCAPED(value, ref); pos != std::string::npos;
       pos = FIND_UNESCAPED(value, ref, pos + before.size()))
    value.replace(pos, ref.size(), before);
  return value;
}

std::shared_ptr<std::tuple<std::string, int, int>> maiken::Properties::KEY(
    mkn::kul::hash::map::S2S const& /*ps*/, std::string const& s) KTHROW(mkn::kul::Exception) {
  std::string r = s;
  int lb = s.find("${");
  int clb = s.find("\\${");
  int rb = s.find("}");
  int crb = s.find("\\}");
  while ((lb - clb + 1) == 0) {
    lb = r.substr(clb + 3).find("${");
    clb = r.substr(clb + 3).find("\\${");
  }
  while ((rb - crb + 1) == 0) {
    rb = r.substr(crb + 2).find("}");
    crb = r.substr(crb + 2).find("\\}");
  }
  if (lb != -1 && clb == -1 && rb != -1 && crb == -1)
    return std::make_shared<std::tuple<std::string, int, int>>(r.substr(lb + 2, rb - 2 - lb), lb,
                                                               rb);
  return 0;
}

std::optional<std::string> maiken::Properties::PROJECT_LOOKUP(Application const& app,
                                                              std::string const& k) {
  auto const& av = AppVars::INSTANCE();
  auto const& pks = av.properkeys();
  if (auto const& cps = av.cliProperkeys(); cps.count(k)) return cps.at(k);
  if (SETTINGS_KEY(k)) {  // a settings file's (-X in it), or the built-in, never a project's
    if (pks.count(k)) return pks.at(k);
    if (auto const& sps = Settings::INSTANCE().properties(); sps.count(k)) return sps.at(k);
    return BUILTIN(k);
  }
  if (app.properties().count(k)) return app.properties().at(k);
  if (pks.count(k)) return pks.at(k);
  return std::nullopt;
}

std::string maiken::Properties::RESOLVE(Application const& app, std::string const& s)
    KTHROW(mkn::kul::Exception) {
  std::string r = s;
  std::shared_ptr<std::tuple<std::string, int, int>> t = KEY(app.properties(), s);
  if (t) {
    std::string k = std::get<0>(*t);
    int const& lb = std::get<1>(*t);
    int const& rb = std::get<2>(*t);

    if (auto const pk = PROJECT_LOOKUP(app, k))
      k = *pk;
    else if (app.project().root()[k] && app.project().root()[k].Type() == 2)
      k = app.project().root()[k].Scalar();
    else
      KEXIT(1, "Property : '" + k + "' has not been defined");
    r = Properties::RESOLVE(app, r.substr(0, lb) + k + r.substr(rb + 1));
  }
  mkn::kul::String::TRIM(r);
  return r;
}

std::string maiken::Properties::RESOLVE(Settings const& set, std::string const& s)
    KTHROW(mkn::kul::Exception) {
  std::string r = s;
  std::shared_ptr<std::tuple<std::string, int, int>> t = KEY(set.properties(), s);
  if (t) {
    std::string k = std::get<0>(*t);
    int const& lb = std::get<1>(*t);
    int const& rb = std::get<2>(*t);

    // -X, the file's own (with its super: files'), then a built-in; -P is a project's
    auto const& av = AppVars::INSTANCE();
    auto const& pks = av.properkeys();
    if (av.settingsProperties().count(k))
      k = av.settingsProperties().at(k);
    else if (SETTINGS_KEY(k) && pks.count(k))  // as set from the settings, the environment, -X
      k = pks.at(k);
    else if (set.properties().count(k) > 0)
      k = set.properties().at(k);
    else if (pks.count(k))
      k = pks.at(k);
    else
      KEXIT(1, "Property : '" + k + "' has not been defined");
    r = Properties::RESOLVE(set, r.substr(0, lb) + k + r.substr(rb + 1));
  }
  mkn::kul::String::TRIM(r);
  return r;
}
