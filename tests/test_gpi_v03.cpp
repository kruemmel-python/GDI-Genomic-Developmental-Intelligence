#include "gpi/natural_task.hpp"
#include "gpi/line_endings.hpp"
#include "gpi/process_runner.hpp"
#include "gpi/filesystem_safety.hpp"
#include "gpi/autonomous_project_developer.hpp"
#include "gpi/project_emitter.hpp"
#include "gpi/research_developer.hpp"
#include "gpi/task_ir.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
void require(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main(int argc,char** argv) {
    if (argc>1 && std::string(argv[1])=="--sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(3)); return 0;
    }
    if (argc>1 && std::string(argv[1])=="--echo") {
        std::cout<<"ECHO_OK\n"; return 0;
    }
    try {
        using genomic::gpi::normalizeLineEndings;
        require(normalizeLineEndings("VALUE=11.000\r\nVALUE=21.000\r\nREAD=4\r\nSELECTED=2\r\nSUM=32.000\r\n") ==
                normalizeLineEndings("VALUE=11.000\nVALUE=21.000\nREAD=4\nSELECTED=2\nSUM=32.000\n"),
                "Windows CRLF output must match LF oracle output");
        require(normalizeLineEndings("SUM=31.000\r\n") != normalizeLineEndings("SUM=32.000\n"),
                "Line ending normalization must not conceal behavior differences");
        require(normalizeLineEndings("A\rB\n") == "AB\n", "CR normalization must be deterministic");
        const auto research=genomic::gpi::decomposeResearchTask(
            "Ordne beliebig viele Zahlen und entferne Duplikate",false);
        require(research.supported && research.problem==genomic::gpi::ResearchProblem::Composite,
                "Research decomposition failed");
        require(research.components.size()>=3 && research.strategies.size()==2,
                "Research plan must select components and competing strategies");
        const auto composite=genomic::gpi::decomposeResearchTask(
            "Lies Zahlen, behalte Werte groesser als 2, entferne Duplikate, sortiere sie und berechne den Median",false);
        require(composite.supported && composite.problem==genomic::gpi::ResearchProblem::Composite,
                "Generative composite analysis failed");
        require(composite.operations.size()==4 && composite.graph.size()==6,
                "Composite analysis must produce four typed operations plus input/output");
        require(composite.operations.front().operation=="filter_gt" &&
                composite.operations.front().integerParameter==2 &&
                composite.operations.back().operation=="median",
                "Composite conditions or dependencies were not grounded");
        require(composite.contract.accepted && composite.contract.semanticHash!=0 &&
                composite.contract.originalRequest.find("behalte Werte")!=std::string::npos &&
                composite.contract.requiredOperations.size()==4,
                "Explicit goal contract must preserve request, normalization and required operations");
        const auto contradictoryThreshold=genomic::gpi::decomposeResearchTask(
            "Behalte Werte groesser als 2 und groesser als 5, sortiere sie",false);
        require(!contradictoryThreshold.supported && !contradictoryThreshold.contract.contradictions.empty(),
                "Conflicting thresholds must not be silently reinterpreted");
        const auto unsupportedDirection=genomic::gpi::decomposeResearchTask(
            "Sortiere Zahlen absteigend und entferne Duplikate",false);
        require(!unsupportedDirection.supported && !unsupportedDirection.contract.unresolvedRequirements.empty(),
                "Unknown goal semantics must be reported explicitly");
        const auto unsupportedSideEffect=genomic::gpi::decomposeResearchTask(
            "Sortiere Zahlen und speichere das Ergebnis als CSV-Datei",false);
        require(!unsupportedSideEffect.supported && !unsupportedSideEffect.contract.unresolvedRequirements.empty(),
                "Unimplemented side effects must not disappear from the goal contract");
        const auto conflicting=genomic::gpi::decomposeResearchTask(
            "Sortiere Zahlen und berechne Median und Summe",false);
        require(!conflicting.supported && !conflicting.diagnostics.empty(),
                "Conflicting composite terminal operations must fail closed");
        const auto arbitraryBlocked=genomic::gpi::decomposeResearchTask(
            "experiment\n```cpp\nint main(){return 0;}\n```",false);
        require(!arbitraryBlocked.supported && arbitraryBlocked.requiresUnsafeMode,
                "Arbitrary prompt code must require explicit unsafe mode");
        const auto arbitraryAllowed=genomic::gpi::decomposeResearchTask(
            "experiment\n```cpp\nint main(){return 0;}\n```",true);
        require(arbitraryAllowed.supported && arbitraryAllowed.problem==genomic::gpi::ResearchProblem::ArbitraryCpp,
                "Unsafe research planning failed");
        const auto de=genomic::gpi::translateNaturalTask("Berechne die Summe der Quadrate der ersten 4 Zahlen");
        require(de.error.empty(),"German natural task translation failed");
        require(de.structured=="map=x*x; reduce=sum; samples=4; emit=false","German task mismatch");
        const auto en=genomic::gpi::translateNaturalTask("compute the average of the first 5 squares");
        require(en.error.empty() && en.structured.find("reduce=mean")!=std::string::npos,"English task mismatch");
        const auto formula=genomic::gpi::translateNaturalTask(
            "Berechne die Summe der ersten 4 Zahlen mit Formel x*x+3*x mit Filter y>10");
        require(formula.error.empty() && formula.structured.find("map=x*x+3*x")!=std::string::npos &&
                formula.structured.find("filter=y>10")!=std::string::npos,
                "Novel German formula was not translated");
        const auto formulaParsed=genomic::gpi::parseTask(formula.structured);
        require(formulaParsed.task.has_value(),"Novel formula must type-check");
        require(genomic::gpi::expectedSmokeOutput(*formulaParsed.task)==
                "READ=4\nSELECTED=2\nSUM=46.000\n","Novel formula oracle mismatch");
        require(!genomic::gpi::translateNaturalTask("Create a website with login and database").error.empty(),
                "Unsupported natural task must fail closed");
        require(!genomic::gpi::translateNaturalTask("Berechne die Summe der Quadrate der ersten 100 Zahlen").error.empty(),
                "Oversized natural task must fail closed");
        require(!genomic::gpi::translateNaturalTask("Berechne die Summe der Quadrate der ersten 4 Zahlen; system(x)").error.empty(),
                "Appended arbitrary instructions must fail closed");
        const auto parsed=genomic::gpi::parseTask(de.structured);
        require(parsed.task.has_value(),"Normalized task must parse");
        require(genomic::gpi::expectedOutputForInputs(*parsed.task,{1,2,3,4})==
                "READ=4\nSELECTED=4\nSUM=30.000\n","Independent oracle mismatch");
        const auto temp=std::filesystem::current_path()/"gpi_v03_unit_test_artifacts";
        std::filesystem::remove_all(temp);
        std::filesystem::create_directories(temp);
        const auto protectedRoot=temp/"protected";
        std::filesystem::create_directories(protectedRoot);
        {
            std::ofstream sample(protectedRoot/"sample.txt",std::ios::binary);
            sample<<"abc";
        }
        const auto manifest=genomic::gpi::createSha256DirectoryManifest(protectedRoot);
        require(manifest.files.size()==1 && manifest.totalBytes==3 &&
                manifest.files.front().sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "SHA-256 project manifest is incorrect");
        bool nestedWorkspaceBlocked=false;
        try { genomic::gpi::validateIsolatedWorkspace(protectedRoot,protectedRoot/"work"); }
        catch(const std::invalid_argument&) { nestedWorkspaceBlocked=true; }
        require(nestedWorkspaceBlocked,"Workspace inside protected project must fail closed");
        genomic::gpi::validateIsolatedWorkspace(protectedRoot,temp/"isolated");
        std::error_code symlinkError;
        std::filesystem::create_symlink(protectedRoot/"sample.txt",protectedRoot/"escape-link",symlinkError);
        if(!symlinkError) {
            bool symlinkBlocked=false;
            try { static_cast<void>(genomic::gpi::createSha256DirectoryManifest(protectedRoot)); }
            catch(const std::invalid_argument&) { symlinkBlocked=true; }
            require(symlinkBlocked,"Symbolic links in foreign projects must fail closed");
            std::filesystem::remove(protectedRoot/"escape-link",symlinkError);
        }
        const auto diagnostics=genomic::gpi::parseCompilerDiagnostics(
            "C:/work/main.cpp:12:7: error: cannot convert 'int' to 'Widget'\n"
            "ld.exe: object.o: undefined reference to `run()'\n");
        require(diagnostics.size()==2 && diagnostics[0].line==12 && diagnostics[0].column==7 &&
                diagnostics[0].classification==genomic::gpi::CompilerDiagnosticClass::TypeMismatch &&
                diagnostics[1].classification==genomic::gpi::CompilerDiagnosticClass::UndefinedReference,
                "Structured compiler diagnostic parsing failed");
        const auto project=genomic::gpi::emitProject(*parsed.task,42,temp/"generated");
        require(project.files.size()==6,"Expected six project files");
        require(std::filesystem::exists(project.pipelineSource),"Generated source missing");
        bool overwriteBlocked=false;
        try { (void)genomic::gpi::emitProject(*parsed.task,42,temp/"generated"); }
        catch (const std::invalid_argument&) { overwriteBlocked=true; }
        require(overwriteBlocked,"Existing generated project must not be overwritten");
        const auto exe=std::filesystem::absolute(argv[0]).string();
        const auto echo=genomic::gpi::runProcess({exe,"--echo"},temp,{},temp/"out",temp/"err",
                                                  std::chrono::seconds(2));
        require(echo.launched && echo.exitCode==0 && !echo.timedOut,"Direct process launch failed");
        const auto sleeper=genomic::gpi::runProcess({exe,"--sleep"},temp,{},temp/"out",temp/"err",
                                                     std::chrono::milliseconds(100));
        require(sleeper.launched && sleeper.timedOut,"Process timeout not enforced");
        std::filesystem::remove_all(temp);
        std::cout<<"GPI v0.3 unit checks: PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"GPI v0.3 test FAILED: "<<e.what()<<'\n'; return 1;
    }
}
