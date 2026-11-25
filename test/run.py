#! /usr/bin/env python3
"""
Behaviour tests: a built mkn against the projects under test/, as dry runs ("build -R", never with
clean, which a dry run still does) so no compiler runs. Each case checks the exit code and text
expected, or not, in the output.

    python3 test/run.py <path to mkn>

Add a case to CASES, with a project directory under test/ when none fits.
"""

import os
import subprocess
import sys
import tempfile

TEST = os.path.dirname(os.path.abspath(__file__))
APPLE = sys.platform == "darwin"
BSD = APPLE or sys.platform.startswith("freebsd")
OS = "nix" if sys.platform.startswith("linux") else "bsd" if BSD else "win"
ARCH = os.environ.get("MKN_ARCH", "x86_64")


def source(profile, settings, ok, has=(), lacks=()):
    """test/source: a src: entry's files without a compiler in the settings file list"""
    return dict(
        name=f"source {profile} {settings}",
        cwd=os.path.join(TEST, "source"),
        args=["build", "-R", "-p", profile, "-x", os.path.join(TEST, "source", settings)],
        ok=ok,
        has=has,
        lacks=lacks,
    )


def prop(args=(), has=(), lacks=()):
    """test/property: a project property over a built-in of the same name, -P over both"""
    return dict(
        name=f"property {' '.join(args)}".rstrip(),
        cwd=os.path.join(TEST, "property"),
        args=["build", "-R", *args],
        ok=True,
        has=has,
        lacks=lacks,
    )


def sprop(args=(), ok=True, has=(), lacks=(), stdout=None):
    """test/settings_property: -X over a settings file's property (and those using it), -P not"""
    case = dict(
        name=f"settings property {' '.join(args)}".rstrip(),
        cwd=os.path.join(TEST, "settings_property"),
        args=["build", "-R", "-x", os.path.join(TEST, "settings_property", "dry.yaml"), *args],
        ok=ok,
        has=has,
        lacks=lacks,
    )
    if stdout is not None:
        case["stdout"] = stdout
    return case


def sres(args=(), ok=True, has=(), lacks=(), stdout=None, x=("-X", "flag=x"), p=("-P", "pflag=p")):
    """test/settings_resolve: top.yaml's properties, its super: file's, -X and -P over values never
    resolved"""
    case = dict(
        name=f"settings resolve {' '.join([*args, *x, *p])}".rstrip(),
        cwd=os.path.join(TEST, "settings_resolve"),
        args=[*(args or ["build", "-R"]), "-x", "top.yaml", *x, *p],
        ok=ok,
        has=has,
        lacks=lacks,
    )
    if stdout is not None:
        case["stdout"] = stdout
    return case


# the default settings file (a fresh HOME) of a project run with no arguments, its header line
#  running mkn again: its own properties resolved again from what they extend
HEADER_SETTINGS = """super:
  - with space/base
property:
  sdk: ${sdk}/v2
env:
  - SR_V=x
file:
- type: cpp:cxx:cc
  archiver: ar -cr
  compiler: g++ -DSDK=${sdk}
  linker: g++
"""


def pub(project, args=(), ok=True, has=(), lacks=(), once=()):
    """test/pub_arg: pub_arg compile arguments of a project and its dependents"""
    return dict(
        name=f"pub_arg {project} {' '.join(args)}".rstrip(),
        cwd=os.path.join(TEST, "pub_arg", project),
        args=["build", "-R", *args, "-x", os.path.join(TEST, "pub_arg", "settings.yaml")],
        ok=ok,
        # whole arguments: -DBASE_PUB is neither -DBASE_PUB_X nor in -DWRAPPED=-DBASE_PUB
        has=[f" -D{d} " for d in has] if ok else has,
        lacks=[f" -D{d} " for d in lacks],
        once=[f" -D{d} " for d in once],
    )


BASE_STATIC = ["BASE_PUB", "BASE_OS", "BASE_STATIC", "BASE_GXX"]

CASES = [
    # settings files' properties: their own, -X replacing them and those built from them, -P not
    sprop(has=["-DSP_FLAG=from_file ", "-DSP_DERIVED=from_file_derived "]),
    sprop(["-X", "flag=from_x"], has=["-DSP_FLAG=from_x ", "-DSP_DERIVED=from_x_derived "],
          lacks=["from_file"]),
    sprop(["-P", "flag=from_p"], has=["-DSP_FLAG=from_file "], lacks=["from_p"]),
    # -P of a built-in: the project's, settings files keep the built-in
    sprop(["-P", "OS=from_p"], has=["-DSP_OS="], lacks=["from_p"]),
    sprop(has=[f"-DSP_ARCH={ARCH}_sp "]),
    # mkn.sys.* and mkn.user.* cannot be set: not in a settings file, nor with -X or -P
    dict(name="settings property fixed", cwd=os.path.join(TEST, "settings_property"),
         args=["build", "-R", "-x", os.path.join(TEST, "settings_property", "fixed.yaml")],
         ok=False, has=["'mkn.sys.os' in", "cannot be set"], lacks=[]),
    sprop(["-X", "mkn.user.home=x"], ok=False, has=["'mkn.user.home' in -X", "cannot be set"]),
    dict(name="settings property -G OS -P", cwd=os.path.join(TEST, "settings_property"),
         args=["-G", "OS", "-P", "OS=from_p"], ok=True, has=[], lacks=[], stdout="from_p"),
    # maiken's own settings properties are -X, -P says so
    sprop(["-P", "mkn.with.bin=1"], ok=False, has=["use -X mkn.with.bin=1"]),
    dict(name="settings property -G mkn.with.bin -X", cwd=os.path.join(TEST, "settings_property"),
         args=["-G", "mkn.with.bin", "-X", "mkn.with.bin=1"], ok=True, has=[], lacks=[], stdout="1"),
    # settings files' properties: a super: list item with a space, ${MKN_REPO} in a super: file the
    #  chain's local: repo, an env: entry replaced by $NAME_X never resolved
    sres(has=["-DSDK=/opt/sdk/v2 ", "local_repo", "-DFLAG=x ", "-DPFLAG=p "]),
    # -X and -P: the value they replace never resolved
    sres(x=(), ok=False, has=["'not_defined' has not been defined"]),
    sres(p=(), ok=False, has=["'not_defined' has not been defined"]),
    # mkn.with.bin resolved, -X of the property it uses; a settings key: the settings file's
    sres(["-G", "mkn.with.bin"], x=("-X", "flag=x,enabled=1"), stdout="1"),
    sres(["-G", "mkn.link.rpathing"], stdout="from_settings"),
    dict(name="settings resolve header line", cwd=os.path.join(TEST, "settings_resolve"), args=[],
         home=HEADER_SETTINGS, ok=True, has=["-DSDK=/opt/sdk/v2 ", "-DPFLAG=p "], lacks=["v2/v2"]),
    # a directory: a known source type without a compiler is an error, hidden directories unread
    source(
        "dir",
        "c.yaml",
        ok=False,
        has=["Unable to handle sources in", "asm.S", "1 with types not in file list, of 2"],
        lacks=["hidden.S"],
    ),
    source("dir", "c_S.yaml", ok=True, has=["asm.S", "two.c"], lacks=["hidden.S"]),
    # a file named in src: without a compiler is an error
    source("named", "c.yaml", ok=False, has=["Unable to handle source:", "named.S"]),
    source("named", "c_S.yaml", ok=True, has=["named.S", "two.c"]),
    # files of no source type are skipped
    source("headers", "c.yaml", ok=True, has=["two.c"], lacks=["README"]),
    # C# is a known source type, csc builds it
    source("csharp", "c.yaml", ok=False, has=["Unable to handle sources in", "app.cs"]),
    # Fortran's upper case suffixes are known source types too
    source("fortran", "c.yaml", ok=False, has=["Unable to handle sources in", "kernel.F08"]),
    # Objective-C is a known source type only on Apple
    source("objc", "c.yaml", ok=not APPLE, has=["apple.m"] if APPLE else ["two.c"]),
    # pub_arg: a string always applies, a map keyed as if_arg for the declaring profile
    pub("base", ["-p", "static"], has=BASE_STATIC, lacks=["BASE_NOT_SHARED"]),
    # a dependent gets its dependency's public arguments, not its private ones
    pub("mid", has=BASE_STATIC + ["MID_PRIVATE", "MID_PUB"]),
    # passed on again: app depends on mid only
    pub("app", ["-p", "mid"], has=BASE_STATIC + ["MID_PUB"], lacks=["MID_PRIVATE"]),
    # two paths to base: its arguments once, a different argument containing one kept
    pub("app", ["-p", "diamond"], has=["MID_PUB", "WRAPPED=-DBASE_PUB"], once=BASE_STATIC),
    # an explicit mode: is not changed by -K
    pub("app", ["-p", "shared"], has=["BASE_PUB", "BASE_SHARED"], lacks=["BASE_NOT_STATIC"]),
    pub("app", ["-p", "shared", "-K"], has=["BASE_PUB", "BASE_SHARED"], lacks=["BASE_NOT_STATIC"]),
    # without a mode: the build decides
    pub("app", ["-p", "lib"], has=["LIB_SHARED"], lacks=["LIB_STATIC"]),
    pub("app", ["-p", "lib", "-K"], has=["LIB_STATIC"], lacks=["LIB_SHARED"]),
    # keys are checked as if_arg's
    pub("bad", ok=False, has=["malformed pub_arg key"]),
    # a project property over a built-in of the same name, -P over both, mkn.sys.os fixed
    prop(has=["-DPROP_OS=mine ", f"-DSYS_OS={OS} "], lacks=["WARNING"]),
    prop(["-P", "OS=cli"], has=["-DPROP_OS=cli ", f"-DSYS_OS={OS} "]),
    # -G prints the value alone on stdout
    dict(name="property -G OS", cwd=os.path.join(TEST, "property"), args=["-G", "OS"], ok=True,
         has=[], lacks=[], stdout="mine"),
    dict(name="property -G mkn.sys.os", cwd=os.path.join(TEST, "property"),
         args=["-G", "mkn.sys.os"], ok=True, has=[], lacks=[], stdout=OS),
    dict(name="property -P mkn.sys.os", cwd=os.path.join(TEST, "property"),
         args=["build", "-R", "-P", "mkn.sys.os=x"], ok=False, has=["cannot be set"], lacks=[]),
    dict(name="property fixed", cwd=os.path.join(TEST, "property_fixed"), args=["build", "-R"],
         ok=False, has=["'mkn.user.home' in", "cannot be set"], lacks=[]),
    dict(name="property -G unknown", cwd=os.path.join(TEST, "property"), args=["-G", "mkn.nope"],
         ok=True, has=[], lacks=[], stdout=""),
    # "@" is the top level (-p @, dep[@], prebuilt keys' profile=@), never a profile's name
    dict(name="profile named @", cwd=os.path.join(TEST, "profile_at"), args=["build", "-R"], ok=False,
         has=['Profile may not be named "@"'], lacks=[]),
]


def run(mkn, case):
    """has/lacks/once look at stdout and stderr together; "stdout", when given, is all of stdout"""
    separate = "stdout" in case
    with tempfile.TemporaryDirectory() as home:
        env = None
        if "home" in case:  # a fresh HOME with this default settings file
            settings = os.path.join(home, ".maiken" if os.name != "nt" else "maiken")
            os.makedirs(settings)
            with open(os.path.join(settings, "settings.yaml"), "w") as f:
                f.write(case["home"])
            env = dict(os.environ, HOME=home)
        result = subprocess.run(
            [mkn] + case["args"],
            cwd=case["cwd"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE if separate else subprocess.STDOUT,
            text=True,
            env=env,
        )
    out = result.stdout + (result.stderr if separate else "")
    errors = []
    if separate and result.stdout.strip() != case["stdout"]:
        errors.append(f"stdout {result.stdout.strip()!r}, expected {case['stdout']!r}")
    if (result.returncode == 0) != case["ok"]:
        errors.append(f"exit {result.returncode}, expected {'0' if case['ok'] else 'failure'}")
    errors += [f"missing '{s}'" for s in case["has"] if s not in out]
    errors += [f"unexpected '{s}'" for s in case["lacks"] if s in out]
    errors += [f"'{s}' {out.count(s)} times" for s in case.get("once", ()) if out.count(s) != 1]
    return errors, out


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <path to mkn>", file=sys.stderr)
        sys.exit(2)
    mkn = os.path.abspath(sys.argv[1])
    failed = 0
    for case in CASES:
        errors, out = run(mkn, case)
        if errors:
            failed += 1
            print(f"FAIL {case['name']}: {', '.join(errors)}\n{out}")
        else:
            print(f"PASS {case['name']}")
    print(f"{len(CASES) - failed}/{len(CASES)} passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
