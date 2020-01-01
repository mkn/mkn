/**
Copyright (c) 2017, Philip Deegan.
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
#include "maiken.hpp"
#include "maiken/dist.hpp"

namespace maiken {
using namespace kul::cli;
class CLIHandler : public Constants {
 private:
  std::vector<kul::cli::Arg> argV {
    Arg('a', STR_ARG, ArgType::STRING), Arg('A', STR_ADD, ArgType::STRING),
        Arg('b', STR_BINC, ArgType::STRING), Arg('B', STR_BPATH, ArgType::STRING),
        Arg('C', STR_DIR, ArgType::STRING), Arg('d', STR_DEP, ArgType::MAYBE),
        Arg('R', STR_DRY_RUN), Arg('E', STR_ENV, ArgType::STRING),
        Arg('f', STR_FINC, ArgType::STRING), Arg('F', STR_FPATH, ArgType::STRING),
        Arg('g', STR_DEBUG, ArgType::MAYBE), Arg('G', STR_GET, ArgType::STRING), Arg('h', STR_HELP),
        Arg('j', STR_JARG, ArgType::STRING), Arg('K', STR_STATIC),
        Arg('l', STR_LINKER, ArgType::STRING), Arg('L', STR_ALINKER, ArgType::STRING),
        Arg('m', STR_MOD, ArgType::STRING), Arg('M', STR_MAIN, ArgType::STRING),
#if defined(_MKN_WITH_MKN_RAM_) && defined(_MKN_WITH_IO_CEREAL_)
        Arg('n', STR_NODES, ArgType::MAYBE),
#endif  //_MKN_WITH_MKN_RAM_) && _MKN_WITH_IO_CEREAL_
        Arg('o', STR_OUT, ArgType::STRING), Arg('O', STR_OPT, ArgType::MAYBE), Arg('q', STR_QUIET),
        Arg('p', STR_PROFILE, ArgType::STRING), Arg('P', STR_PROPERTY, ArgType::STRING),
        Arg('r', STR_RUN_ARG, ArgType::STRING), Arg('s', STR_SCM_STATUS), Arg('S', STR_SHARED),
        Arg('t', STR_THREADS, ArgType::MAYBE), Arg('T', STR_WITHOUT, ArgType::STRING),
        Arg('u', STR_SCM_UPDATE), Arg('U', STR_SCM_FUPDATE), Arg('v', STR_VERSION),
        Arg('w', STR_WITH, ArgType::STRING), Arg('W', STR_WARN, ArgType::MAYBE),
        Arg('x', STR_SETTINGS, ArgType::STRING)
  };
  std::vector<kul::cli::Cmd> cmdV{
      Cmd(STR_INIT),    Cmd(STR_INC),  Cmd(STR_SRC),      Cmd(STR_MERGE),
#ifndef _MKN_DISABLE_MODULES_
      Cmd(STR_MODS),
#endif  //_MKN_DISABLE_MODULES_
      Cmd(STR_CLEAN),   Cmd(STR_DEPS), Cmd(STR_BUILD),    Cmd(STR_RUN),
      Cmd(STR_COMPILE), Cmd(STR_LINK), Cmd(STR_PROFILES), Cmd(STR_DBG),
      Cmd(STR_PACK),    Cmd(STR_INFO), Cmd(STR_TREE),     Cmd(STR_TEST)};

 public:
  std::vector<kul::cli::Arg> args() { return argV; }
  std::vector<kul::cli::Cmd> cmds() { return cmdV; }
};
}  // namespace maiken

std::vector<maiken::Application *> maiken::Application::CREATE(int16_t argc, char *argv[])
    KTHROW(kul::Exception) {
  using namespace kul::cli;
  CLIHandler cli;
  Args args(cli.cmds(), cli.args());
  try {
    args.process(argc, argv);
  } catch (const kul::cli::Exception &e) {
    KEXIT(1, e.what());
  }
  return CREATE(args);
}
std::vector<maiken::Application *> maiken::Application::CREATE(const kul::cli::Args &args)
    KTHROW(kul::Exception) {
  using namespace kul::cli;

  kul::File yml("mkn.yaml");
  if (args.empty() || (args.size() == 1 && args.has(STR_DIR))) {
    if (args.size() == 1 && args.has(STR_DIR)) {
      kul::Dir d(args.get(STR_DIR));
      kul::File f(args.get(STR_DIR));
      if (!d && !f) KEXIT(1, "Invalid -C argument, no such item: " + args.get(STR_DIR));
      if(f) {
        yml = f;
        d = f.dir();
      } else {
        if(kul::File("mkn.yaml", d.real()).is()) yml = kul::File("mkn.yaml", d.real());
        if(kul::File("mkn.yml", d.real()).is())  yml = kul::File("mkn.yml", d.real());
      }
      kul::env::CWD(d);
    }
    if (yml) {
      kul::io::Reader reader(yml);
      const char *c = reader.readLine();
      const std::string s(c ? c : "");
      if (s.size() > 3 && s.substr(0, 3) == "#! ") {
        std::string line(s.substr(3));
        if (!line.empty()) {
          std::vector<std::string> lineArgs(kul::cli::asArgs(line));
          std::vector<char *> lineV;
          lineV.push_back(const_cast<char *>(maiken::PROGRAM.c_str()));
          for (size_t i = 0; i < lineArgs.size(); i++) lineV.push_back(&lineArgs[i][0]);
          return CREATE(lineV.size(), &lineV[0]);
        }
      }
    }
    showHelp();
    KEXIT(0, "");
  }
  if (args.empty() || args.has(STR_HELP)) {
    showHelp();
    KEXIT(0, "");
  }
  if (args.has(STR_VERSION)) {
    std::stringstream ss, mod;
    ss << KTOSTRING(_MKN_VERSION_) << " (" << KTOSTRING(__KUL_OS__) << ") w/[";
    if (_MKN_REMOTE_EXEC_) mod << "exec,";
#ifndef _MKN_DISABLE_MODULES_
    mod << "mod,";
#endif  //_MKN_DISABLE_MODULES_
#ifdef _MKN_WITH_MKN_RAM_
#if defined(_MKN_WITH_IO_CEREAL_)
    mod << "dist,";
#endif  //_MKN_WITH_MKN_RAM_ && _MKN_WITH_IO_CEREAL_
    mod << "ram,";
#endif  //_MKN_WITH_MKN_RAM_
#ifdef _MKN_WITH_GOOGLE_SPARSEHASH_
    mod << "sparsehash,";
#endif  //_MKN_WITH_GOOGLE_SPARSEHASH_

    if (_MKN_TIMESTAMPS_) mod << "ts,";
    if (mod.str().size()) mod.seekp(-1, mod.cur);
    mod << "]";
    ss << mod.str();
    KOUT(NON) << ss.str();
    KEXIT(0, "");
  }
  if (args.has(STR_DIR)) {
    kul::Dir d(args.get(STR_DIR));
    kul::File f(args.get(STR_DIR));
    if (!d && !f) KEXIT(1, "Invalid -C argument, no such item: " + args.get(STR_DIR));
    if(f) {
      yml = f;
      d = f.dir();
    } else {
      if(kul::File("mkn.yaml", d.real()).is()) yml = kul::File("mkn.yaml", d.real());
      if(kul::File("mkn.yml", d.real()).is())  yml = kul::File("mkn.yml", d.real());
    }
    kul::env::CWD(d);
  }
  if (args.has(STR_INIT)) NewProject{};

  if(!yml && kul::File("mkn.yml").is()) yml = kul::File("mkn.yml");
  const Project &project(*Projects::INSTANCE().getOrCreate(yml));

  std::vector<std::string> profiles;
  if (args.has(STR_PROFILE)) {
    for (auto profile : kul::String::SPLIT(args.get(STR_PROFILE), ",")) {
      bool f = 0;
      if (profile == "@") {
        profiles.emplace_back("");
        continue;
      }
      bool wildcard = profile.rfind("*") == profile.size() - 1;
      for (const auto &n : project.root()[STR_PROFILE]) {
        std::string yProfile = n[STR_NAME].Scalar();
        if(wildcard && yProfile.find(profile.substr(0, profile.rfind("*"))) == 0){
          profiles.emplace_back(yProfile);
          f = 1;
        }
        else
          f = yProfile == profile;

        if (f && !wildcard) break;
      }

      if (!f) KEXIT(1, "profile does not exist");
      if (!wildcard) profiles.emplace_back(profile);
    }
  }
  if (profiles.empty()) profiles.emplace_back("");

  if (args.has(STR_SETTINGS) && !Settings::SET(args.get(STR_SETTINGS)))
    KEXIT(1, "Unable to set specific settings xml");
  else
    Settings::INSTANCE();

  if (args.has(STR_DRY_RUN)) AppVars::INSTANCE().dryRun(true);
  if (args.has(STR_SHARED)) AppVars::INSTANCE().shar(true);
  if (args.has(STR_STATIC)) AppVars::INSTANCE().stat(true);
  if (AppVars::INSTANCE().shar() && AppVars::INSTANCE().stat())
    KEXIT(1, "Cannot specify shared and static simultaneously");
  if (args.has(STR_SCM_FUPDATE)) AppVars::INSTANCE().fupdate(true);
  if (args.has(STR_SCM_UPDATE)) AppVars::INSTANCE().update(true);
  if (args.has(STR_DEP)) AppVars::INSTANCE().dependencyLevel((std::numeric_limits<int16_t>::max)());

#if defined(_MKN_WITH_MKN_RAM_) && defined(_MKN_WITH_IO_CEREAL_)
  if (args.has(STR_NODES)) {
    try {
      AppVars::INSTANCE().nodes((std::numeric_limits<int16_t>::max)());
      if (args.get(STR_NODES).size())
        AppVars::INSTANCE().nodes(kul::String::UINT16(args.get(STR_NODES)));
    } catch (const kul::StringException &e) {
      KEXIT(1, "-n argument is invalid");
    } catch (const kul::Exception &e) {
      KEXIT(1, e.stack());
    }
  }
#endif  //_MKN_WITH_MKN_RAM_) && _MKN_WITH_IO_CEREAL_

  {
    auto getSet = [args](const std::string &a, const std::string &e, const uint16_t &default_value,
                         const std::function<void(const uint16_t &)> &func) {
      if (args.has(a)) try {
          if (args.get(a).size()) {
            auto val(kul::String::UINT16(args.get(a)));
            if (val > 9) KEXIT(1, e) << " argument is invalid";
            func(val);
          } else
            func(default_value);
        } catch (const kul::StringException &e) {
          KEXIT(1, "") << e << " argument is invalid";
        } catch (const kul::Exception &e) {
          KEXIT(1, e.stack());
        }
    };

    getSet(STR_DEBUG, "-g", 9,
           std::bind((void (AppVars::*)(const uint16_t &)) & AppVars::debug,
                     std::ref(AppVars::INSTANCE()), std::placeholders::_1));
    getSet(STR_OPT, "-O", 9,
           std::bind((void (AppVars::*)(const uint16_t &)) & AppVars::optimise,
                     std::ref(AppVars::INSTANCE()), std::placeholders::_1));
    getSet(STR_WARN, "-W", 8,
           std::bind((void (AppVars::*)(const uint16_t &)) & AppVars::warn,
                     std::ref(AppVars::INSTANCE()), std::placeholders::_1));
  }
  {
    auto splitArgs = [](const std::string &s, const std::string &t,
                        const std::function<void(const std::string &, const std::string &)> &f) {
      for (const auto &p : kul::String::ESC_SPLIT(s, ',')) {
        if (p.find("=") == std::string::npos) KEXIT(1, t + " override invalid, = missing");
        std::vector<std::string> ps = kul::String::ESC_SPLIT(p, '=');
        if (ps.size() > 2) KEXIT(1, t + " override invalid, escape extra \"=\"");
        f(ps[0], ps[1]);
      }
    };

    if (args.has(STR_PROPERTY))
      splitArgs(
          args.get(STR_PROPERTY), "property",
          std::bind(
              (void (AppVars::*)(const std::string &, const std::string &)) & AppVars::properkeys,
              std::ref(AppVars::INSTANCE()), std::placeholders::_1, std::placeholders::_2));
    if (args.has(STR_ENV))
      splitArgs(
          args.get(STR_ENV), "environment",
          std::bind((void (AppVars::*)(const std::string &, const std::string &)) & AppVars::envVar,
                    std::ref(AppVars::INSTANCE()), std::placeholders::_1, std::placeholders::_2));
  }

  std::vector<std::string> cmds = {{STR_CLEAN, STR_BUILD, STR_COMPILE, STR_LINK, STR_RUN, STR_TEST,
                                    STR_DBG, STR_PACK, STR_MERGE}};
  for (const auto &cmd : cmds)
    if (args.has(cmd)) AppVars::INSTANCE().command(cmd);

  if (args.has(STR_WITH)) AppVars::INSTANCE().with(args.get(STR_WITH));
  if (args.has(STR_MOD)) AppVars::INSTANCE().mods(args.get(STR_MOD));

  if (args.has(STR_WITHOUT)) {
    AppVars::INSTANCE().without(args.get(STR_WITHOUT));
    kul::hash::set::String wop;
    try {
      Application::parseDependencyString(AppVars::INSTANCE().without(), wop);
    } catch (const kul::Exception &e) {
      KEXIT(1, MKN_ERR_INVALID_WITHOUT_CLI);
    }
    AppVars::INSTANCE().withoutParsed(wop);
  }

  AppVars::INSTANCE().dependencyString(args.has(STR_DEP) ? args.get(STR_DEP) : "");
  std::vector<Application *> apps;
  for (auto profile : profiles) {
    auto *app = Applications::INSTANCE().getOrCreateRoot(project, profile);
    auto &a = *app;
    a.ig = 0;
    a.ro = 1;
    apps.emplace_back(app);
    a.buildDepVec(AppVars::INSTANCE().dependencyString());
    a.addCLIArgs(args);
  }

  if (apps.size() == 1) {
    if (args.has(STR_PROFILES)) {
      apps[0]->showProfiles();
      KEXIT(0, "");
    }
    if (args.has(STR_INFO)) apps[0]->showConfig(1);
    if (args.has(STR_TREE)) apps[0]->showTree();
    if (args.has(STR_GET)) {
      const auto &get(args.get(STR_GET));
      if (apps[0]->properties().count(get)) {
        KOUT(NON) << (*apps[0]->properties().find(get)).second;
      }
      if (AppVars::INSTANCE().properkeys().count(get)) {
        KOUT(NON) << (*AppVars::INSTANCE().properkeys().find(get)).second;
      }
      KEXIT(0, "");
    }
  }

  if (args.has(STR_ARG)) AppVars::INSTANCE().args(args.get(STR_ARG));
  if (args.has(STR_RUN_ARG)) AppVars::INSTANCE().runArgs(args.get(STR_RUN_ARG));
  if (args.has(STR_LINKER)) AppVars::INSTANCE().linker(args.get(STR_LINKER));
  if (args.has(STR_ALINKER)) AppVars::INSTANCE().allinker(args.get(STR_ALINKER));
  if (args.has(STR_THREADS)) {
    try {
      AppVars::INSTANCE().threads(kul::cpu::threads());
      if (args.get(STR_THREADS).size())
        AppVars::INSTANCE().threads(kul::String::UINT16(args.get(STR_THREADS)));
    } catch (const kul::StringException &e) {
      KEXIT(1, "-t argument is invalid");
    } catch (const kul::Exception &e) {
      KEXIT(1, e.stack());
    }
  }
  if (kul::env::EXISTS("MKN_COMPILE_THREADS")) {
    try {
      AppVars::INSTANCE().threads(kul::String::UINT16(kul::env::GET("MKN_COMPILE_THREADS")));
    } catch (const kul::StringException &e) {
      KEXIT(1, "MKN_COMPILE_THREADS is invalid");
    } catch (const kul::Exception &e) {
      KEXIT(1, e.stack());
    }
  }
  if (args.has(STR_JARG)) {
    try {
      YAML::Node node = YAML::Load(args.get(STR_JARG));
      for (YAML::const_iterator it = node.begin(); it != node.end(); ++it)
        for (const auto &s : kul::String::SPLIT(it->first.Scalar(), ':'))
          AppVars::INSTANCE().jargs(s, it->second.Scalar());
    } catch (const std::exception &e) {
      KEXIT(1, "JSON args failed to parse");
    }
  }

  auto printDeps = [&](const std::vector<Application *> &vec) {
    std::vector<const Application *> v;
    for (auto app = vec.rbegin(); app != vec.rend(); ++app) {
      const std::string &s((*app)->project().dir().real());
      auto it = std::find_if(v.begin(), v.end(),
                             [&s](const Application *a) { return a->project().dir().real() == s; });
      if (it == v.end()) v.push_back(*app);
    }
    for (auto *app : v) KOUT(NON) << app->project().dir();
    KEXIT(0, "");
  };

  if (args.has(STR_DEPS))
    for (auto a : apps) printDeps(a->deps);
  if (args.has(STR_MODS))
    for (auto a : apps) printDeps(a->modDeps);

  if (args.has(STR_INC)) {
    for (auto a : apps)
      for (const auto &p : a->includes()) KOUT(NON) << p.first;
    KEXIT(0, "");
  }
  if (args.has(STR_SRC)) {
    for (auto a : apps) {
      for (const auto &p1 : a->sourceMap())
        for (const auto &p2 : p1.second)
          for (const auto &p3 : p2.second) KOUT(NON) << kul::File(p3).full();
      for (auto app = a->deps.rbegin(); app != a->deps.rend(); ++app)
        for (const auto &p1 : (*app)->sourceMap())
          if (!(*app)->ig)
            for (const auto &p2 : p1.second)
              for (const auto &p3 : p2.second) KOUT(NON) << kul::File(p3).full();
    }
    KEXIT(0, "");
  }
  if (args.has(STR_SCM_STATUS)) {
    for (auto a : apps) a->scmStatus(args.has(STR_DEP));
    KEXIT(0, "");
  }

  if (apps.size() == 1) {
    if (args.has(STR_ADD))
      for (const auto &s : kul::String::ESC_SPLIT(args.get(STR_ADD), ','))
        apps[0]->addSourceLine(s);
    if (args.has(STR_MAIN)) apps[0]->main = args.get(STR_MAIN);
    if (args.has(STR_OUT)) apps[0]->out = args.get(STR_OUT);
  }

#if defined(_MKN_WITH_MKN_RAM_) && defined(_MKN_WITH_IO_CEREAL_)
  if (AppVars::INSTANCE().nodes()) {
    maiken::dist::RemoteCommandManager::INST().build_hosts(Settings::INSTANCE());
    auto &hosts(maiken::dist::RemoteCommandManager::INST().hosts());
    if (hosts.empty()) KEXCEPTION("Settings file has no hosts configured");
    size_t threads =
        (hosts.size() < AppVars::INSTANCE().nodes()) ? hosts.size() : AppVars::INSTANCE().nodes();
    // ping nodes and set active
    auto ping = [&](const maiken::dist::Host &host) {
      auto post = std::make_unique<maiken::dist::Post>(
          maiken::dist::RemoteCommandManager::INST().build_setup_query(*apps[0], args));
      post->send(host);
    };
    kul::ChroncurrentThreadPool<> ctp(threads, 1, 1000000000, 1000);
    std::exception_ptr exp;
    auto pingex = [&](const kul::Exception &e) {
      ctp.stop().interrupt();
      throw e;
    };
    for (size_t i = 0; i < threads; i++) ctp.async(std::bind(ping, std::ref(hosts[i])), pingex);
    ctp.finish(10000000);  // 10 milliseconds
    ctp.rethrow();
  }
#endif  //  _MKN_WITH_MKN_RAM_) && defined(_MKN_WITH_IO_CEREAL_)
  return apps;
}
