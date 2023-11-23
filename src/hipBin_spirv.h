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
public:
  bool present = false;
  std::vector<string> values;
  Argument(){};
  Argument(bool presentIn) : present(presentIn){};
};

class CompilerOptions {
public:
  int verbose = 0x0; // 0x1=commands, 0x2=paths, 0x4=hipcc args
  // bool setStdLib = 0; // set if user explicitly requests -stdlib=libc++
  Argument sourcesC;
  Argument sourcesCpp;
  Argument sourcesObj;
  Argument sourcesHip;
  Argument compileOnly;
  Argument outputObject;
  Argument dashX;
  Argument printHipVersion;
  Argument printCXXFlags;
  Argument printLDFlags;
  Argument runCmd{true};
  Argument rdc;
  Argument offload;
  Argument linkOnly;
  Argument MT;
  Argument MF;
  vector<string> defaultSources;
  vector<string> cSources;
  vector<string> cppSources;
  vector<string> hipSources;

  /**
   * @brief Pre-process given command line args to make parsing easier
   * 1. replace all instances of multiple whitespaces with one
   * 2. convert -x <lang> to -x<lang>
   *
   * @param argv command line arguments
   * @return vector<string>
   */
  vector<string> preprocessArgs(const vector<string> &argv) {
    string argvStr;
    for (auto arg : argv) {
      argvStr += " " + arg;
    }

    // replace all instances of multiple whitespaces with one
    argvStr = regex_replace(argvStr, regex("\\s+"), " ");

    // convert -x <lang> to -x<lang>
    argvStr = regex_replace(argvStr, regex("\\s-x\\s"), " -x");

    // convert argvStr back to array by splitting on whitespace
    vector<string> argvNew;
    std::istringstream iss(argvStr);
    for (std::string s; iss >> s;)
      argvNew.push_back(s);

    return argvNew;
  }

  /**
   * @brief process arguments and set flags for what to do
   * Handle the cases where options take an argument such as -o <file>, -MT
   * <file>, -MF <file>
   *
   * @param argv
   * @return vector<string>
   */
  vector<string> processArgs(const vector<string> &argv) {
    vector<string> remainingArgs;
    string prevArg = "";
    for (auto arg : argv) {
      if (arg == "-c") {
        compileOnly.present = true;
        remainingArgs.push_back(arg);
      } else if (arg == "--offload=spirv64") {
        offload.present = true;
      } else if (arg == "-fgpu-rdc") {
        rdc.present = true;
        remainingArgs.push_back(arg);
      } else if (arg == "--short-version") {
        runCmd.present = false;
        printHipVersion.present = true;
      } else if (arg == "--cxxflags") {
        runCmd.present = false;
        printCXXFlags.present = true;
      } else if (arg == "--ldflags") {
        runCmd.present = false;
        printLDFlags.present = true;
      } else if (arg == "-o") {
        prevArg = arg;
        continue; // don't pass it on
      } else if (prevArg == "-o") {
        outputObject.present = true;
        outputObject.values.push_back("-o " + arg);
      } else if (arg == "-MT") {
        prevArg = arg;
        continue; // don't pass it on
      } else if (prevArg == "-MT") {
        MT.present = true;
        MT.values.push_back("-MT " + arg);
      } else if (arg == "-MF") {
        prevArg = arg;
        continue; // don't pass it on
      } else if (prevArg == "-MF") {
        MF.present = true;
        MF.values.push_back("-MF " + arg);
      } else {
        // pass through all other arguments
        remainingArgs.push_back(arg);
      }

      prevArg = arg;
    } // end arg loop

    return remainingArgs;
  }

  bool argIsXSource(const string &arg, const vector<string> &extensions) {
    for (const auto &ext : extensions) {
      auto substridx = std::max(0, (int)arg.size() - (int)ext.size());
      if (arg.substr(substridx) == ext)
        return true;
    }

    return false;
  }

  bool argIsCppSource(const string &arg) {
    return argIsXSource(arg, {".cpp", ".cxx", ".cc"});
  }

  bool argIsHipSource(const string &arg) {
    return argIsXSource(arg, {".hip", ".cu"});
  }

  bool argIsCSource(const string &arg) { return argIsXSource(arg, {".c"}); }

  bool argIsObject(const string &arg) { return argIsXSource(arg, {".o"}); }

  /**
   * @brief Given an array of arugments, extract the sources and classify them
   * expecting that -x <lang> has been converted to -x<lang>
   * @param argv
   * @return vector<string>
   */
  vector<string> processSources(const vector<string> &argv) {
    vector<string> remainingArgs;
    bool parsingDashXc = false;
    bool parsingDashXcpp = false;
    bool parsingDashXhip = false;

    for (auto arg : argv) {
      if (arg == "-xc") {
        sourcesC.present = true;
        parsingDashXc = true;
        dashX.present = true;
      } else if (arg == "-xc++") {
        sourcesCpp.present = true;
        parsingDashXcpp = true;
        dashX.present = true;
      } else if (arg == "-xhip") {
        sourcesHip.present = true;
        parsingDashXhip = true;
        dashX.present = true;
      } else if (arg == "-x") {
        assert(!"Error: -x <lang> should have been converted to -x<lang>");
      } else if (parsingDashXc) {
        sourcesC.values.push_back(arg);
      } else if (parsingDashXcpp) {
        sourcesCpp.values.push_back(arg);
      } else if (parsingDashXhip) {
        sourcesHip.values.push_back(arg);
        // dealt with -x cases, now deal with everything else

      } else if (argIsCSource(arg)) {
        sourcesC.present = true;
        sourcesC.values.push_back(arg);
      } else if (argIsCppSource(arg)) {
        sourcesCpp.present = true;
        sourcesCpp.values.push_back(arg);
      } else if (argIsHipSource(arg)) {
        sourcesHip.present = true;
        sourcesHip.values.push_back(arg);
      } else if (argIsObject(arg)) {
        sourcesObj.present = true;
        sourcesObj.values.push_back(arg);
      } else {
        remainingArgs.push_back(arg);
      }
    } // end arg loop

    // check if we need to compile anything, if not, linkOnly is true
    if (!sourcesC.present && !sourcesCpp.present && !sourcesHip.present) {
      linkOnly.present = true;
    }

    // if -x was not found, assume all c++ sources are HIP
    if (dashX.present == false) {
      sourcesHip.present = true;
      sourcesHip.values.insert(sourcesHip.values.end(),
                               sourcesCpp.values.begin(),
                               sourcesCpp.values.end());
      sourcesCpp.present = false;
      sourcesCpp.values.clear();
    }

    return remainingArgs;
  }
};
class HipBinSpirv : public HipBinBase {
private:
  HipBinUtil *hipBinUtilPtr_;
  string hipClangPath_ = "";
  PlatformInfo platformInfo_;
  string hipCFlags_, hipCXXFlags_, hipLdFlags_, fixupHeader_;
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

  hipLdFlags_ = hipLdFlags;
}

void HipBinSpirv::initializeHipCFlags() {
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

  std::smatch Match;
  std::regex R("-include ([\\S]+)/hip/spirv_fixups.h");
  bool HasMatch = regex_search(hipCXXFlags, Match, R);
  if (HasMatch) {
    fixupHeader_ = Match[0].str();
    hipCXXFlags = hipBinUtilPtr_->replaceStr(hipCXXFlags, fixupHeader_, " ");
  }

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

/**
 * @brief Filter away possible duplicates of --offload=spirv64 and
 * -D__HIP_PLATFORM_SPIRV__ flags which can be passed in builds that call
 * hipcc with direct output of hip_config --cpp_flags. We have to include the
 * flag in the --cpp_flags output to retain the option to use clang++ directly
 * for HIP compilation instead of hipcc.
 *
 * @param argsIn
 * @return vector<string>
 */
vector<string> argsFilter(const vector<string> &argsIn) {
  vector<string> excludedArgs{
      "--offload=spirv64",
      "-D__HIP_PLATFORM_SPIRV__",
      "-D__HIP_PLATFORM_SPIRV__=",
      "-D__HIP_PLATFORM_SPIRV__=1",
  };

  vector<string> argsOut;
  for (int i = 0; i < argsIn.size(); i++) {
    auto found = find(excludedArgs.begin(), excludedArgs.end(), argsIn[i]);
    if (found == excludedArgs.end())
      argsOut.push_back(argsIn[i]);
  }
  return argsOut;
}

void HipBinSpirv::executeHipCCCmd(vector<string> argv) {
  if (argv.size() < 2) {
    cout << "No Arguments passed, exiting ...\n";
    exit(EXIT_SUCCESS);
  }

  // filter out chipStar flags that could have been passed in from hipConfig
  argv = argsFilter(argv);

  // drop the first argument as it's the name of the binary
  argv.erase(argv.begin());

  // for generating dgb arg list
  // cout << "INVOKING HIPCC DEBUG:\n";
  // for (auto arg : argv) {
  //   cout << "\"" << arg << "\", ";
  // }
  // cout << endl;

  CompilerOptions opts;
  EnvVariables var = getEnvVariables();
  if (!var.verboseEnv_.empty())
    opts.verbose = stoi(var.verboseEnv_);

  // trim whitespace, convert -x <lang> to -x<lang>
  argv = opts.preprocessArgs(argv);

  // check arguments to figure out if we need to compile, link, or both + other
  auto processedArgs = opts.processArgs(argv);

  // parse sources handling -x<lang> cases
  processedArgs = opts.processSources(processedArgs);

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
  if (!var.hipccCompileFlagsAppendEnv_.empty()) {
    HIPCXXFLAGS += " " + var.hipccCompileFlagsAppendEnv_ + " ";
    HIPCFLAGS += " " + var.hipccCompileFlagsAppendEnv_ + " ";
  }
  if (!var.hipccLinkFlagsAppendEnv_.empty()) {
    HIPLDFLAGS += " " + var.hipccLinkFlagsAppendEnv_ + " ";
  }

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

  // Add --hip-link only if it is link only and -fgpu-rdc is on.
  if (opts.rdc.present && opts.linkOnly.present) {
    CMD += " " + hipInfo_.rdcSupplementLinkFlags;
  }

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

  if (!fixupHeader_.empty()) {
    CMD += " " + fixupHeader_;
  }

  // always add HIP include path for hip_runtime_api.h
  CMD += " -I/" + hipIncludePath;

  // append all user provided arguments that weren't handled
  for (auto arg : processedArgs)
    CMD += " " + arg;

  // append all objects
  for (auto obj : opts.sourcesObj.values) {
    CMD += " " + obj;
  }

  if (opts.sourcesHip.present && opts.sourcesHip.values.size() > 0) {
    std::string compileSources = " -x hip ";
    for (auto m : opts.sourcesHip.values) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCXXFLAGS;
  }

  if (opts.sourcesCpp.present) {
    std::string compileSources = " -x c++ ";
    for (auto m : opts.sourcesCpp.values) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCXXFLAGS;
  }

  if (opts.sourcesC.present) {
    std::string compileSources = " -x c ";
    for (auto m : opts.sourcesC.values) {
      compileSources += m + " ";
    }
    CMD += compileSources;
    CMD += HIPCFLAGS;
  }

  if (opts.outputObject.present) {
    CMD += " " + opts.outputObject.values[0];
  }

  if (!opts.compileOnly.present) {
    CMD += " " + HIPLDFLAGS;
  }

  if (opts.MT.present) {
    CMD += " " + opts.MT.values[0];
  }

  if (opts.MF.present) {
    CMD += " " + opts.MF.values[0];
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
