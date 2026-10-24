#include "gpi/build_contract.hpp"
#include "gpi/filesystem_safety.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace genomic::gpi {
namespace {
std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return {};
    return {std::istreambuf_iterator<char>(in),{}};
}
std::string lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return value;
}
std::string trim(std::string value) {
    const auto first=value.find_first_not_of(" \t\r\n\"");
    if(first==std::string::npos)return {};
    const auto last=value.find_last_not_of(" \t\r\n\"");
    return value.substr(first,last-first+1);
}
std::vector<std::string> words(const std::string& value) {
    std::vector<std::string> result;
    std::string current;
    bool quoted=false;
    for(const char c:value) {
        if(c=='"'){quoted=!quoted;continue;}
        if(!quoted&&std::isspace(static_cast<unsigned char>(c))) {
            if(!current.empty()){result.push_back(current);current.clear();}
        } else current+=c;
    }
    if(!current.empty())result.push_back(current);
    return result;
}
void makeUnique(std::vector<std::string>& values) {
    std::sort(values.begin(),values.end());
    values.erase(std::unique(values.begin(),values.end()),values.end());
}
std::string hex(const std::string& value) {
    static constexpr char digits[]="0123456789abcdef";
    std::string result;result.reserve(value.size()*2);
    for(const unsigned char c:value){result+=digits[c>>4];result+=digits[c&15];}
    return result;
}
std::string unhex(const std::string& value) {
    if(value.size()%2!=0)throw std::runtime_error("Corrupt GDIBC1 hex field");
    auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);throw std::runtime_error("Corrupt GDIBC1 hex digit");};
    std::string result;result.reserve(value.size()/2);
    for(std::size_t i=0;i<value.size();i+=2)result+=static_cast<char>((digit(value[i])<<4)|digit(value[i+1]));
    return result;
}
std::vector<std::string> split(const std::string& value,char delimiter) {
    std::vector<std::string> result;std::size_t begin=0;
    while(begin<=value.size()){const auto end=value.find(delimiter,begin);result.push_back(value.substr(begin,end==std::string::npos?end:end-begin));if(end==std::string::npos)break;begin=end+1;}
    return result;
}
std::uint64_t stableHash(std::string_view value) {
    std::uint64_t hash=1469598103934665603ULL;
    for(const unsigned char c:value){hash^=c;hash*=1099511628211ULL;}
    return hash;
}
std::uint64_t parseUnsigned(const std::string& value) {
    std::uint64_t result{};const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::runtime_error("Corrupt GDIBC1 integer");
    return result;
}
ContractValue contractValue(std::string value,ContractEvidence evidence,std::string origin) {
    return {std::move(value),evidence,std::move(origin)};
}
bool buildOutputDirectory(std::string_view first) {
    const auto name=lower(std::string(first));
    return name=="build"||name=="dist"||name=="out"||name=="bin"||name=="obj"||name==".git"||name==".vs"||name=="kr_build"||name.starts_with("cmake-build-");
}
void copyProject(const std::filesystem::path& source,const std::filesystem::path& target) {
    for(auto iterator=std::filesystem::recursive_directory_iterator(source,std::filesystem::directory_options::none);iterator!=std::filesystem::recursive_directory_iterator();++iterator) {
        const auto relative=iterator->path().lexically_relative(source);
        if(relative.empty())continue;
        const auto first=relative.begin()->string();
        if(iterator->is_directory()&&buildOutputDirectory(first)){iterator.disable_recursion_pending();continue;}
        if(buildOutputDirectory(first))continue;
        if(iterator->is_symlink())throw std::runtime_error("Foreign project contains a symbolic link or junction: "+iterator->path().string());
        const auto destination=target/relative;
        if(iterator->is_directory())std::filesystem::create_directories(destination);
        else if(iterator->is_regular_file()){
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(iterator->path(),destination,std::filesystem::copy_options::overwrite_existing);
        }
    }
}
std::optional<std::string> cacheValue(const std::string& text,const std::string& key) {
    std::istringstream input(text);std::string line;
    while(std::getline(input,line))if(!line.empty()&&line.front()!='#'&&line.front()!='/'&&line.starts_with(key+":")){
        const auto equals=line.find('=');if(equals!=std::string::npos)return trim(line.substr(equals+1));
    }
    return {};
}
void setFromCache(const std::string& text,const std::string& key,ContractValue& destination,const std::string& origin) {
    if(const auto found=cacheValue(text,key))destination=contractValue(*found,ContractEvidence::Discovered,origin);
}
std::optional<std::string> jsonString(const std::string& text,const std::string& key) {
    std::smatch match;
    const std::regex expression("\\\""+key+"\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"",std::regex::icase);
    if(std::regex_search(text,match,expression))return match[1].str();
    return {};
}
std::optional<std::string> cmakeSetValue(const std::string& text,const std::string& key) {
    std::smatch match;const std::regex expression("set\\s*\\(\\s*"+key+"\\s+\\\"?([^\\\"\\)]+)",std::regex::icase);
    if(std::regex_search(text,match,expression))return trim(match[1].str());
    return {};
}
std::string jsonEscape(const std::string& value) {
    std::string result;
    for(const unsigned char c:value){if(c=='"'||c=='\\')result+='\\';if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else if(c>=32)result+=static_cast<char>(c);}
    return result;
}
void addTokens(std::vector<std::string>& output,const std::string& text) {
    for(auto token:words(text))if(token!="PRIVATE"&&token!="PUBLIC"&&token!="INTERFACE"&&token!="BEFORE"&&token!="SYSTEM"&&!token.empty())output.push_back(std::move(token));
}
BuildTargetContract* findTarget(BuildContract& contract,const std::string& name) {
    const auto found=std::find_if(contract.targets.begin(),contract.targets.end(),[&](const auto& target){return target.name==name;});
    return found==contract.targets.end()?nullptr:&*found;
}
void parseCMakeSource(const std::filesystem::path& file,const std::filesystem::path& root,BuildContract& contract) {
    const auto text=readFile(file);const auto origin=file.lexically_relative(root).generic_string();
    std::unordered_map<std::string,std::vector<std::string>> variables;
    const std::regex setExpression(R"(set\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+([^\)]*)\))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),setExpression),end;iterator!=end;++iterator)variables[(*iterator)[1].str()]=words((*iterator)[2].str());
    std::smatch match;
    if(std::regex_search(text,match,std::regex(R"(cmake_minimum_required\s*\(\s*VERSION\s+([^\s\)]+))",std::regex::icase)))contract.buildSystemVersion=contractValue(match[1].str(),ContractEvidence::Discovered,origin);
    if(std::regex_search(text,match,std::regex(R"((?:CMAKE_CXX_STANDARD\s+|cxx_std_)([0-9]+))",std::regex::icase)))contract.languageStandard=contractValue("C++"+match[1].str(),ContractEvidence::Discovered,origin);
    const std::regex targetExpression(R"(add_(executable|library)\s*\(\s*([^\s\)]+)\s*([^\)]*)\))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),targetExpression),end;iterator!=end;++iterator){
        BuildTargetContract target;target.name=(*iterator)[2].str();target.kind=lower((*iterator)[1].str());const auto targetWords=words((*iterator)[3].str());
        if(target.kind=="library"&&!targetWords.empty()&&targetWords.front()=="SHARED")target.kind="shared_library";
        else if(target.kind=="library"&&!targetWords.empty()&&targetWords.front()=="MODULE")target.kind="module_library";
        for(auto token:targetWords){
            if(token=="STATIC"||token=="SHARED"||token=="MODULE"||token=="WIN32"||token=="MACOSX_BUNDLE"||token=="EXCLUDE_FROM_ALL")continue;
            if(token.size()>3&&token.starts_with("${")&&token.back()=='}'){
                const auto variable=token.substr(2,token.size()-3);if(variables.contains(variable))target.sources.insert(target.sources.end(),variables[variable].begin(),variables[variable].end());
            } else target.sources.push_back(token);
        }
        contract.targets.push_back(std::move(target));
    }
    const auto parseTargetCommand=[&](const std::regex& expression,auto consumer){
        for(std::sregex_iterator iterator(text.begin(),text.end(),expression),end;iterator!=end;++iterator)if(auto* target=findTarget(contract,(*iterator)[1].str()))consumer(*target,(*iterator)[2].str());
    };
    parseTargetCommand(std::regex(R"(target_link_libraries\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract& target,const std::string& body){addTokens(target.linkLibraries,body);for(const auto& token:target.linkLibraries){contract.linkLibraries.push_back(token);target.dependencies.push_back(token);contract.graph.push_back({target.name,token,"links",origin});}});
    parseTargetCommand(std::regex(R"(target_include_directories\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract&,const std::string& body){addTokens(contract.includeDirectories,body);});
    parseTargetCommand(std::regex(R"(target_compile_definitions\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract&,const std::string& body){addTokens(contract.definitions,body);});
    parseTargetCommand(std::regex(R"(target_compile_options\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract&,const std::string& body){addTokens(contract.compileOptions,body);});
    parseTargetCommand(std::regex(R"(target_compile_features\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract&,const std::string& body){addTokens(contract.compileFeatures,body);});
    parseTargetCommand(std::regex(R"(target_link_options\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase),[&](BuildTargetContract&,const std::string& body){addTokens(contract.linkOptions,body);});
    const std::regex testExpression(R"(add_test\s*\(\s*(?:NAME\s+)?([^\s\)]+))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),testExpression),end;iterator!=end;++iterator)contract.testTargets.push_back((*iterator)[1].str());
    const std::regex customExpression(R"(add_custom_(?:command|target)\s*\(([^\)]*)\))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),customExpression),end;iterator!=end;++iterator){const auto command=trim((*iterator)[1].str());contract.customCommands.push_back(command);if(command.find("..")!=std::string::npos)contract.conflicts.push_back("Custom command contains parent traversal: "+origin);}
    const std::regex generatedExpression(R"(configure_file\s*\(\s*[^\s\)]+\s+([^\s\)]+))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),generatedExpression),end;iterator!=end;++iterator)contract.generatedFiles.push_back((*iterator)[1].str());
    const std::regex dependencyExpression(R"(add_dependencies\s*\(\s*([^\s\)]+)\s+([^\)]*)\))",std::regex::icase);
    for(std::sregex_iterator iterator(text.begin(),text.end(),dependencyExpression),end;iterator!=end;++iterator)if(auto* target=findTarget(contract,(*iterator)[1].str()))for(const auto& dependency:words((*iterator)[2].str())){target->dependencies.push_back(dependency);contract.graph.push_back({target->name,dependency,"depends",origin});}
    for(auto& target:contract.targets)for(const auto& source:target.sources)contract.graph.push_back({target.name,source,"compiles",origin});
}
std::string combinedLogs(const NativeBuildResult& result) {
    std::string output;for(const auto& step:result.steps){output+=readFile(step.stdoutLog);output+='\n';output+=readFile(step.stderrLog);output+='\n';}return output;
}
NativeBuildStep executeStep(std::string name,std::vector<std::string> command,const std::filesystem::path& cwd,const std::filesystem::path& logs,std::size_t timeout) {
    NativeBuildStep step;step.name=std::move(name);step.command=std::move(command);step.stdoutLog=logs/(step.name+".stdout.log");step.stderrLog=logs/(step.name+".stderr.log");
    step.process=runProcess(step.command,cwd,{},step.stdoutLog,step.stderrLog,std::chrono::milliseconds(timeout));return step;
}
void recordValue(std::ostringstream& output,const std::string& key,const ContractValue& value) {
    output<<"VALUE|"<<key<<'|'<<static_cast<int>(value.evidence)<<'|'<<hex(value.value)<<'|'<<hex(value.origin)<<'\n';
}
void recordVector(std::ostringstream& output,const std::string& key,const std::vector<std::string>& values) {
    for(const auto& value:values)output<<"LIST|"<<key<<'|'<<hex(value)<<'\n';
}
ContractValue* valueByName(BuildContract& contract,const std::string& name) {
    if(name=="buildSystemVersion")return &contract.buildSystemVersion;
    if(name=="generator")return &contract.generator;
    if(name=="generatorPlatform")return &contract.generatorPlatform;
    if(name=="compilerFamily")return &contract.compilerFamily;
    if(name=="compilerVersion")return &contract.compilerVersion;
    if(name=="targetArchitecture")return &contract.targetArchitecture;
    if(name=="cCompiler")return &contract.cCompiler;
    if(name=="cppCompiler")return &contract.cppCompiler;
    if(name=="rcCompiler")return &contract.rcCompiler;
    if(name=="windowsSdk")return &contract.windowsSdk;
    if(name=="toolchainFile")return &contract.toolchainFile;
    if(name=="languageStandard")return &contract.languageStandard;
    if(name=="workingDirectory")return &contract.workingDirectory;
    return nullptr;
}
std::vector<std::string>* vectorByName(BuildContract& contract,const std::string& name) {
    if(name=="configurations")return &contract.configurations;
    if(name=="includeDirectories")return &contract.includeDirectories;
    if(name=="definitions")return &contract.definitions;
    if(name=="compileFeatures")return &contract.compileFeatures;
    if(name=="compileOptions")return &contract.compileOptions;
    if(name=="linkDirectories")return &contract.linkDirectories;
    if(name=="linkOptions")return &contract.linkOptions;
    if(name=="linkLibraries")return &contract.linkLibraries;
    if(name=="staticLibraries")return &contract.staticLibraries;
    if(name=="dynamicLibraries")return &contract.dynamicLibraries;
    if(name=="executables")return &contract.executables;
    if(name=="resources")return &contract.resources;
    if(name=="generatedFiles")return &contract.generatedFiles;
    if(name=="preBuildCommands")return &contract.preBuildCommands;
    if(name=="postBuildCommands")return &contract.postBuildCommands;
    if(name=="customCommands")return &contract.customCommands;
    if(name=="testTargets")return &contract.testTargets;
    if(name=="runtimeDependencies")return &contract.runtimeDependencies;
    if(name=="discoveryEvidence")return &contract.discoveryEvidence;
    if(name=="conflicts")return &contract.conflicts;
    return nullptr;
}
}

const char* toString(ContractEvidence value) noexcept {switch(value){case ContractEvidence::Discovered:return "DISCOVERED";case ContractEvidence::Inferred:return "INFERRED";case ContractEvidence::Verified:return "VERIFIED";case ContractEvidence::Unavailable:return "UNAVAILABLE";case ContractEvidence::Conflicting:return "CONFLICTING";}return "UNAVAILABLE";}
const char* toString(BuildSystemKind value) noexcept {switch(value){case BuildSystemKind::CMake:return "CMake";case BuildSystemKind::MSBuild:return "MSBuild";case BuildSystemKind::Ninja:return "Ninja";case BuildSystemKind::Make:return "Make";case BuildSystemKind::DirectCompiler:return "DirectCompiler";case BuildSystemKind::KrUnified:return "KRUnified";case BuildSystemKind::Mixed:return "Mixed";case BuildSystemKind::Unknown:return "Unknown";}return "Unknown";}
const char* toString(BuildBaselineStatus value) noexcept {switch(value){case BuildBaselineStatus::BuildContractUnverified:return "BUILD_CONTRACT_UNVERIFIED";case BuildBaselineStatus::BuildContractVerified:return "BUILD_CONTRACT_VERIFIED";case BuildBaselineStatus::BaselineReproduced:return "BASELINE_REPRODUCED";case BuildBaselineStatus::BaselineFailedProjectDefect:return "BASELINE_FAILED_PROJECT_DEFECT";case BuildBaselineStatus::BaselineBlockedEnvironment:return "BASELINE_BLOCKED_ENVIRONMENT";case BuildBaselineStatus::BaselineBlockedToolchain:return "BASELINE_BLOCKED_TOOLCHAIN";case BuildBaselineStatus::BaselineInconclusive:return "BASELINE_INCONCLUSIVE";}return "BASELINE_INCONCLUSIVE";}

BuildContract discoverBuildContract(const std::filesystem::path& projectRoot) {
    if(!std::filesystem::is_directory(projectRoot))throw std::invalid_argument("Build contract root is not a directory");
    BuildContract contract;contract.projectRoot=std::filesystem::canonical(projectRoot);contract.workingDirectory=contractValue(".",ContractEvidence::Discovered,"project root");
    bool cmake=false,msbuild=false,ninja=false,make=false,kr=false;std::vector<std::filesystem::path> cmakeSources,caches,presets,compilerMetadata,msbuildProjects;std::vector<std::string> directSources;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(contract.projectRoot,std::filesystem::directory_options::skip_permission_denied)){
        if(!entry.is_regular_file())continue;
        const auto relative=entry.path().lexically_relative(contract.projectRoot);const auto rel=relative.generic_string();const auto name=lower(entry.path().filename().string());const auto extension=lower(entry.path().extension().string());const bool generated=!relative.empty()&&buildOutputDirectory(relative.begin()->string());
        if(name=="cmakelists.txt"){cmake=true;cmakeSources.push_back(entry.path());contract.discoveryEvidence.push_back(rel);}
        else if(name=="cmakepresets.json"||name=="cmakeuserpresets.json"){cmake=true;presets.push_back(entry.path());contract.discoveryEvidence.push_back(rel);}
        else if(name=="cmakecache.txt"){caches.push_back(entry.path());contract.discoveryEvidence.push_back(rel);}
        if(name=="cmakecxxcompiler.cmake")compilerMetadata.push_back(entry.path());
        if((extension==".sln"||extension==".vcxproj")&&!generated){msbuild=true;contract.discoveryEvidence.push_back(rel);if(extension==".vcxproj")msbuildProjects.push_back(entry.path());}
        if(name=="build.ninja"&&!generated){ninja=true;contract.discoveryEvidence.push_back(rel);}
        if((name=="makefile"||name=="gnumakefile")&&!generated){make=true;contract.discoveryEvidence.push_back(rel);}
        if(extension==".kr"||extension==".krproject"){kr=true;contract.discoveryEvidence.push_back(rel);}
        if(extension==".cpp"||extension==".cc"||extension==".cxx")if(!generated)directSources.push_back(rel);
        if(extension==".rc")contract.resources.push_back(rel);
    }
    const int primary=static_cast<int>(cmake)+static_cast<int>(msbuild)+static_cast<int>(ninja)+static_cast<int>(make)+static_cast<int>(kr);
    if(primary>1)contract.buildSystem=BuildSystemKind::Mixed;else if(kr)contract.buildSystem=BuildSystemKind::KrUnified;else if(cmake)contract.buildSystem=BuildSystemKind::CMake;else if(msbuild)contract.buildSystem=BuildSystemKind::MSBuild;else if(ninja)contract.buildSystem=BuildSystemKind::Ninja;else if(make)contract.buildSystem=BuildSystemKind::Make;else contract.buildSystem=directSources.empty()?BuildSystemKind::Unknown:BuildSystemKind::DirectCompiler;
    for(const auto& file:cmakeSources)parseCMakeSource(file,contract.projectRoot,contract);
    for(const auto& project:msbuildProjects){
        const auto text=readFile(project);const auto origin=project.lexically_relative(contract.projectRoot).generic_string();
        BuildTargetContract target;target.name=project.stem().string();
        target.kind=text.find("DynamicLibrary")!=std::string::npos?"shared_library":
            (text.find("StaticLibrary")!=std::string::npos?"library":"executable");
        const std::regex sourceExpression("<ClCompile\\s+Include=\\\"([^\\\"]+)\\\"",std::regex::icase);
        for(std::sregex_iterator iterator(text.begin(),text.end(),sourceExpression),end;iterator!=end;++iterator)
            target.sources.push_back((*iterator)[1].str());
        contract.targets.push_back(std::move(target));std::smatch match;
        if(std::regex_search(text,match,std::regex(R"(<WindowsTargetPlatformVersion>([^<]+))",std::regex::icase)))
            contract.windowsSdk=contractValue(match[1].str(),ContractEvidence::Discovered,origin);
        if(std::regex_search(text,match,std::regex(R"(<LanguageStandard>stdcpp([0-9]+)</LanguageStandard>)",std::regex::icase)))
            contract.languageStandard=contractValue("C++"+match[1].str(),ContractEvidence::Discovered,origin);
    }
    for(const auto& preset:presets){const auto text=readFile(preset);const auto origin=preset.lexically_relative(contract.projectRoot).generic_string();if(const auto generator=jsonString(text,"generator"))contract.generator=contractValue(*generator,ContractEvidence::Discovered,origin);if(const auto architecture=jsonString(text,"architecture"))contract.generatorPlatform=contractValue(*architecture,ContractEvidence::Discovered,origin);if(const auto binary=jsonString(text,"binaryDir"))contract.generatedFiles.push_back(*binary);}
    if(!caches.empty()){
        std::sort(caches.begin(),caches.end(),[](const auto& a,const auto& b){return a.native().size()<b.native().size();});const auto& cache=caches.front();const auto text=readFile(cache);const auto origin=cache.lexically_relative(contract.projectRoot).generic_string();
        setFromCache(text,"CMAKE_GENERATOR",contract.generator,origin);setFromCache(text,"CMAKE_GENERATOR_PLATFORM",contract.generatorPlatform,origin);setFromCache(text,"CMAKE_CXX_COMPILER",contract.cppCompiler,origin);setFromCache(text,"CMAKE_C_COMPILER",contract.cCompiler,origin);setFromCache(text,"CMAKE_RC_COMPILER",contract.rcCompiler,origin);setFromCache(text,"CMAKE_TOOLCHAIN_FILE",contract.toolchainFile,origin);setFromCache(text,"CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION",contract.windowsSdk,origin);setFromCache(text,"CMAKE_CXX_COMPILER_VERSION",contract.compilerVersion,origin);
    }
    if(!compilerMetadata.empty()){
        std::sort(compilerMetadata.begin(),compilerMetadata.end(),[](const auto& a,const auto& b){return a.native().size()<b.native().size();});const auto& metadata=compilerMetadata.front();const auto text=readFile(metadata);const auto origin=metadata.lexically_relative(contract.projectRoot).generic_string();
        if(const auto found=cmakeSetValue(text,"CMAKE_CXX_COMPILER"))contract.cppCompiler=contractValue(*found,ContractEvidence::Discovered,origin);
        if(const auto found=cmakeSetValue(text,"CMAKE_CXX_COMPILER_VERSION"))contract.compilerVersion=contractValue(*found,ContractEvidence::Discovered,origin);
        if(const auto found=cmakeSetValue(text,"CMAKE_CXX_COMPILER_ID"))contract.compilerFamily=contractValue(*found,ContractEvidence::Discovered,origin);
        if(const auto found=cmakeSetValue(text,"CMAKE_CXX_COMPILER_ARCHITECTURE_ID"))contract.targetArchitecture=contractValue(*found,ContractEvidence::Discovered,origin);
    }
    if(contract.windowsSdk.value.empty()&&!contract.rcCompiler.value.empty()){
        std::smatch match;const auto normalized=contract.rcCompiler.value;const std::regex sdk(R"(Windows Kits[/\\]10[/\\]bin[/\\]([0-9\.]+)[/\\])",std::regex::icase);
        if(std::regex_search(normalized,match,sdk))contract.windowsSdk=contractValue(match[1].str(),ContractEvidence::Inferred,contract.rcCompiler.origin);
    }
    if(contract.generator.value.find("Visual Studio")!=std::string::npos||msbuild){contract.compilerFamily=contractValue("MSVC",ContractEvidence::Inferred,"Visual Studio generator/project metadata");contract.targetArchitecture=contractValue(contract.generatorPlatform.value.empty()?"x64":contract.generatorPlatform.value,ContractEvidence::Inferred,"generator platform");}
    else if(!contract.cppCompiler.value.empty())contract.compilerFamily=contractValue(lower(contract.cppCompiler.value).find("clang")!=std::string::npos?"Clang":"GNU",ContractEvidence::Inferred,"compiler path");
    if(contract.buildSystem==BuildSystemKind::DirectCompiler&&!directSources.empty())contract.targets.push_back({"direct-project","executable",directSources,{},{},ContractEvidence::Inferred});
    contract.configurations={"Debug","Release"};if(cmake)contract.customCommands.push_back("cmake configure/build");
    for(auto& target:contract.targets){makeUnique(target.sources);makeUnique(target.dependencies);makeUnique(target.linkLibraries);if(target.kind=="executable")contract.executables.push_back(target.name);else if(target.kind=="shared_library"||target.kind=="module_library")contract.dynamicLibraries.push_back(target.name);else if(target.kind=="library")contract.staticLibraries.push_back(target.name);}
    makeUnique(contract.discoveryEvidence);makeUnique(contract.resources);makeUnique(contract.generatedFiles);makeUnique(contract.testTargets);makeUnique(contract.linkLibraries);makeUnique(contract.includeDirectories);makeUnique(contract.definitions);makeUnique(contract.compileFeatures);makeUnique(contract.compileOptions);makeUnique(contract.linkOptions);makeUnique(contract.executables);makeUnique(contract.staticLibraries);makeUnique(contract.customCommands);
    contract.verified=false;return contract;
}

void saveBuildContract(const BuildContract& contract,const std::filesystem::path& path) {
    std::ostringstream body;body<<"GDIBC1\nROOT|"<<hex(contract.projectRoot.generic_string())<<"\nSYSTEM|"<<static_cast<int>(contract.buildSystem)<<"\nFLAGS|"<<(contract.nativeBuildRequired?1:0)<<'|'<<(contract.verified?1:0)<<'\n';
    recordValue(body,"buildSystemVersion",contract.buildSystemVersion);recordValue(body,"generator",contract.generator);recordValue(body,"generatorPlatform",contract.generatorPlatform);recordValue(body,"compilerFamily",contract.compilerFamily);recordValue(body,"compilerVersion",contract.compilerVersion);recordValue(body,"targetArchitecture",contract.targetArchitecture);recordValue(body,"cCompiler",contract.cCompiler);recordValue(body,"cppCompiler",contract.cppCompiler);recordValue(body,"rcCompiler",contract.rcCompiler);recordValue(body,"windowsSdk",contract.windowsSdk);recordValue(body,"toolchainFile",contract.toolchainFile);recordValue(body,"languageStandard",contract.languageStandard);recordValue(body,"workingDirectory",contract.workingDirectory);
    const std::vector<std::pair<std::string,const std::vector<std::string>*>> lists={{"configurations",&contract.configurations},{"includeDirectories",&contract.includeDirectories},{"definitions",&contract.definitions},{"compileFeatures",&contract.compileFeatures},{"compileOptions",&contract.compileOptions},{"linkDirectories",&contract.linkDirectories},{"linkOptions",&contract.linkOptions},{"linkLibraries",&contract.linkLibraries},{"staticLibraries",&contract.staticLibraries},{"dynamicLibraries",&contract.dynamicLibraries},{"executables",&contract.executables},{"resources",&contract.resources},{"generatedFiles",&contract.generatedFiles},{"preBuildCommands",&contract.preBuildCommands},{"postBuildCommands",&contract.postBuildCommands},{"customCommands",&contract.customCommands},{"testTargets",&contract.testTargets},{"runtimeDependencies",&contract.runtimeDependencies},{"discoveryEvidence",&contract.discoveryEvidence},{"conflicts",&contract.conflicts}};
    for(const auto& [name,values]:lists)recordVector(body,name,*values);
    for(const auto& [name,value]:contract.environment)body<<"ENV|"<<hex(name)<<'|'<<hex(value)<<'\n';
    for(const auto& target:contract.targets)body<<"TARGET|"<<hex(target.name)<<'|'<<hex(target.kind)<<'|'<<static_cast<int>(target.evidence)<<'|'<<hex(std::accumulate(target.sources.begin(),target.sources.end(),std::string{},[](std::string a,const std::string& b){return a+(a.empty()?"":"\t")+b;}))<<'|'<<hex(std::accumulate(target.dependencies.begin(),target.dependencies.end(),std::string{},[](std::string a,const std::string& b){return a+(a.empty()?"":"\t")+b;}))<<'|'<<hex(std::accumulate(target.linkLibraries.begin(),target.linkLibraries.end(),std::string{},[](std::string a,const std::string& b){return a+(a.empty()?"":"\t")+b;}))<<'\n';
    for(const auto& edge:contract.graph)body<<"EDGE|"<<hex(edge.consumer)<<'|'<<hex(edge.provider)<<'|'<<hex(edge.relation)<<'|'<<hex(edge.origin)<<'\n';
    const auto payload=body.str();if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<payload<<"HASH|"<<stableHash(payload)<<'\n';if(!output)throw std::runtime_error("Cannot persist build contract");
}

BuildContract loadBuildContract(const std::filesystem::path& path) {
    const auto content=readFile(path);const auto marker=content.rfind("HASH|");if(marker==std::string::npos)throw std::runtime_error("GDIBC1 integrity record missing");const auto payload=content.substr(0,marker);auto stored=content.substr(marker+5);stored.erase(std::remove(stored.begin(),stored.end(),'\r'),stored.end());stored.erase(std::remove(stored.begin(),stored.end(),'\n'),stored.end());if(parseUnsigned(stored)!=stableHash(payload))throw std::runtime_error("GDIBC1 integrity mismatch");
    std::istringstream input(payload);std::string line;if(!std::getline(input,line)||line!="GDIBC1")throw std::runtime_error("Unsupported build contract version");BuildContract contract;
    while(std::getline(input,line)){if(line.empty())continue;const auto fields=split(line,'|');if(fields.empty())continue;
        if(fields[0]=="ROOT"&&fields.size()==2)contract.projectRoot=unhex(fields[1]);
        else if(fields[0]=="SYSTEM"&&fields.size()==2){const auto raw=parseUnsigned(fields[1]);if(raw>static_cast<unsigned>(BuildSystemKind::Unknown))throw std::runtime_error("Invalid GDIBC1 build system");contract.buildSystem=static_cast<BuildSystemKind>(raw);}
        else if(fields[0]=="FLAGS"&&fields.size()==3){contract.nativeBuildRequired=parseUnsigned(fields[1])!=0;contract.verified=parseUnsigned(fields[2])!=0;}
        else if(fields[0]=="VALUE"&&fields.size()==5){auto* destination=valueByName(contract,fields[1]);const auto raw=parseUnsigned(fields[2]);if(!destination||raw>static_cast<unsigned>(ContractEvidence::Conflicting))throw std::runtime_error("Invalid GDIBC1 value");*destination=contractValue(unhex(fields[3]),static_cast<ContractEvidence>(raw),unhex(fields[4]));}
        else if(fields[0]=="LIST"&&fields.size()==3){auto* destination=vectorByName(contract,fields[1]);if(!destination)throw std::runtime_error("Unknown GDIBC1 list");destination->push_back(unhex(fields[2]));}
        else if(fields[0]=="ENV"&&fields.size()==3)contract.environment.emplace(unhex(fields[1]),unhex(fields[2]));
        else if(fields[0]=="TARGET"&&fields.size()==7){const auto raw=parseUnsigned(fields[3]);if(raw>static_cast<unsigned>(ContractEvidence::Conflicting))throw std::runtime_error("Invalid GDIBC1 target evidence");contract.targets.push_back({unhex(fields[1]),unhex(fields[2]),split(unhex(fields[4]),'\t'),split(unhex(fields[5]),'\t'),split(unhex(fields[6]),'\t'),static_cast<ContractEvidence>(raw)});for(auto* vector:{&contract.targets.back().sources,&contract.targets.back().dependencies,&contract.targets.back().linkLibraries})if(vector->size()==1&&vector->front().empty())vector->clear();}
        else if(fields[0]=="EDGE"&&fields.size()==5)contract.graph.push_back({unhex(fields[1]),unhex(fields[2]),unhex(fields[3]),unhex(fields[4])});
        else throw std::runtime_error("Corrupt or unknown GDIBC1 record");
    }
    if(contract.projectRoot.empty()||contract.buildSystem==BuildSystemKind::Unknown)throw std::runtime_error("Incomplete GDIBC1 contract");
    return contract;
}

std::string renderBuildContractJson(const BuildContract& contract) {
    std::ostringstream output;output<<"{\n  \"format\": \"GDIBC1\",\n  \"project_root\": \""<<jsonEscape(contract.projectRoot.generic_string())<<"\",\n  \"build_system\": \""<<toString(contract.buildSystem)<<"\",\n  \"verified\": "<<(contract.verified?"true":"false")<<",\n  \"generator\": {\"value\": \""<<jsonEscape(contract.generator.value)<<"\", \"status\": \""<<toString(contract.generator.evidence)<<"\", \"origin\": \""<<jsonEscape(contract.generator.origin)<<"\"},\n  \"compiler\": {\"family\": \""<<jsonEscape(contract.compilerFamily.value)<<"\", \"version\": \""<<jsonEscape(contract.compilerVersion.value)<<"\", \"path\": \""<<jsonEscape(contract.cppCompiler.value)<<"\"},\n  \"windows_sdk\": \""<<jsonEscape(contract.windowsSdk.value)<<"\",\n  \"targets\": [";
    for(std::size_t i=0;i<contract.targets.size();++i)output<<(i?",":"")<<"\n    {\"name\": \""<<jsonEscape(contract.targets[i].name)<<"\", \"kind\": \""<<jsonEscape(contract.targets[i].kind)<<"\", \"sources\": "<<contract.targets[i].sources.size()<<", \"dependencies\": "<<contract.targets[i].dependencies.size()<<"}";
    output<<"\n  ],\n  \"tests\": "<<contract.testTargets.size()<<",\n  \"resources\": "<<contract.resources.size()<<",\n  \"conflicts\": "<<contract.conflicts.size()<<"\n}\n";return output.str();
}

std::vector<BuildDiagnostic> classifyBuildDiagnostics(const BuildContract& contract,const std::string& output,bool reconstructionFailed) {
    std::vector<BuildDiagnostic> result;std::istringstream input(output);std::string line;
    while(std::getline(input,line)){const auto text=lower(line);const bool errorToken=text.find(": error")!=std::string::npos||text.starts_with("error:")||text.find(" error ")!=std::string::npos;BuildDiagnosticClass kind=BuildDiagnosticClass::Unknown;
        if(text.find("cmake error")!=std::string::npos)kind=BuildDiagnosticClass::CMakeConfiguration;
        else if((text.find("windows sdk")!=std::string::npos&&(text.find("missing")!=std::string::npos||text.find("not found")!=std::string::npos||text.find("cannot")!=std::string::npos))||text.find("cannot find rc")!=std::string::npos)kind=BuildDiagnosticClass::MissingSdk;
        else if(text.find("is not recognized as an internal")!=std::string::npos||text.find("createprocessw failed")!=std::string::npos)kind=BuildDiagnosticClass::WrongCompilerFamily;
        else if(text.find("no such file or directory")!=std::string::npos||text.find("cannot open include")!=std::string::npos)kind=BuildDiagnosticClass::MissingConfiguredInclude;
        else if(text.find("unknown argument")!=std::string::npos||text.find("unrecognized command-line option")!=std::string::npos)kind=BuildDiagnosticClass::IncompatibleCompilerOption;
        else if(text.find("unresolved external")!=std::string::npos||text.find("undefined reference")!=std::string::npos||text.find("lnk2019")!=std::string::npos)kind=BuildDiagnosticClass::MissingLinkLibrary;
        else if(text.find("generated")!=std::string::npos&&text.find("not found")!=std::string::npos)kind=BuildDiagnosticClass::MissingGeneratedSource;
        else if(text.find("rc.exe")!=std::string::npos&&errorToken)kind=BuildDiagnosticClass::ResourceCompiler;
        else if((text.find("abi")!=std::string::npos||text.find("calling convention")!=std::string::npos)&&(text.find("incompatib")!=std::string::npos||text.find("mismatch")!=std::string::npos))kind=BuildDiagnosticClass::AbiIncompatibility;
        else if(text.find("the following tests failed")!=std::string::npos)kind=BuildDiagnosticClass::TestFailure;
        else if(errorToken)kind=BuildDiagnosticClass::SourceCode;else continue;
        result.push_back({kind,line,line,"",toString(contract.buildSystem),reconstructionFailed});if(result.size()>=256)break;
    }
    if(reconstructionFailed&&result.empty())result.push_back({BuildDiagnosticClass::ReconstructionFailure,"GDI could not execute the discovered native build contract","executor failure","",toString(contract.buildSystem),true});
    return result;
}

NativeBuildResult executeNativeBuild(const BuildContract& original,const NativeBuildConfig& config) {
    if(original.projectRoot.empty())throw std::invalid_argument("Native build requires a discovered build contract");
    if(config.timeoutMilliseconds==0||config.testTimeoutMilliseconds==0)throw std::invalid_argument("Native build time budgets must be positive");
    validateIsolatedWorkspace(config.originalProject,config.isolatedWorkspace);
    if(std::filesystem::exists(config.isolatedWorkspace)&&!std::filesystem::is_empty(config.isolatedWorkspace))throw std::invalid_argument("Native build workspace must be new or empty");
    const char* inheritedPath=std::getenv("PATH");const std::string savedPath=inheritedPath==nullptr?std::string{}:inheritedPath;
    const bool injectCompilerRuntime=!config.cppCompilerOverride.empty();
    if(injectCompilerRuntime){const auto runtimeDirectory=std::filesystem::absolute(config.cppCompilerOverride).parent_path().string();const auto runtimePath=runtimeDirectory+";"+savedPath;_putenv_s("PATH",runtimePath.c_str());}
    NativeBuildResult result;result.originalManifestBefore=createSha256DirectoryManifest(config.originalProject).sha256;std::filesystem::create_directories(config.isolatedWorkspace);result.isolatedProject=config.isolatedWorkspace/"project";copyProject(config.originalProject,result.isolatedProject);result.buildDirectory=config.isolatedWorkspace/"build";std::filesystem::create_directories(result.buildDirectory);const auto logs=config.isolatedWorkspace/"logs";std::filesystem::create_directories(logs);result.executedContract=original;result.executedContract.projectRoot=result.isolatedProject;
    if(!original.conflicts.empty()){result.baselineStatus=BuildBaselineStatus::BaselineInconclusive;result.diagnostics.push_back({BuildDiagnosticClass::ReconstructionFailure,"Build contract contains unresolved safety conflicts",original.conflicts.front(),"",toString(original.buildSystem),true});}
    else if(original.buildSystem==BuildSystemKind::CMake||original.buildSystem==BuildSystemKind::Mixed){std::vector<std::string> configure={config.cmakeExecutable.string(),"-S",result.isolatedProject.string(),"-B",result.buildDirectory.string()};const auto generator=!config.generatorOverride.empty()?config.generatorOverride:original.generator.value;const auto platform=!config.generatorPlatformOverride.empty()?config.generatorPlatformOverride:original.generatorPlatform.value;if(!generator.empty()){configure.push_back("-G");configure.push_back(generator);}if(!platform.empty()){configure.push_back("-A");configure.push_back(platform);}if(generator.find("Makefiles")!=std::string::npos&&!config.makeExecutable.empty())configure.push_back("-DCMAKE_MAKE_PROGRAM="+std::filesystem::absolute(config.makeExecutable).string());if(generator.find("Ninja")!=std::string::npos&&!config.ninjaExecutable.empty())configure.push_back("-DCMAKE_MAKE_PROGRAM="+std::filesystem::absolute(config.ninjaExecutable).string());if(!config.cppCompilerOverride.empty()){configure.push_back("-DCMAKE_CXX_COMPILER="+std::filesystem::absolute(config.cppCompilerOverride).string());}if(!original.toolchainFile.value.empty()&&std::filesystem::exists(original.toolchainFile.value)){configure.push_back("-DCMAKE_TOOLCHAIN_FILE="+original.toolchainFile.value);}auto step=executeStep("configure",configure,result.isolatedProject,logs,config.timeoutMilliseconds);result.configured=step.process.launched&&!step.process.timedOut&&step.process.exitCode==0;result.steps.push_back(std::move(step));if(result.configured){const auto configured=discoverBuildContract(result.buildDirectory);result.executedContract.generator=configured.generator;result.executedContract.generatorPlatform=configured.generatorPlatform;result.executedContract.compilerFamily=configured.compilerFamily;result.executedContract.compilerVersion=configured.compilerVersion;result.executedContract.cppCompiler=configured.cppCompiler;result.executedContract.rcCompiler=configured.rcCompiler;result.executedContract.windowsSdk=configured.windowsSdk;result.executedContract.targetArchitecture=configured.targetArchitecture;for(auto* field:{&result.executedContract.generator,&result.executedContract.generatorPlatform,&result.executedContract.compilerFamily,&result.executedContract.compilerVersion,&result.executedContract.cppCompiler,&result.executedContract.rcCompiler,&result.executedContract.windowsSdk,&result.executedContract.targetArchitecture})if(!field->value.empty())field->evidence=ContractEvidence::Verified;result.executedContract.verified=true;auto build=executeStep("build",{config.cmakeExecutable.string(),"--build",result.buildDirectory.string(),"--config",config.configuration},result.isolatedProject,logs,config.timeoutMilliseconds);result.built=build.process.launched&&!build.process.timedOut&&build.process.exitCode==0;result.steps.push_back(std::move(build));}if(result.built&&config.runTests){auto test=executeStep("test",{config.ctestExecutable.string(),"--test-dir",result.buildDirectory.string(),"-C",config.configuration,"--output-on-failure"},result.isolatedProject,logs,config.timeoutMilliseconds);result.testsPassed=test.process.launched&&!test.process.timedOut&&test.process.exitCode==0;result.steps.push_back(std::move(test));}else result.testsPassed=result.built&&!config.runTests;}
    else if(original.buildSystem==BuildSystemKind::MSBuild){std::filesystem::path buildInput;for(const auto& entry:std::filesystem::directory_iterator(result.isolatedProject))if(lower(entry.path().extension().string())==".sln"){buildInput=entry.path();break;}if(buildInput.empty())for(const auto& entry:std::filesystem::directory_iterator(result.isolatedProject))if(lower(entry.path().extension().string())==".vcxproj"){buildInput=entry.path();break;}if(!buildInput.empty()&&!config.msbuildExecutable.empty()){result.configured=true;auto build=executeStep("build",{config.msbuildExecutable.string(),buildInput.string(),"/m","/p:Configuration="+config.configuration,"/p:Platform=x64"},result.isolatedProject,logs,config.timeoutMilliseconds);result.built=build.process.launched&&!build.process.timedOut&&build.process.exitCode==0;result.testsPassed=result.built&&(original.testTargets.empty()||!config.runTests);result.steps.push_back(std::move(build));}}
    else if(original.buildSystem==BuildSystemKind::Ninja||original.buildSystem==BuildSystemKind::Make){const auto tool=original.buildSystem==BuildSystemKind::Ninja?config.ninjaExecutable:config.makeExecutable;result.configured=true;auto build=executeStep("build",{tool.string()},result.isolatedProject,logs,config.timeoutMilliseconds);result.built=build.process.launched&&!build.process.timedOut&&build.process.exitCode==0;result.testsPassed=result.built&&(original.testTargets.empty()||!config.runTests);result.steps.push_back(std::move(build));}
    else if(original.buildSystem==BuildSystemKind::DirectCompiler&&!original.targets.empty()){std::vector<std::string> command={original.cppCompiler.value.empty()?"g++":original.cppCompiler.value,"-std=c++20","-o",(result.buildDirectory/"direct-project.exe").string()};for(const auto& source:original.targets.front().sources)command.push_back((result.isolatedProject/source).string());result.configured=true;auto build=executeStep("build",command,result.isolatedProject,logs,config.timeoutMilliseconds);result.built=build.process.launched&&!build.process.timedOut&&build.process.exitCode==0;result.testsPassed=result.built&&(original.testTargets.empty()||!config.runTests);result.steps.push_back(std::move(build));}
    const bool noExecutor=result.steps.empty();
    auto classified=classifyBuildDiagnostics(original,combinedLogs(result),noExecutor);
    result.diagnostics.insert(result.diagnostics.end(),classified.begin(),classified.end());
    if(result.built&&result.testsPassed)result.baselineStatus=BuildBaselineStatus::BaselineReproduced;else if(noExecutor)result.baselineStatus=BuildBaselineStatus::BaselineBlockedToolchain;else if(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[](const auto& diagnostic){return diagnostic.classification==BuildDiagnosticClass::MissingSdk||diagnostic.classification==BuildDiagnosticClass::WrongCompilerFamily;}))result.baselineStatus=BuildBaselineStatus::BaselineBlockedEnvironment;else if(!result.diagnostics.empty()&&!result.diagnostics.front().causedByReconstruction)result.baselineStatus=BuildBaselineStatus::BaselineFailedProjectDefect;else result.baselineStatus=BuildBaselineStatus::BaselineInconclusive;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(result.buildDirectory,std::filesystem::directory_options::skip_permission_denied))if(entry.is_regular_file()){const auto extension=lower(entry.path().extension().string());if(extension==".exe"||extension==".dll"||extension==".so"||extension==".a"||extension==".lib")result.artifacts.push_back(entry.path());}
    result.originalManifestAfter=createSha256DirectoryManifest(config.originalProject).sha256;result.originalUntouched=result.originalManifestBefore==result.originalManifestAfter;result.reportPath=config.isolatedWorkspace/"native_build_result.json";std::ofstream report(result.reportPath);report<<"{\n  \"format\": \"GDI-native-build-v2\",\n  \"build_system\": \""<<toString(original.buildSystem)<<"\",\n  \"generator\": \""<<jsonEscape(result.executedContract.generator.value)<<"\",\n  \"compiler_family\": \""<<jsonEscape(result.executedContract.compilerFamily.value)<<"\",\n  \"compiler_version\": \""<<jsonEscape(result.executedContract.compilerVersion.value)<<"\",\n  \"windows_sdk\": \""<<jsonEscape(result.executedContract.windowsSdk.value)<<"\",\n  \"baseline\": \""<<toString(result.baselineStatus)<<"\",\n  \"configured\": "<<(result.configured?"true":"false")<<",\n  \"built\": "<<(result.built?"true":"false")<<",\n  \"tests_passed\": "<<(result.testsPassed?"true":"false")<<",\n  \"diagnostics\": "<<result.diagnostics.size()<<",\n  \"artifacts\": "<<result.artifacts.size()<<",\n  \"original_manifest_before\": \""<<result.originalManifestBefore<<"\",\n  \"original_manifest_after\": \""<<result.originalManifestAfter<<"\",\n  \"original_untouched\": "<<(result.originalUntouched?"true":"false")<<"\n}\n";return result;
}
} // namespace genomic::gpi
