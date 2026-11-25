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
#ifndef MAIKEN_SETTINGS_HPP
#define MAIKEN_SETTINGS_HPP

#include "mkn/kul/os.hpp"
#include "mkn/kul/cli.hpp"
#include "mkn/kul/yaml.hpp"
#include "mkn/kul/except.hpp"
#include "maiken/defs.hpp"
#include "mkn/kul/map.hpp"
#include "mkn/kul/lang/compiler.hpp"
#include "yaml-cpp/yaml.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mkn {
namespace kul {
namespace env {
class Var;
}  // namespace env
}  // namespace kul
}  // namespace mkn

namespace maiken {

class SettingsException : public mkn::kul::Exception {
 public:
  SettingsException(char const* f, uint16_t const& l, std::string const& s)
      : mkn::kul::Exception(f, l, s) {}
};

class Settings : public mkn::kul::yaml::File, public Constants {
  using Findables = mkn::kul::hash::map::S2T<mkn::kul::hash::map::S2T<std::vector<std::string>>>;

 public:
  Settings(std::filesystem::path const& s);

  // this file, then each super: file with the files it extends before the next one
  std::vector<Settings const*> chain() const;

  mkn::kul::yaml::Validator validator() const;
  auto remotes(bool const module) const {
    if (module) return remoteModules();
    return remoteRepos();
  }
  std::vector<std::string> const& remoteModules() const { return rms; }
  std::vector<std::string> const& remoteRepos() const { return rrs; }
  mkn::kul::hash::map::S2S const& properties() const { return ps; }
  bool propertyBool(std::string const& key, bool const default_val = true) const {
    auto const it = ps.find(key);
    if (it == ps.end()) return default_val;
    return it->second == "true" || it->second == "1";
  }
  // flags with their named parts replaced by properties: "mkn.flags.<part>" for compile and
  //  link, then "mkn.<stage>.flags.<part>" for stage "compile" or "link" only. An empty value
  //  drops the part, a name flags don't have adds a new part at the end.
  mkn::kul::lang::Flags flags(mkn::kul::lang::Flags flags, std::string const& stage) const {
    for (auto const& prefix : {std::string{"mkn.flags."}, "mkn." + stage + ".flags."})
      for (auto const& [key, value] : ps)
        if (key.size() > prefix.size() && key.starts_with(prefix))
          flags.set(key.substr(prefix.size()), value);
    return flags;
  }
  // the parts of several flags in order, as one
  static mkn::kul::lang::Flags JOIN(std::initializer_list<mkn::kul::lang::Flags> const all) {
    mkn::kul::lang::Flags joined;
    for (auto const& f : all)
      for (auto const& p : f.parts()) joined.set(p.name, p.flags);
    return joined;
  }
  Findables const& findables() const { return findables_; }
  // bin: dependencies used as prebuilt binaries, project name to release tag
  mkn::kul::hash::map::S2S const& binaries() const { return binaries_; }
  std::optional<std::string> local_dep_repo() const;
  std::optional<std::string> local_mod_repo() const;

  static Settings& INSTANCE() KTHROW(mkn::kul::Exit);
  static void SET(std::string const& s);
  static mkn::kul::File RESOLVE(std::string const& s, Settings const* settings = nullptr)
      KTHROW(SettingsException);
  static void POST_CONSTRUCT(Settings* settings = nullptr);
  static mkn::kul::env::Var PARSE_ENV_NODE(YAML::Node const&, Settings const&);
  static mkn::kul::File userDefault() {
    return mkn::kul::File("settings.yaml", mkn::kul::Dir(mkn::kul::user::home("maiken").string()));
  }
  static void writeDefault() {
    auto const f = userDefault();
    if (!f.dir().is()) f.dir().mk();
    if (!f.is()) write(f);
  }

  auto getFirstFound(auto const fn) const;
  void traverse(auto const fn) const;

  auto operator[](std::string const& s) { return root()[s]; }
  auto operator[](std::string const& s) const { return root()[s]; }

 private:
  std::vector<std::string> rrs, rms;
  std::vector<std::unique_ptr<Settings>>
      sups;  // super: files, in order, those seen before left out
  static constexpr std::size_t MAX_SUPERS = 5;
  // every super: file, in order, also those loaded under an earlier file (owned there): what this
  //  file's properties extend; files above it in the chain (a cycle) left out
  std::array<Settings*, MAX_SUPERS> supers{};
  std::size_t n_supers = 0;
  mkn::kul::hash::map::S2S ps;
  std::optional<mkn::kul::hash::map::S2S> base_ps;  // ps before its own were resolved into it
  Findables findables_;
  mkn::kul::hash::map::S2S binaries_;
  std::vector<bool> envUsed;  // per env: entry, see ENV_USED, empty: all
  // the variables its env: sets as they were before it, unset: nullopt
  std::optional<std::vector<std::pair<std::string, std::optional<std::string>>>> envBefore;

  void resolveProperties() KTHROW(SettingsException);
  void preorder(std::vector<Settings*>& files);  // this file, then each super: file's chain
  // each file once, after its super: files
  void postorder(std::vector<Settings*>& files, std::unordered_set<Settings const*>& seen);
  void mergeSuperProperties();  // its super: files' properties it doesn't set, the first's first
  // sets this file's env: entries used, each in turn: one may use the value the one before set
  void applyEnv() const;
  void resolveFindables() KTHROW(SettingsException);
  void resolveBinaries() KTHROW(SettingsException);
  static std::unique_ptr<Settings> instance;
  static void write(mkn::kul::File const& f) KTHROW(mkn::kul::Exit);
};

void Settings::traverse(auto const fn) const {
  using FuncType = std::function<void(Settings const&)>;

  FuncType const loop = [&](auto const& settings) {
    fn(settings);
    for (auto const& sup : settings.sups) loop(*sup);
  };

  loop(*this);
}

inline std::vector<Settings const*> Settings::chain() const {
  std::vector<Settings const*> v;
  traverse([&](Settings const& s) { v.push_back(&s); });
  return v;
}

auto Settings::getFirstFound(auto const fn) const {
  using Ret = std::invoke_result_t<decltype(fn), Settings const&>;
  using FuncType = std::function<Ret(Settings const&)>;

  FuncType const loop = [&](auto const& settings) -> Ret {
    if (auto const node = fn(settings)) return node;
    for (auto const& sup : settings.sups)
      if (auto const node = loop(*sup)) return node;
    return std::nullopt;
  };

  return loop(*this);
}

}  // namespace maiken

#endif /* MAIKEN_SETTINGS_HPP */
