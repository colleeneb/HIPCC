/*
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef SRC_HIPBIN_SPIRV_H_
#define SRC_HIPBIN_SPIRV_H_

#include "hipBin_base.h"
#include "hipBin_util.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <cassert>

// Use (void) to silent unused warnengs.
#define assertm(exp, msg) assert(((void)msg, exp))

/**
 *
 * # Auto-generated by cmake on 2022-09-01T08:07:28Z UTC
 * HIP_PATH=/home/pvelesko/space/install/HIP/clang14/chip-spv-0.9
 * HIP_RUNTIME=spirv
 * HIP_COMPILER=clang
 * HIP_ARCH=spirv
 * HIP_OFFLOAD_COMPILE_OPTIONS=-D__HIP_PLATFORM_SPIRV__= -x hip
 * --target=x86_64-linux-gnu --offload=spirv64 -nohipwrapperinc
 * --hip-path=/home/pvelesko/space/install/HIP/clang14/chip-spv-0.9
 * HIP_OFFLOAD_LINK_OPTIONS=-L/home/pvelesko/space/install/HIP/clang14/chip-spv-0.9/lib
 * -lCHIP -L/soft/libraries/khronos/loader/master-2022.05.18/lib64
 * -L/soft/restricted/CNDA/emb/libraries/intel-level-zero/api_+_loader/20220614.1/lib64
 * -lOpenCL -lze_loader
 * -Wl,-rpath,/home/pvelesko/space/install/HIP/clang14/chip-spv-0.9/lib
 */

#define HIP_OFFLOAD_COMPILE_OPTIONS "HIP_OFFLOAD_COMPILE_OPTIONS"
#define HIP_OFFLOAD_LINK_OPTIONS "HIP_OFFLOAD_LINK_OPTIONS"
#define HIP_OFFLOAD_RDC_SUPPLEMENT_LINK_OPTIONS                                \
  "HIP_OFFLOAD_RDC_SUPPLEMENT_LINK_OPTIONS"

/**
 * @brief Container class for parsing and storing .hipInfo
 *
 */
class HipInfo {
public:
  string runtime = "";
  string cxxflags = "";
  string ldflags = "";
  string rdcSupplementLinkFlags = "";
  string clangpath = "";

  void parseLine(string line) {
    if (line.find(HIP_RUNTIME) != string::npos) {
      runtime = line.substr(string(HIP_RUNTIME).size() +
                            1); // add + 1 to account for =
    } else if (line.find(HIP_OFFLOAD_COMPILE_OPTIONS) != string::npos) {
      cxxflags = line.substr(string(HIP_OFFLOAD_COMPILE_OPTIONS).size() +
                             1); // add + 1 to account for =
    } else if (line.find(HIP_OFFLOAD_LINK_OPTIONS) != string::npos) {
      ldflags = line.substr(string(HIP_OFFLOAD_LINK_OPTIONS).size() +
                            1); // add + 1 to account for =
    } else if (line.find(HIP_OFFLOAD_RDC_SUPPLEMENT_LINK_OPTIONS) !=
               string::npos) {
      rdcSupplementLinkFlags =
          line.substr(string(HIP_OFFLOAD_RDC_SUPPLEMENT_LINK_OPTIONS).size() +
                      1); // add + 1 to account for =
    } else if (line.find(HIP_CLANG_PATH) != string::npos) {
      // TODO check if llvm-config exists here
      clangpath = line.substr(string(HIP_CLANG_PATH).size() +
                              1); // add + 1 to account for =
    } else {
    }

    return;
  }
};

/**
 * @brief A container class for representing a single argument to hipcc
 *
 */
class Argument {
private:
  // Should this arg be passed onto clang++ invocation
  bool passthrough_ = true;

public:
  // Is the argument present/enabled
  bool present = false;
  std::vector<string> matches;
  // Any additional arguments to this argument.
  // Example: "MyName" in `hipcc <...> -o MyName`
  string args;
  // Regex for which a match enables this argument
  regex regexp;

  Argument(){};
  Argument(string regexpIn) : regexp(regexpIn){};
  Argument(string regexpIn, bool passthrough)
      : passthrough_(passthrough), regexp(regexpIn){};
  /**
   * @brief Parse the compiler invocation and enable this argument
   *
   * @param argline string reprenting the campiler invocation
   */
  void parseLine(string &argline) {
    smatch match;
    std::string arglineCopy{argline.c_str()};
    while (regex_search(arglineCopy, match, regexp)) {
      present = true;

      // get the matched argument
      string arg = match[0].str();

      // remove leading and trailing whitespace
      arg.erase(0, arg.find_first_not_of(" \t\n\r"));
      arg.erase(arg.find_last_not_of(" \t\n\r") + 1);

      // string replace arg with nothing
      // arglineCopy.replace(arglineCopy.find(arg), arg.length(), "");
      std::string removeArgRegex = arg;
      if (arg.find('.') != string::npos)
        removeArgRegex.replace(removeArgRegex.find('.'), 1, "\\.");
      if (arg.find('/') != string::npos)
        removeArgRegex.replace(removeArgRegex.find('/'), 1, "\\/");
      arglineCopy = regex_replace(arglineCopy, regex(removeArgRegex), "");

      // if this arg is not meant to be passed on, remove it from the argline
      if (!passthrough_) {
        argline = regex_replace(argline, regex(removeArgRegex), "");
      }

      matches.push_back(arg);
    }
  }
};

/**
 * @brief Extarct all source files from argline. Here sources are defined as
 * anything that has a . in it but do not end in .o
 *
 * @param argline
 * @param del
 * @return std::vector<std::string>
 */
std::vector<std::string> extractSources(std::string &argline, bool del = true) {
  std::vector<std::string> sources;
  /*
  (         # Start of the capturing group
  (\\S+     # Match one or more non-whitespace characters (\\S matches a
  non-whitespace character, + specifies one or more times)
  \\.       # Match a literal dot character (\\. matches a dot, because a dot is
  a special character in regex)
  (?!o\\b)  # Negative lookahead assertion that asserts what immediately follows
  is not the character "o" followed by a word boundary (\\b is a word boundary)
  [^.\\s]+  # Match one or more characters that are neither a dot nor a
  whitespace ([^...] defines a negated character class, matching one character
  not in the set) )           # End of the capturing group
  */
  std::regex regexp("((\\S+\\.(?!o\\b)[^.\\s]+))");
  std::smatch match;
  while (std::regex_search(argline, match, regexp)) {
    // get the matched argument
    std::string arg = match[0].str();

    // remove leading and trailing whitespace
    arg.erase(0, arg.find_first_not_of(" \t\n\r"));
    arg.erase(arg.find_last_not_of(" \t\n\r") + 1);

    if (arg.find('.') != string::npos)
      arg.replace(arg.find('.'), 1, "\\.");
    // replace all instances of / with \\/
    size_t pos = 0;
    while ((pos = arg.find("/", pos)) != std::string::npos) {
      arg.replace(pos, 1, "\\/");
      pos += 3;
    }
    if (del)
      argline = std::regex_replace(argline, std::regex(arg), "");

    sources.push_back(arg);
  }
  return sources;
}

/**
 * @brief Parse -x <LANG> from argline, removing it from argline and all sources
 * following -x <LANG> option
 *
 * @param argLine string representing the compiler invocation
 * @return std::vector<std::string> 0th element contains <LANG>, rest are
 * sources
 */
std::vector<std::string> parseDashX(std::string &argLine) {
  /*
  \s          # Match a whitespace character
  -x          # Match the characters "-x" literally
  \s*         # Match zero or more whitespace characters (\s matches whitespace,
  * specifies zero or more times)
  (.+?)       # Capture one or more of any character (except for a line
  terminator), but as few as possible, into group 1 (. matches any character, +
  specifies one or more times, ? makes the + non-greedy)
  (?:         # Start of a non-capturing group (?:... specifies a non-capturing
  group) \s        # Match a whitespace character |           # OR $         #
  Match the end of the string )           # End of the non-capturing group
  */
  std::regex regexp("\\s-x\\s*(.+?)(?:\\s|$)");
  std::smatch match;
  std::vector<std::string> result;
  if (std::regex_search(argLine, match, regexp)) {
    // get the matched argument
    std::string arg = match[0].str();
    std::string lang = match[1].str();
    result.push_back(lang);

    // split & remove -x <LANG> from argline, split into before and after
    auto beforeDashX = argLine.substr(0, argLine.find(arg));
    auto afterDashX = argLine.substr(argLine.find(arg) + arg.length());

    // extract (and remove) sources from the part that comes after -x <LANG>
    auto sourcesAfterDashX = extractSources(afterDashX);
    // push sources to result
    result.insert(result.end(), sourcesAfterDashX.begin(),
                  sourcesAfterDashX.end());

    argLine = beforeDashX + afterDashX;
  }

  return result;
}

class CompilerOptions {
public:
  int verbose = 0; // 0x1=commands, 0x2=paths, 0x4=hipcc args
  // bool setStdLib = 0; // set if user explicitly requests -stdlib=libc++
  /**
   * Use compilation mode if any of these matches:
   * - [whitespace]-c[whitespace]
   * - [whitespace][whatever].cpp[whitespace]
   * - [whitespace][whatever].c[whitespace]
   * - [whitespace][whatever].hip[whitespace]
   * - [whitespace][whatever].cu[whitespace]
   */
  // \s(\w*?\.(cc|cpp))
  Argument sourcesC{
      "(?:\\s|^)[\\.a-zA-Z0-9_\\/-]+\\.(?:c)(?:\\s|$)",
      false}; // search for source files, removing them from the command line
  Argument sourcesCpp{
      "(?:\\s|^)[\\.a-zA-Z0-9_\\/-]+\\.(?:cc|cpp|hip|cu)(?!\\.o)",
      false}; // search for source files, removing them from the command line
  Argument
      sourcesHip; // if -x hip is not specified, all c++ sources are assumed to
                  // be HIP sources. Otherwise, this is populated by parseDashX
  Argument compileOnly{
      "(?:\\s|^)-c(?:\\s|$)",
      true}; // search for -c, removing it from the command line
  Argument outputObject{"\\s-o\\b"};         // search for -o
  Argument dashX; // processed by parseDashX
  Argument printHipVersion{"(?:\\s|^)--short-version\\b",
                           false};                         // print HIP version
  Argument printCXXFlags{"(?:\\s|^)--cxxflags\\b", false}; // print HIPCXXFLAGS
  Argument printLDFlags{"(?:\\s|^)--ldflags\\b", false};   // print HIPLDFLAGS
  Argument runCmd;
  Argument rdc{"(?:\\s|^)-fgpu-rdc\\b"}; // whether -fgpu-rdc is on
  Argument offload{"(?:\\s|^)--offload=[^\\s]+",
                   false}; // search for --offload=spirv64, removing it

  string processArgs(vector<string> argv, EnvVariables var) {
    argv.erase(argv.begin()); // remove clang++
    string argStr;
    for (auto arg : argv)
      argStr += " " + arg;

    if (!var.verboseEnv_.empty())
      verbose = stoi(var.verboseEnv_);

    // Kind of an edge case - if arguments are passed in with quotes everything
    // that goes after && and ; should be discrarded
    if (argStr.find("&&") != string::npos) {
      argStr = argStr.substr(0, argStr.find("&&"));
    }
    if (argStr.find(";") != string::npos) {
      argStr = argStr.substr(0, argStr.find(";"));
    }
    if (argStr.find(">") != string::npos) {
      argStr = argStr.substr(0, argStr.find(">"));
    }

    auto dashXresult = parseDashX(argStr);
    sourcesC.parseLine(argStr);
    sourcesCpp.parseLine(argStr);
    if (dashXresult.size()) {
      dashX.present = true;
      auto lang = dashXresult[0];
      // erase the first element
      dashXresult.erase(dashXresult.begin());
      if (lang == "c++") {
        sourcesCpp.present = true;
        sourcesCpp.matches.insert(sourcesCpp.matches.end(), dashXresult.begin(),
                                  dashXresult.end());
      } else if (lang == "c") {
        sourcesC.present = true;
        sourcesC.matches.insert(sourcesC.matches.end(), dashXresult.begin(),
                                dashXresult.end());
      } else if (lang == "hip") {
        sourcesHip.present = true;
        sourcesHip.matches.insert(sourcesHip.matches.end(), dashXresult.begin(),
                                  dashXresult.end());
      } else {
        // error out
        cout << "Error: -x " << lang << " is not supported" << endl;
        exit(-1);
      }
    }
    compileOnly.parseLine(argStr);
    outputObject.parseLine(argStr);

    printHipVersion.parseLine(argStr);
    printCXXFlags.parseLine(argStr);
    printLDFlags.parseLine(argStr);
    offload.parseLine(argStr);
    rdc.parseLine(argStr);
    if (printHipVersion.present || printCXXFlags.present ||
        printLDFlags.present) {
      runCmd.present = false;
    } else {
      runCmd.present = true;
    }
    return argStr;
  }
};
class HipBinSpirv : public HipBinBase {
private:
  HipBinUtil *hipBinUtilPtr_;
  string hipClangPath_ = "";
  PlatformInfo platformInfo_;
  string hipCFlags_, hipCXXFlags_, hipLdFlags_;
  HipInfo hipInfo_;

public:
  HipBinSpirv();
  virtual ~HipBinSpirv() = default;
  virtual bool detectPlatform();
  virtual void constructCompilerPath();
  virtual const string &getCompilerPath() const;
  virtual const PlatformInfo &getPlatformInfo() const;
  virtual string getCppConfig();
  virtual void printFull();
  virtual void printCompilerInfo() const;
  virtual string getCompilerVersion();
  virtual void checkHipconfig();
  virtual string getDeviceLibPath() const;
  virtual string getHipLibPath() const;
  virtual string getHipCC() const;
  virtual string getCompilerIncludePath();
  virtual string getHipInclude() const;
  virtual void initializeHipCXXFlags();
  virtual void initializeHipCFlags();
  virtual void initializeHipLdFlags();
  virtual const string &getHipCXXFlags() const;
  virtual const string &getHipCFlags() const;
  virtual const string &getHipLdFlags() const;
  virtual void executeHipCCCmd(vector<string> argv);

  bool readHipInfo(const string hip_path_share, HipInfo &result) {
    fs::path path(hip_path_share + "/.hipInfo");
    if (!fs::exists(path))
      return false;

    string hipInfoString;
    ifstream file(path);

    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        result.parseLine(line);
      }
      file.close();
    }

    return true;
  };
};

HipBinSpirv::HipBinSpirv() {
  PlatformInfo platformInfo;
  platformInfo.os = getOSInfo();

  platformInfo.platform = PlatformType::intel;
  platformInfo.runtime = RuntimeType::spirv;
  platformInfo.compiler = clang;
  platformInfo_ = platformInfo;

  return;
}

const string &HipBinSpirv::getHipCFlags() const { return hipCFlags_; }

const string &HipBinSpirv::getHipLdFlags() const { return hipLdFlags_; }

void HipBinSpirv::initializeHipLdFlags() {
  string hipLibPath;
  string hipLdFlags = hipInfo_.ldflags;
  // const string &hipClangPath = getCompilerPath();
  // // If $HIPCC clang++ is not compiled, use clang instead
  // string hipCC = "\"" + hipClangPath + "/clang++";

  // hipLibPath = getHipLibPath();
  // hipLdFlags += " -L\"" + hipLibPath + "\"";
  // const OsType &os = getOSInfo();

  hipLdFlags_ = hipLdFlags;
}

void HipBinSpirv::initializeHipCFlags() {
  // string hipCFlags = hipInfo_.cxxflags;
  // string hipclangIncludePath;
  // hipclangIncludePath = getHipInclude();
  // hipCFlags += " -isystem \"" + hipclangIncludePath + "\"";
  // const OsType &os = getOSInfo();
  hipCFlags_ = "-D__HIP_PLATFORM_SPIRV__";
  string hipIncludePath = getHipInclude();
  hipCFlags_ += " -isystem " + hipIncludePath;
}

const string &HipBinSpirv::getHipCXXFlags() const { return hipCXXFlags_; }

string HipBinSpirv::getHipInclude() const {
  fs::path hipIncludefs = getHipPath();
  hipIncludefs /= "include";
  string hipInclude = hipIncludefs.string();
  return hipInclude;
}

void HipBinSpirv::initializeHipCXXFlags() {
  string hipCXXFlags = hipInfo_.cxxflags;
  // const OsType &os = getOSInfo();
  // string hipClangIncludePath;
  // hipClangIncludePath = getCompilerIncludePath();
  // hipCXXFlags += " -isystem \"" + hipClangIncludePath;
  // fs::path hipCXXFlagsTempFs = hipCXXFlags;
  // hipCXXFlagsTempFs /= "..\"";
  // hipCXXFlags = hipCXXFlagsTempFs.string();
  // const EnvVariables &var = getEnvVariables();
  // // Allow __fp16 as function parameter and return type.
  // if (var.hipClangHccCompactModeEnv_.compare("1") == 0) {
  //   hipCXXFlags += " -Xclang -fallow-half-arguments-and-returns "
  //                  "-D__HIP_HCC_COMPAT_MODE__=1";
  // }

  // Add paths to common HIP includes:
  string hipIncludePath;
  //   hipIncludePath = getHipInclude();
  //   hipCXXFlags += " -isystem \"" + hipIncludePath;
  hipCXXFlags_ = hipCXXFlags;
}

// populates clang path.
void HipBinSpirv::constructCompilerPath() {
  string compilerPath = "";
  const EnvVariables &envVariables = getEnvVariables();

  // first check if HIP_CLANG_PATH is set and if so, then make sure llvm-config
  // can be found. This way user overrides always take precedence and get error
  // checked
  fs::path llvmPath = envVariables.hipClangPathEnv_;
  if (!llvmPath.empty()) {
    llvmPath /= "llvm-config";
    if (!fs::exists(llvmPath)) {
      cout << "Error: HIP_CLANG_PATH was set in the environment "
              "HIP_CLANG_PATH="
           << envVariables.hipClangPathEnv_
           << " but llvm-config was not found in " << llvmPath << endl;
      std::exit(EXIT_FAILURE);
    } else {
      hipClangPath_ = envVariables.hipClangPathEnv_;
      return;
    }

  } else {
    hipClangPath_ = hipInfo_.clangpath;
  }

  return;
}

// returns clang path.
const string &HipBinSpirv::getCompilerPath() const { return hipClangPath_; }

void HipBinSpirv::printCompilerInfo() const {
  const string &hipClangPath = getCompilerPath();

  cout << endl;

  string cmd = hipClangPath + "/clang++ --version";
  system(cmd.c_str()); // hipclang version
  cmd = hipClangPath + "/llc --version";
  system(cmd.c_str()); // llc version
  cout << "hip-clang-cxxflags :" << endl;
  cout << hipInfo_.cxxflags << endl;

  cout << endl << "hip-clang-ldflags :" << endl;
  cout << hipInfo_.ldflags << endl;

  cout << endl;
}

string HipBinSpirv::getCompilerVersion() {
  string out, complierVersion;
  const string &hipClangPath = getCompilerPath();
  fs::path cmd = hipClangPath;
  /**
   * Ubuntu systems do not provide this symlink clang++ -> clang++-14
   * Maybe use llvm-config instead?
   * $: llvm-config --version
   * $: 14.0.0
   */
  cmd += "/llvm-config";
  if (canRunCompiler(cmd.string(), out)) {
    regex regexp("([0-9.]+)");
    smatch m;
    if (regex_search(out, m, regexp)) {
      if (m.size() > 1) {
        // get the index =1 match, 0=whole match we ignore
        std::ssub_match sub_match = m[1];
        complierVersion = sub_match.str();
      }
    }
  } else {
    cout << "Hip Clang Compiler not found" << endl;
  }
  return complierVersion;
}

const PlatformInfo &HipBinSpirv::getPlatformInfo() const {
  return platformInfo_;
}

string HipBinSpirv::getCppConfig() { return hipInfo_.cxxflags; }

string HipBinSpirv::getDeviceLibPath() const { return ""; }

bool HipBinSpirv::detectPlatform() {
  if (getOSInfo() == windows) {
    return false;
  }

  bool detected = false;
  const EnvVariables &var = getEnvVariables();

  /**
   * 1. Check if .hipInfo is available in the ../share directory as I am
   *     a. Check .hipInfo for HIP_PLATFORM==spirv if so, return detected
   * 2. If HIP_PATH is set, check ${HIP_PATH}/share for .hipInfo
   *     b. If .hipInfo HIP_PLATFORM=spirv, return detected
   * 3. We haven't found .hipVars meaning we couldn't find a good installation
   * of CHIP-SPV. Check to see if the environment variables were not forcing
   * spirv and if so, return an error explaining what went wrong.
   */

  HipInfo hipInfo;
  fs::path currentBinaryPath = fs::canonical("/proc/self/exe");
  currentBinaryPath = currentBinaryPath.parent_path();
  fs::path sharePathBuild = currentBinaryPath.string() + "/../share";
  fs::path sharePathInstall =
      var.hipPathEnv_.empty() ? "" : var.hipPathEnv_ + "/share";
  if (readHipInfo( // 1.
          sharePathBuild, hipInfo)) {
    detected = hipInfo.runtime.compare("spirv") == 0; // b.
    hipInfo_ = hipInfo;
  } else if (!sharePathInstall.empty()) { // 2.
    if (readHipInfo(sharePathInstall, hipInfo)) {
      detected = hipInfo.runtime == "spirv";

      // check that HIP_RUNTIME found in .hipVars does not conflict with
      // HIP_RUNTIME in the env
      if (detected && !var.hipPlatformEnv_.empty()) {
        if (var.hipPlatformEnv_ != "spirv" && var.hipPlatformEnv_ != "intel") {
          cout << "Error: .hipVars was found in " << var.hipPathEnv_
               << " where HIP_PLATFORM=spirv which conflicts with HIP_PLATFORM "
                  "set in the current environment where HIP_PLATFORM="
               << var.hipPlatformEnv_ << endl;
          std::exit(EXIT_FAILURE);
        }
      }

      hipInfo_ = hipInfo;
    }
  }

  if (!detected && (var.hipPlatformEnv_ == "spirv" ||
                    var.hipPlatformEnv_ == "intel")) { // 3.
    if (var.hipPathEnv_.empty()) {
      cout << "Error: setting HIP_PLATFORM=spirv/intel requires setting "
              "HIP_PATH=/path/to/CHIP-SPV-INSTALL-DIR/"
           << endl;
      std::exit(EXIT_FAILURE);
    } else {
      cout << "Error: HIP_PLATFORM=" << var.hipPlatformEnv_
           << " was set but .hipInfo (generated during CHIP-SPV install) was "
              "not found in HIP_PATH="
           << var.hipPathEnv_ << "/share" << endl;
      std::exit(EXIT_FAILURE);
    }
  }

  // Because our compiler path gets read from hipInfo, we need to do this here
  constructCompilerPath();
  return detected;
}

string HipBinSpirv::getHipLibPath() const { return ""; }

string HipBinSpirv::getHipCC() const {
  string hipCC;
  const string &hipClangPath = getCompilerPath();
  fs::path compiler = hipClangPath;
  compiler /= "clang++";
  if (!fs::exists(compiler)) {
    fs::path compiler = hipClangPath;
    compiler /= "clang";
  }
  hipCC = compiler.string();
  return hipCC;
}

string HipBinSpirv::getCompilerIncludePath() {
  string hipClangVersion, includePath, compilerIncludePath;
  const string &hipClangPath = getCompilerPath();
  hipClangVersion = getCompilerVersion();
  fs::path includePathfs = hipClangPath;
  includePathfs = includePathfs.parent_path();
  includePathfs /= "lib/clang/";
  includePathfs /= hipClangVersion;
  includePathfs /= "include";
  includePathfs = fs::absolute(includePathfs).string();
  compilerIncludePath = includePathfs.string();
  return compilerIncludePath;
}

void HipBinSpirv::checkHipconfig() {
  printFull();
  cout << endl << "Check system installation: " << endl;
  cout << "check hipconfig in PATH..." << endl;
  if (system("which hipconfig > /dev/null 2>&1") != 0) {
    cout << "FAIL " << endl;
  } else {
    cout << "good" << endl;
  }
  string ldLibraryPath;
  const EnvVariables &env = getEnvVariables();
  ldLibraryPath = env.ldLibraryPathEnv_;
  const string &hsaPath(""); // = getHsaPath();
  cout << "check LD_LIBRARY_PATH (" << ldLibraryPath << ") contains HSA_PATH ("
       << hsaPath << ")..." << endl;
  if (ldLibraryPath.find(hsaPath) == string::npos) {
    cout << "FAIL" << endl;
  } else {
    cout << "good" << endl;
  }
}

void HipBinSpirv::printFull() {
  const string &hipVersion = getHipVersion();
  const string &hipPath = getHipPath();
  const PlatformInfo &platformInfo = getPlatformInfo();
  const string &ccpConfig = getCppConfig();
  const string &hsaPath(""); // = getHsaPath();
  const string &hipClangPath = getCompilerPath();

  cout << "HIP version: " << hipVersion << endl;
  cout << endl << "==hipconfig" << endl;
  cout << "HIP_PATH           :" << hipPath << endl;
  cout << "HIP_COMPILER       :" << CompilerTypeStr(platformInfo.compiler)
       << endl;
  cout << "HIP_PLATFORM       :" << PlatformTypeStr(platformInfo.platform)
       << endl;
  cout << "HIP_RUNTIME        :" << RuntimeTypeStr(platformInfo.runtime)
       << endl;
  cout << "CPP_CONFIG         :" << ccpConfig << endl;

  cout << endl << "==hip-clang" << endl;
  cout << "HIP_CLANG_PATH     :" << hipClangPath << endl;
  printCompilerInfo();
  cout << endl << "== Envirnoment Variables" << endl;
  printEnvironmentVariables();
  getSystemInfo();
  if (fs::exists("/usr/bin/lsb_release"))
    system("/usr/bin/lsb_release -a");
  cout << endl;
}

vector<string> excludedArgs{
    "--offload=spirv64",
    "-D__HIP_PLATFORM_SPIRV__",
    "-D__HIP_PLATFORM_SPIRV__=",
    "-D__HIP_PLATFORM_SPIRV__=1",
};

vector<string> argsFilter(vector<string> argsIn) {
  vector<string> argsOut;
  for (int i = 0; i < argsIn.size(); i++) {
    auto found = find(excludedArgs.begin(), excludedArgs.end(), argsIn[i]);
    if (found == excludedArgs.end())
      argsOut.push_back(argsIn[i]);
  }
  return argsOut;
}

void HipBinSpirv::executeHipCCCmd(vector<string> origArgv) {
  vector<string> argv;
  // Filter away a possible duplicate --offload=spirv64 flag which can be passed
  // in builds that call hipcc with direct output of hip_config --cpp_flags.
  // We have to include the flag in the --cpp_flags output to retain the
  // option to use clang++ directly for HIP compilation instead of hipcc.
  argv = argsFilter(origArgv);

  if (argv.size() < 2) {
    cout << "No Arguments passed, exiting ...\n";
    exit(EXIT_SUCCESS);
  }

  CompilerOptions opts;
  EnvVariables var = getEnvVariables();
  string processedArgs = opts.processArgs(argv, var);

  string toolArgs; // arguments to pass to the clang tool

  const OsType &os = getOSInfo();
  string hip_compile_cxx_as_hip;
  if (var.hipCompileCxxAsHipEnv_.empty()) {
    hip_compile_cxx_as_hip = "1";
  } else {
    hip_compile_cxx_as_hip = var.hipCompileCxxAsHipEnv_;
  }

  string HIPLDARCHFLAGS;

  initializeHipCXXFlags();
  initializeHipCFlags();
  initializeHipLdFlags();
  string HIPCXXFLAGS, HIPCFLAGS, HIPLDFLAGS;
  HIPCFLAGS = getHipCFlags();
  HIPCXXFLAGS = getHipCXXFlags();
  HIPLDFLAGS = getHipLdFlags();
  string hipLibPath;
  string hipclangIncludePath, hipIncludePath, deviceLibPath;
  hipLibPath = getHipLibPath();
  // const string &roccmPath = getRoccmPath();
  const string &hipPath = getHipPath();
  const PlatformInfo &platformInfo = getPlatformInfo();
  const string &rocclrHomePath(""); // = getRocclrHomePath();
  const string &hipClangPath = getCompilerPath();
  hipclangIncludePath = getCompilerIncludePath();
  hipIncludePath = getHipInclude();
  deviceLibPath = getDeviceLibPath();
  const string &hipVersion = getHipVersion();
  if (opts.verbose & 0x2) {
    cout << "HIP_PATH=" << hipPath << endl;
    cout << "HIP_PLATFORM=" << PlatformTypeStr(platformInfo.platform) << endl;
    cout << "HIP_COMPILER=" << CompilerTypeStr(platformInfo.compiler) << endl;
    cout << "HIP_RUNTIME=" << RuntimeTypeStr(platformInfo.runtime) << endl;
    cout << "HIP_CLANG_PATH=" << hipClangPath << endl;
    cout << "HIP_CLANG_INCLUDE_PATH=" << hipclangIncludePath << endl;
    cout << "HIP_INCLUDE_PATH=" << hipIncludePath << endl;
    cout << "HIP_LIB_PATH=" << hipLibPath << endl;
    cout << "DEVICE_LIB_PATH=" << deviceLibPath << endl;
    cout << "HIP_CXX_FLAGS=" << HIPCXXFLAGS << endl;
  }

  if (opts.verbose & 0x4) {
    cout << "hipcc-args: ";
    for (unsigned int i = 1; i < argv.size(); i++) {
      cout << argv.at(i) << " ";
    }
    cout << endl;
  }

  // Begin building the compilation command
  string CMD = getHipCC();
  CMD += "";

  // Add --hip-link only if it is compile only and -fgpu-rdc is on.
  if (opts.rdc.present && !opts.compileOnly.present &&
      !opts.sourcesCpp.present) {
    CMD += " " + hipInfo_.rdcSupplementLinkFlags;
  }

  // Link against CHIP if compileOnly not present
  if (!opts.compileOnly.present) {
    CMD += " " + HIPLDFLAGS;
  }

  if (!var.hipccCompileFlagsAppendEnv_.empty()) {
    HIPCXXFLAGS += " " + var.hipccCompileFlagsAppendEnv_ + " ";
    HIPCFLAGS += " " + var.hipccCompileFlagsAppendEnv_ + " ";
  }
  if (!var.hipccLinkFlagsAppendEnv_.empty()) {
    HIPLDFLAGS += " " + var.hipccLinkFlagsAppendEnv_ + " ";
  }

  if (opts.outputObject.present && !opts.compileOnly.present) {
    CMD += " " + HIPLDFLAGS;
  }

  CMD += " " + toolArgs;

  if (opts.printHipVersion.present) {
    if (opts.runCmd.present) {
      cout << "HIP version: ";
    }
    cout << hipVersion << endl;
  }
  if (opts.printCXXFlags.present) {
    cout << HIPCXXFLAGS;
  }
  if (opts.printLDFlags.present) {
    cout << HIPLDFLAGS;
  }

  // Always add HIP include path for hip_runtime_api.h
  CMD += "-I/" + hipIncludePath;

  // 1st arg is the full path to hipcc
  processedArgs = regex_replace(processedArgs, regex("\""), "\"\\\"");

  // append the remaining args
  CMD += " " + processedArgs + " ";

  // if -x was not found, assume all c++ sources are HIP
  if (opts.dashX.present == false) {
    opts.sourcesHip.present = true;
    opts.sourcesHip.matches = opts.sourcesCpp.matches;
    opts.sourcesCpp.present = false;
    opts.sourcesCpp.matches.clear();
  }
  if (opts.sourcesHip.present && opts.sourcesHip.matches.size() > 0) {
    std::string compileSources = " -x hip ";
    for (auto m : opts.sourcesHip.matches) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCXXFLAGS;
  }

  if (opts.sourcesCpp.present) {
    std::string compileSources = "-x c++ ";
    for (auto m : opts.sourcesCpp.matches) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCXXFLAGS;
  }

  if (opts.sourcesC.present) {
    std::string compileSources = " -x c ";
    for (auto m : opts.sourcesC.matches) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCFLAGS;
  }

  if (opts.verbose & 0x1) {
    cout << "hipcc-cmd: " << CMD << "\n";
  }

  if (opts.runCmd.present) {
    SystemCmdOut sysOut;
    sysOut = hipBinUtilPtr_->exec(CMD.c_str(), true);
    string cmdOut = sysOut.out;
    int CMD_EXIT_CODE = sysOut.exitCode;
    if (CMD_EXIT_CODE != 0) {
      cout << "failed to execute:" << CMD << std::endl;
    }
    exit(CMD_EXIT_CODE);
  } // end of runCmd section
} // end of function

#endif // SRC_HIPBIN_SPIRV_H_
