#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <cmath>

#include "runtime/substrate_kernel.hpp"
#include "genome/genome_base.hpp"
#include "genome/genome_strand.hpp"
#include "genome/genome_decoder.hpp"
#include "genome/genome_integrity.hpp"
#include "genome/genome_bootstrap.hpp"
#include "cpp20/cpp20_lexicon.hpp"
#include "cpp20/cpp20_preprocessor.hpp"
#include "cpp20/cpp20_types.hpp"
#include "cpp20/cpp20_ast.hpp"
#include "cpp20/cpp20_semantics.hpp"
#include "cpp20/cpp20_compiler.hpp"
#include "runtime/genome_vm.hpp"
#include "regulation/molecule.hpp"
#include "regulation/molecular_pool.hpp"
#include "regulation/regulatory_resolver.hpp"
#include "expression/splice_graph.hpp"
#include "expression/program_synthesizer.hpp"
#include "expression/semantic_validator.hpp"
#include "development/phenotype_builder.hpp"
#include "research/causal_trace.hpp"
#include "genome/genomic_rule_engine.hpp"
#include "cpp20/cpp20_parser.hpp"
#include "cpp20/cpp20_classes.hpp"
#include "cpp20/cpp20_templates.hpp"
#include "cpp20/cpp20_constexpr.hpp"
#include "cpp20/cpp20_runtime_features.hpp"
#include "cpp20/cpp20_memory_model.hpp"
#include "cpp20/cpp20_source_genome.hpp"
#include "evolution/genome_evolution.hpp"
#include "runtime/genome_persistence.hpp"
#include "runtime/genome_stdlib.hpp"
#include "gpi/prompt_regulatory_resolver.hpp"
#include "gpi/prompt_program_synthesizer.hpp"
#include "gpi/task_ir.hpp"
#include "gpi/task_genome_policy.hpp"
#include "gpi/experience_store.hpp"
#include "gpi/integrated_development.hpp"
#include "gpi/generative_project.hpp"

using namespace genomic;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_testsFailed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        int before = g_testsFailed; \
        std::cout << "[RUNNING] " << #fn << " ... " << std::flush; \
        fn(); \
        if (g_testsFailed == before) { \
            std::cout << "PASSED\n" << std::flush; \
            g_testsPassed++; \
        } \
    } while(0)

// -------------------------------------------------------------
// 1. Genome Tests
// -------------------------------------------------------------
void test_strand_roundtrip_is_exact() {
    std::string original = "template <typename T> requires std::integral<T> constexpr T square(T x) { return x * x; }";
    auto bases = GenomeDecoder::encodeString(original);
    std::string decoded = GenomeDecoder::decodeString(bases);
    TEST_ASSERT(original == decoded, "Decoded string must match original exactly");

    // Codon roundtrip
    for (uint8_t op = 0; op < 64; ++op) {
        Codon c = Codon::fromOpcode(static_cast<GisaOpcode>(op));
        TEST_ASSERT(c.toIndex() == op, "Codon index must match opcode value");
        TEST_ASSERT(c.toOpcode() == static_cast<GisaOpcode>(op), "Opcode must roundtrip");
    }
}

void test_genome_hash_is_stable() {
    auto sys1 = GenomeBootstrap::buildCompleteSystem();
    auto sys2 = GenomeBootstrap::buildCompleteSystem();

    uint64_t hash1 = sys1.strand->getGenomeHash();
    uint64_t hash2 = sys2.strand->getGenomeHash();

    TEST_ASSERT(hash1 != 0, "Genome hash must not be zero");
    TEST_ASSERT(hash1 == hash2, "Identical genome construction must produce identical hash");
}

void test_region_references_are_deterministic() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    auto integrity = sys.strand->verifyIntegrity();
    TEST_ASSERT(integrity.passed, "Genome integrity report must pass with zero errors");
    TEST_ASSERT(integrity.errors.empty(), "No errors in reference graph");
}

void test_experience_never_mutates_genome() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    uint64_t originalHash = sys.strand->getGenomeHash();

    // Verify invariant: Mutating after freeze MUST throw exception
    bool exceptionThrown = false;
    try {
        sys.strand->appendBasesFromString("ACGT");
    } catch (const std::runtime_error&) {
        exceptionThrown = true;
    }
    TEST_ASSERT(exceptionThrown, "Appending bases to frozen strand must throw exception");

    exceptionThrown = false;
    try {
        sys.strand->addRegion(RegionKind::DomainModules, "IllegalRegion", {GenomeBase::A, GenomeBase::C});
    } catch (const std::runtime_error&) {
        exceptionThrown = true;
    }
    TEST_ASSERT(exceptionThrown, "Adding region to frozen strand must throw exception");

    TEST_ASSERT(sys.strand->getGenomeHash() == originalHash, "Genome hash must remain unchanged");
}

// -------------------------------------------------------------
// 2. Molecular & Regulation Tests
// -------------------------------------------------------------
void test_binding_respects_affinity() {
    GenomeBindingSite site;
    site.acceptedSignature = 0x1234;
    site.minimumAffinity = 0.5;

    auto evalBelow = RegulatoryResolver::evaluateSite(site, 0.3);
    TEST_ASSERT(!evalBelow.bound, "Concentration below affinity must not bind");

    auto evalAbove = RegulatoryResolver::evaluateSite(site, 0.7);
    TEST_ASSERT(evalAbove.bound, "Concentration above affinity must bind");
    TEST_ASSERT(evalAbove.effectiveAffinity > 0.0, "Effective affinity must be positive");
}

void test_binding_respects_orientation() {
    GenomeBindingSite site;
    site.acceptedSignature = 0x1234;
    site.minimumAffinity = 0.2;
    site.orientationSensitive = true;
    site.orientationForward = true;

    auto evalWrong = RegulatoryResolver::evaluateSite(site, 0.8, false); // reverse orientation
    TEST_ASSERT(!evalWrong.bound, "Mismatched orientation must fail binding");

    auto evalRight = RegulatoryResolver::evaluateSite(site, 0.8, true); // forward orientation
    TEST_ASSERT(evalRight.bound, "Matched orientation must succeed binding");
}

void test_cooperative_binding_is_deterministic() {
    RegulatoryResolver resolver;
    GenomeEnhancer enh;
    enh.id = 500;
    enh.targetRegion = 999;
    enh.activationThreshold = 0.5;
    enh.cooperativity = 2.5; // High cooperativity
    enh.maxBindingDistance = 20;

    enh.sites.push_back(GenomeBindingSite{0xAA, 0.2, 1.0, 0.0, 5, false, true});
    enh.sites.push_back(GenomeBindingSite{0xAA, 0.2, 1.0, 0.0, 15, false, true});
    resolver.addEnhancer(enh);

    MolecularPool pool;
    pool.setConcentration(0, 0xAA, 0.8);

    auto res = resolver.resolve(pool, 0);
    TEST_ASSERT(res.enhancerDetails.size() == 1, "Enhancer must be evaluated");
    TEST_ASSERT(res.enhancerDetails[0].cooperativityMultiplier > 1.0, "Cooperativity multiplier must apply");
    TEST_ASSERT(res.enhancerDetails[0].activated, "Enhancer must activate under cooperative binding");
}

void test_molecule_decay_is_stable() {
    MolecularPool pool;
    pool.setConcentration(0, 0x99, 1.0, "TestMol", MoleculeClass::Signal, 10.0); // half life 10 ticks

    // After 10 ticks, concentration should be approx 0.5
    pool.updateDecay(10.0);
    double c = pool.getConcentration(0, 0x99);
    TEST_ASSERT(std::abs(c - 0.5) < 0.02, "Concentration after half-life must be approx 0.5");
}

void test_compartment_rules_hold() {
    MolecularPool pool;
    pool.setConcentration(0, 0x55, 0.9);

    TEST_ASSERT(pool.getConcentration(0, 0x55) == 0.9, "Compartment 0 must have molecule");
    TEST_ASSERT(pool.getConcentration(1, 0x55) == 0.0, "Compartment 1 must not have molecule before diffusion");

    pool.updateDiffusion(0, 1, 0.2); // 20% diffusion rate
    TEST_ASSERT(pool.getConcentration(1, 0x55) > 0.0, "Compartment 1 must receive molecule after diffusion");
}

void test_molecular_pool_preserves_full_signature_width() {
    MolecularPool pool;
    constexpr uint64_t low=0x0000000000000111ULL;
    constexpr uint64_t highA=0xA100000000000111ULL;
    constexpr uint64_t highB=0xB200000000000111ULL;
    pool.setConcentration(7,low,0.20,"low");
    pool.setConcentration(7,highA,0.60,"high-a");
    pool.setConcentration(7,highB,0.90,"high-b");
    TEST_ASSERT(pool.getConcentration(7,low)==0.20,"Low signature must remain distinct");
    TEST_ASSERT(pool.getConcentration(7,highA)==0.60,"First full-width signature must remain distinct");
    TEST_ASSERT(pool.getConcentration(7,highB)==0.90,"Second full-width signature must remain distinct");
    TEST_ASSERT(pool.getMolecules(7).size()==3,"Full 64-bit signatures must not alias inside one compartment");
}

void test_universal_cpp20_source_space_is_lossless_and_genomic() {
    const auto system=GenomeBootstrap::buildCompleteSystem();
    const auto sourceGenome=Cpp20SourceGenome::fromGenome(*system.strand);
    TEST_ASSERT(sourceGenome.mutationEnabled("token-integer-delta"),"Numeric token mutation must be genomically authorized");
    TEST_ASSERT(sourceGenome.mutationEnabled("empirical-numeric-calibration"),"Empirical calibration must be genomically authorized");
    TEST_ASSERT(sourceGenome.mutationEnabled("infer-link-provider"),"Link-provider inference must be genomically authorized");
    TEST_ASSERT(sourceGenome.mutationEnabled("coerce-numeric-string-literal"),"Literal type coercion must be genomically authorized");
    TEST_ASSERT(sourceGenome.mutationEnabled("local-integer-neighborhood"),"Local test-guided search must be genomically authorized");
    TEST_ASSERT(!sourceGenome.mutationEnabled("arbitrary-host-edit"),"Unknown host edits must not be genomically authorized");
    TEST_ASSERT(sourceGenome.structureEnabled("typed-functions"),"Typed function synthesis must be genomically authorized");
    TEST_ASSERT(sourceGenome.structureEnabled("header-source-executable-test"),"Multi-file project synthesis must be genomically authorized");
    TEST_ASSERT(!sourceGenome.structureEnabled("arbitrary-host-project"),"Unknown project structures must not be authorized");
    const auto headers=sourceGenome.suggestedHeaders("error: std::cout is unavailable and vector was not declared");
    TEST_ASSERT(std::find(headers.begin(),headers.end(),"iostream")!=headers.end(),"Compiler symbol must activate genomic iostream rule");
    TEST_ASSERT(std::find(headers.begin(),headers.end(),"vector")!=headers.end(),"Compiler symbol must activate genomic vector rule");
    std::string everyOctet;everyOctet.reserve(256);
    for(unsigned value=0;value<256;++value)everyOctet.push_back(static_cast<char>(value));
    const auto artifact=sourceGenome.encode("modules/core.cpp",everyOctet);
    TEST_ASSERT(sourceGenome.decode(artifact)==everyOctet,"Every source octet must roundtrip exactly");
    TEST_ASSERT(artifact.immutableGenomeHash==system.strand->getGenomeHash(),"Source artifact must retain immutable genome identity");
    TEST_ASSERT(artifact.generativeRuleRegion!=0,"Source artifact must identify its genomic generative rule");

    auto corrupted=artifact;corrupted.encodedSource.front()=corrupted.encodedSource.front()==GenomeBase::A?GenomeBase::C:GenomeBase::A;
    TEST_ASSERT(!sourceGenome.verifies(corrupted),"Corrupted generated source must fail closed");
    bool unsafePathRejected=false;
    try { static_cast<void>(sourceGenome.encode("../escape.cpp","int main(){}")); }
    catch(const std::invalid_argument&) { unsafePathRejected=true; }
    TEST_ASSERT(unsafePathRejected,"Generated source must not escape its project workspace");
}

void test_general_generative_project_graph() {
    using namespace genomic::gpi;
    const auto system=GenomeBootstrap::buildCompleteSystem();
    GenerativeProjectGraph graph(Cpp20SourceGenome::fromGenome(*system.strand));
    graph.addNode("api",ProjectNodeKind::Header,"include/math/api.hpp",
        "#pragma once\nnamespace math { template<class T> concept Addable=requires(T a){a+a;}; template<Addable T>T twice(T); }\n");
    graph.addNode("implementation",ProjectNodeKind::Source,"src/api.cpp",
        "#include \"math/api.hpp\"\nnamespace math { template<class T>T twice(T x){return x+x;} template int twice<int>(int); }\n");
    graph.addNode("application",ProjectNodeKind::Source,"src/main.cpp",
        "#include \"math/api.hpp\"\n#include <iostream>\nint main(){std::cout<<math::twice(21)<<'\\n';}\n");
    graph.addNode("test",ProjectNodeKind::Test,"tests/api_test.cpp",
        "#include \"math/api.hpp\"\nint main(){return math::twice(3)==6?0:1;}\n");
    graph.addNode("manifest",ProjectNodeKind::BuildManifest,"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.20)\nproject(generated LANGUAGES CXX)\n");
    graph.addDependency("implementation","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","api",ProjectDependencyKind::Includes);
    graph.addDependency("application","implementation",ProjectDependencyKind::Links);
    graph.addDependency("test","implementation",ProjectDependencyKind::Tests);
    graph.addTarget("math",ProjectTargetKind::Library,{"api","implementation"});
    graph.addTarget("program",ProjectTargetKind::Executable,{"application","implementation"});
    graph.addTarget("unit",ProjectTargetKind::Test,{"test","implementation"});
    TEST_ASSERT(graph.graphHash()!=0,"General project graph must have a stable identity");
    const auto output=std::filesystem::temp_directory_path()/"gpi_general_project_graph_test";
    std::error_code error;std::filesystem::remove_all(output,error);
    graph.materialize(output);
    TEST_ASSERT(std::filesystem::is_regular_file(output/"include/math/api.hpp"),"Header node must materialize");
    TEST_ASSERT(std::filesystem::is_regular_file(output/"src/main.cpp"),"Source node must materialize");
    std::filesystem::remove_all(output,error);

    bool cycleRejected=false;
    try { graph.addDependency("api","application",ProjectDependencyKind::Includes);graph.validate(); }
    catch(const std::invalid_argument&) { cycleRejected=true; }
    TEST_ASSERT(cycleRejected,"Cyclic project architecture must fail closed");

    bool typeRejected=false;
    try {
        GenerativeProjectGraph invalid(Cpp20SourceGenome::fromGenome(*system.strand));
        invalid.addNode("source",ProjectNodeKind::Source,"source.cpp","int value=1;\n");
        invalid.addNode("other",ProjectNodeKind::Source,"other.cpp","int other=2;\n");
        invalid.addDependency("source","other",ProjectDependencyKind::Includes);
        invalid.addTarget("bad",ProjectTargetKind::Executable,{"source"});invalid.validate();
    } catch(const std::invalid_argument&) { typeRejected=true; }
    TEST_ASSERT(typeRejected,"Typed dependency roles must reject including a source file as a header");
}

void test_multi_compartment_signal_boundaries() {
    using namespace genomic::gpi;
    MultiCompartmentSystem communicating(0x1234,true);
    communicating.addCompartment({1,CompartmentRole::Analysis,false,
        {DevelopmentSignalKind::NegativeEvidence},{}});
    communicating.addCompartment({2,CompartmentRole::Research,true,
        {DevelopmentSignalKind::CapabilityProbeRequested,DevelopmentSignalKind::GoalAccepted},{}});
    TEST_ASSERT(communicating.exchange({DevelopmentSignalKind::CapabilityProbeRequested,1,2,0,99,0.8,"probe",false}),
                "Allowed typed signal must be delivered");
    TEST_ASSERT(communicating.exchange({DevelopmentSignalKind::GoalAccepted,1,2,0,99,0.6,"goal",false}),
                "Second allowed typed signal must be delivered");
    TEST_ASSERT(MultiCompartmentSystem::signature(DevelopmentSignalKind::CapabilityProbeRequested)!=
                MultiCompartmentSystem::signature(DevelopmentSignalKind::GoalAccepted),
                "Signal signatures must retain their distinct high bits");
    TEST_ASSERT(communicating.concentration(2,DevelopmentSignalKind::CapabilityProbeRequested)==0.8,
                "First signal concentration must remain independently addressable");
    TEST_ASSERT(communicating.concentration(2,DevelopmentSignalKind::GoalAccepted)==0.6,
                "Second signal concentration must remain independently addressable");
    bool rejected=false;
    try {
        communicating.exchange({DevelopmentSignalKind::ComponentReady,1,2,0,99,1.0,"forbidden",false});
    } catch(const std::invalid_argument&) { rejected=true; }
    TEST_ASSERT(rejected,"Analysis compartment must not emit research-only signals");

    MultiCompartmentSystem isolated(0x1234,false);
    isolated.addCompartment({1,CompartmentRole::Analysis,false,{},{}});
    isolated.addCompartment({2,CompartmentRole::Research,false,
        {DevelopmentSignalKind::CapabilityProbeRequested},{}});
    TEST_ASSERT(!isolated.exchange({DevelopmentSignalKind::CapabilityProbeRequested,1,2,0,99,1.0,"isolated",false}),
                "Isolated compartments must not deliver signals");
    TEST_ASSERT(isolated.concentration(2,DevelopmentSignalKind::CapabilityProbeRequested)==0.0,
                "Undelivered signals must not alter molecular state");
}

// -------------------------------------------------------------
// 3. C++20 Tests
// -------------------------------------------------------------
void test_cpp20_lexer_matches_reference_cases() {
    std::string code = "template<typename T> concept Incrementable = requires(T x) { ++x; }; "
                       "consteval int getConst() { return 42; } "
                       "auto task = []() -> Task { co_await something(); co_return 0; };";
    auto tokens = Cpp20Lexicon::tokenize(code);
    TEST_ASSERT(!tokens.empty(), "Tokens must not be empty");

    bool hasConcept = false;
    bool hasRequires = false;
    bool hasConsteval = false;
    bool hasCoAwait = false;
    bool hasCoReturn = false;

    for (const auto& tok : tokens) {
        if (tok.isKeyword("concept")) hasConcept = true;
        if (tok.isKeyword("requires")) hasRequires = true;
        if (tok.isKeyword("consteval")) hasConsteval = true;
        if (tok.isKeyword("co_await")) hasCoAwait = true;
        if (tok.isKeyword("co_return")) hasCoReturn = true;
    }

    TEST_ASSERT(hasConcept, "Must lex 'concept'");
    TEST_ASSERT(hasRequires, "Must lex 'requires'");
    TEST_ASSERT(hasConsteval, "Must lex 'consteval'");
    TEST_ASSERT(hasCoAwait, "Must lex 'co_await'");
    TEST_ASSERT(hasCoReturn, "Must lex 'co_return'");
}

void test_cpp20_preprocessor_works() {
    Cpp20Preprocessor pp;
    pp.defineMacro("VERSION", "20");
    pp.defineFunctionMacro("ADD", {"x", "y"}, "(x + y)");

    std::string src = "#ifdef VERSION\nint ver = VERSION;\n#endif\nint sum = ADD(10, 20);\n";
    std::string processed = pp.process(src);

    TEST_ASSERT(processed.find("20") != std::string::npos, "VERSION must be substituted with 20");
    TEST_ASSERT(processed.find("( 10 + 20 )") != std::string::npos || processed.find("10 + 20") != std::string::npos,
                "ADD macro must be expanded");
}

void test_name_lookup_matches_cpp20() {
    Scope rootScope;
    Symbol s1{"globalX", CppType::makeInt64(), false, true, 100};
    rootScope.declare(s1);

    auto childScope = std::make_shared<Scope>(std::make_shared<Scope>(rootScope));
    Symbol s2{"localY", CppType::makeInt64(), false, true, 200};
    childScope->declare(s2);

    TEST_ASSERT(childScope->lookup("localY").has_value(), "Local symbol must be found in scope");
    TEST_ASSERT(childScope->lookup("globalX").has_value(), "Global symbol must be found in child scope");
    TEST_ASSERT(!childScope->lookup("nonExistent").has_value(), "Non-existent symbol must return nullopt");
}

void test_overload_resolution_matches_cpp20() {
    SemanticAnalyzer analyzer;
    // Register int64 overload and float64 overload
    analyzer.registerFunction(FunctionSignature{"calc", CppType::makeInt64(), {CppType::makeInt64()}});
    analyzer.registerFunction(FunctionSignature{"calc", CppType::makeFloat64(), {CppType::makeFloat64()}});

    // Exact match for int64
    auto matchedInt = analyzer.resolveOverload("calc", {CppType::makeInt64()});
    TEST_ASSERT(matchedInt.has_value(), "Must resolve overload");
    TEST_ASSERT(matchedInt->returnType->kind == TypeKind::Int64, "Must select int64 overload");

    // Exact match for float64
    auto matchedFloat = analyzer.resolveOverload("calc", {CppType::makeFloat64()});
    TEST_ASSERT(matchedFloat.has_value(), "Must resolve overload");
    TEST_ASSERT(matchedFloat->returnType->kind == TypeKind::Float64, "Must select float64 overload");
}

void test_constexpr_evaluation_is_correct() {
    Scope scope;
    Symbol c1{"BASE", CppType::makeInt64(), true, true, 50};
    scope.declare(c1);

    // Expression: BASE * 2 + 10 = 110
    auto expr = AstNode::makeBinaryExpr("+",
        AstNode::makeBinaryExpr("*", AstNode::makeIdentifier("BASE"), AstNode::makeLiteralInt(2)),
        AstNode::makeLiteralInt(10)
    );

    auto val = SemanticAnalyzer::evaluateConstexprInt(*expr, scope);
    TEST_ASSERT(val.has_value(), "Constexpr must evaluate");
    TEST_ASSERT(*val == 110, "Constexpr result must equal 110");
}

void test_semantic_validator_guards() {
    // Test UB Guard: Division by literal zero
    auto divZeroExpr = AstNode::makeBinaryExpr("/", AstNode::makeLiteralInt(100), AstNode::makeLiteralInt(0));
    auto stmt = AstNode::makeVariableDecl("x", CppType::makeInt64(), divZeroExpr);
    auto comp = AstNode::makeCompoundStmt({stmt});

    auto valResult = SemanticValidator::validate(*comp);
    TEST_ASSERT(!valResult.allGatesPassed, "Validation must FAIL when division by zero is present");
}

// -------------------------------------------------------------
// 4. Synthesis & Expression Tests
// -------------------------------------------------------------
void test_same_context_generates_same_ast() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    MolecularPool pool;
    pool.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.85);

    ProgramRequirement req;
    req.desiredCapability = 101;
    req.capabilityName = "DeterministicTest";

    ProgramSynthesizer synth(*sys.strand, *sys.resolver, *sys.spliceGraph);
    auto [p1, t1] = synth.synthesize(pool, req, "NAV_A", "NAV_C");
    auto [p2, t2] = synth.synthesize(pool, req, "NAV_A", "NAV_C");

    TEST_ASSERT(p1 != nullptr && p2 != nullptr, "Both syntheses must succeed");
    TEST_ASSERT(t1.astHash == t2.astHash, "Identical context must produce identical AST hash");
    TEST_ASSERT(t1.irHash == t2.irHash, "Identical context must produce identical IR hash");
    TEST_ASSERT(t1.generatedCppSource == t2.generatedCppSource, "Generated C++20 source must be bit-identical");
}

void test_inactive_region_never_enters_program() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    MolecularPool pool;
    // Only ThreatDetected is present -> EnergyCost (Exon D) enhancer will NOT be active
    pool.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.8);

    ProgramRequirement req;
    req.desiredCapability = 101;
    req.capabilityName = "SafeCheck";

    ProgramSynthesizer synth(*sys.strand, *sys.resolver, *sys.spliceGraph);
    auto [p, trace] = synth.synthesize(pool, req, "NAV_A", "NAV_C");

    TEST_ASSERT(p != nullptr, "Synthesis must succeed");
    for (const auto& exonName : trace.splicedExonNames) {
        TEST_ASSERT(exonName != "Exon_D_EnergyCost", "Inactive Exon D must NEVER enter the synthesized program");
        TEST_ASSERT(exonName != "Exon_E_ThreatAvoidance", "Inactive Exon E must NEVER enter the synthesized program");
    }
}

void test_program_provenance_is_complete() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    MolecularPool pool;
    pool.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.85);

    ProgramRequirement req;
    req.desiredCapability = 101;
    req.capabilityName = "ProvenanceTest";

    ProgramSynthesizer synth(*sys.strand, *sys.resolver, *sys.spliceGraph);
    auto [p, trace] = synth.synthesize(pool, req, "NAV_A", "NAV_C", 0, 777);

    TEST_ASSERT(p != nullptr, "Program must be synthesized");
    TEST_ASSERT(p->provenance.sourceRegion != 0, "Source region must be recorded in provenance");
    TEST_ASSERT(p->provenance.enhancer != 0, "Enhancer must be recorded in provenance");
    TEST_ASSERT(!p->provenance.triggeringMolecules.empty(), "Triggering molecules must be recorded");
    TEST_ASSERT(p->provenance.expressionEvent == 777, "Expression event ID must match");
}

// -------------------------------------------------------------
// 5. TATARUS & Organism Tests
// -------------------------------------------------------------
void test_same_genome_different_molecules_changes_expression() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    ProgramSynthesizer synth(*sys.strand, *sys.resolver, *sys.spliceGraph);

    // State A: ThreatDetected
    MolecularPool poolA;
    poolA.setConcentration(0, MoleculeSignatures::NutrientRich, 0.3);
    poolA.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.85);
    ProgramRequirement reqA{101, "ThreatEvasion", {}, {}, 1000.0, 10000.0};
    auto [pA, traceA] = synth.synthesize(poolA, reqA, "NAV_A", "NAV_C");

    // State B: EnergyLow
    MolecularPool poolB;
    poolB.setConcentration(0, MoleculeSignatures::NutrientRich, 0.3);
    poolB.setConcentration(0, MoleculeSignatures::EnergyLow, 0.90);
    ProgramRequirement reqB{102, "EnergyNav", {}, {}, 1000.0, 10000.0};
    auto [pB, traceB] = synth.synthesize(poolB, reqB, "NAV_A", "NAV_E");

    TEST_ASSERT(pA != nullptr && pB != nullptr, "Both programs must be synthesized from same genome");
    TEST_ASSERT(traceA.astHash != traceB.astHash, "Different molecular states must produce different ASTs");
    TEST_ASSERT(traceA.splicedExonNames != traceB.splicedExonNames, "Different exons must be spliced");
}

void test_same_genome_different_history_changes_phenotype_not_genome() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    uint64_t initialHash = sys.strand->getGenomeHash();

    LineageContext lineage{TissueLineage::AnteriorNervous, 0, "AnteriorCortical"};
    MolecularPool pool;

    // Simulate extensive organism experience & growth
    sys.phenotypeBuilder->developEntity(1, lineage, pool, 10);
    sys.phenotypeBuilder->developEntity(1, lineage, pool, 20);
    sys.phenotypeBuilder->setStage(NeuralDevelopmentStage::Mature);
    sys.phenotypeBuilder->developEntity(4, lineage, pool, 30);

    TEST_ASSERT(sys.phenotypeBuilder->countByCategory(EntityCategory::Neuron) == 2, "Phenotype has 2 neurons");
    TEST_ASSERT(sys.phenotypeBuilder->countByCategory(EntityCategory::Program) == 1, "Phenotype has 1 program");

    // Invariant: Genome remains strictly untouched
    TEST_ASSERT(sys.strand->getGenomeHash() == initialHash, "Genome hash must remain identical despite experience");
}

void test_behavior_trace_reaches_genome_region() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    MolecularPool pool;
    pool.setConcentration(0, MoleculeSignatures::ThreatDetected, 0.85);

    ProgramRequirement req{101, "EvasionAction", {}, {}, 1000.0, 10000.0};
    ProgramSynthesizer synth(*sys.strand, *sys.resolver, *sys.spliceGraph);
    auto [prog, trace] = synth.synthesize(pool, req, "NAV_A", "NAV_C");

    auto steps = CausalTrace::traceProgramBackwards(*prog, trace);
    TEST_ASSERT(steps.size() >= 5, "Causal trace must span at least 5 layers");
    TEST_ASSERT(steps[0].stepName.find("IR") != std::string::npos, "First step must be execution");
    TEST_ASSERT(steps.back().stepName.find("Trigger") != std::string::npos, "Terminal step must be trigger");
}

// -------------------------------------------------------------
// 6. Extended Section 22 Tests (Full C++20, Parser, Classes, etc.)
// -------------------------------------------------------------
void test_corrupted_region_and_dangling_reference_rejected() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    auto regions = sys.strand->getAllRegions();
    TEST_ASSERT(!regions.empty(), "Regions must not be empty");

    // Introduce dangling reference in a test region copy
    auto testRegions = regions;
    testRegions.back().referencedRegions.push_back(999999); // non-existent region

    std::vector<std::string> errors;
    bool valid = GenomeIntegrity::validateReferenceGraph(testRegions, errors);
    TEST_ASSERT(!valid, "Reference graph with dangling pointer must fail validation");
    TEST_ASSERT(!errors.empty(), "Errors must report dangling reference");
}

void test_cpp20_parser_classes_and_inheritance() {
    std::string code = "class Derived : public Base { public: int x; virtual void run(); };";
    auto tokens = Cpp20Lexicon::tokenize(code);
    Cpp20Parser parser(tokens);
    auto node = parser.parseDeclaration();
    TEST_ASSERT(node != nullptr, "Parser must parse class declaration");
    TEST_ASSERT(node->kind == AstNodeKind::RecordDecl, "Node must be RecordDecl");
    TEST_ASSERT(node->name == "Derived", "Class name must be Derived");
    TEST_ASSERT(!node->children.empty(), "Class must contain member declarations");
}

void test_cpp20_parser_templates_and_concepts() {
    std::string conceptCode = "concept Addable = requires(int a, int b) { (a + b); };";
    auto tokens = Cpp20Lexicon::tokenize(conceptCode);
    Cpp20Parser parser(tokens);
    auto cNode = parser.parseDeclaration();
    TEST_ASSERT(cNode != nullptr, "Parser must parse concept");
    TEST_ASSERT(cNode->kind == AstNodeKind::ConceptDecl, "Node must be ConceptDecl");
    TEST_ASSERT(cNode->name == "Addable", "Concept name must be Addable");

    std::string templateCode = "template <typename T> requires Integral int process(T x);";
    auto tTokens = Cpp20Lexicon::tokenize(templateCode);
    Cpp20Parser tParser(tTokens);
    auto tNode = tParser.parseDeclaration();
    TEST_ASSERT(tNode != nullptr, "Parser must parse template");
    TEST_ASSERT(tNode->kind == AstNodeKind::TemplateDecl, "Node must be TemplateDecl");
}

void test_cpp20_parser_lambdas_and_coroutines() {
    std::string lambdaCode = "auto l = [=](int x) mutable { co_await susp; return x; };";
    auto tokens = Cpp20Lexicon::tokenize(lambdaCode);
    Cpp20Parser parser(tokens);
    auto stmt = parser.parseStatement();
    TEST_ASSERT(stmt != nullptr, "Parser must parse lambda statement");
    TEST_ASSERT(stmt->kind == AstNodeKind::VariableDecl, "Statement must be variable declaration");
    TEST_ASSERT(!stmt->children.empty() && stmt->children[0]->kind == AstNodeKind::LambdaExpr,
                "Initializer must be LambdaExpr");
}

void test_classes_construction_destruction_order_and_vtable() {
    ClassRegistry registry;
    ClassModel baseCls;
    baseCls.name = "Base";
    MethodDefinition m1{"speak", CppType::makeVoid(), AccessSpecifier::Public, true, false, false, false, false, 0};
    baseCls.methods.push_back(m1);
    registry.registerClass(baseCls);

    ClassModel derivedCls;
    derivedCls.name = "Derived";
    derivedCls.baseClasses.push_back(BaseClassSpecifier{"Base", AccessSpecifier::Public, false});
    derivedCls.memberVariables.push_back(FieldDefinition{"id", CppType::makeInt64(), 0, true});
    MethodDefinition m2{"speak", CppType::makeVoid(), AccessSpecifier::Public, true, false, true, false, false, 0};
    derivedCls.methods.push_back(m2);
    registry.registerClass(derivedCls);

    auto constructOrder = derivedCls.getConstructionOrder();
    TEST_ASSERT(constructOrder.size() == 3, "Construction order must have 3 steps");
    TEST_ASSERT(constructOrder[0] == "Base::Base", "Base must construct first");
    TEST_ASSERT(constructOrder[1] == "Member::id", "Members construct second");
    TEST_ASSERT(constructOrder[2] == "Derived::Constructor", "Derived constructor runs third");

    auto destructOrder = derivedCls.getDestructionOrder();
    TEST_ASSERT(destructOrder[0] == "Derived::Constructor", "Derived destructs first");
    TEST_ASSERT(destructOrder[2] == "Base::Base", "Base destructs last");

    TEST_ASSERT(registry.isDerivedFrom("Derived", "Base"), "Derived must derive from Base");
    auto vDisp = derivedCls.dispatchVirtual(0);
    TEST_ASSERT(vDisp.has_value() && *vDisp == "Derived::speak", "Virtual dispatch must resolve override");
}

void test_templates_fold_expressions_and_subsumption() {
    // Fold sum: 10 + 20 + 30 + 40 = 100
    int64_t sum = TemplateEngine::evaluateFoldSum({10, 20, 30, 40});
    TEST_ASSERT(sum == 100, "Fold sum must equal 100");

    // Fold all (&&): true && true && false = false
    bool allTrue = TemplateEngine::evaluateFoldAll({true, true, false});
    TEST_ASSERT(!allTrue, "Fold all must equal false");

    TemplateEngine engine;
    TEST_ASSERT(engine.isMoreConstrained("Integral", "Any"), "Integral must subsume Any");
    TEST_ASSERT(engine.isMoreConstrained("SignedIntegral", "Integral"), "SignedIntegral must subsume Integral");
    TEST_ASSERT(!engine.isMoreConstrained("Integral", "SignedIntegral"), "Integral must not subsume SignedIntegral");
}

void test_constexpr_engine_functions_and_static_assert() {
    ConstexprEngine engine;

    // Define constexpr int square(int x) { return x * x; }
    auto paramNode = AstNode::makeVariableDecl("x", CppType::makeInt64());
    auto multExpr = AstNode::makeBinaryExpr("*", AstNode::makeIdentifier("x"), AstNode::makeIdentifier("x"));
    auto retStmt = AstNode::makeReturnStmt(multExpr);
    auto body = AstNode::makeCompoundStmt({retStmt});
    auto fnDecl = AstNode::makeFunctionDecl("square", CppType::makeInt64(), {{"x", CppType::makeInt64()}}, body, true);
    engine.registerConstexprFunction("square", fnDecl);

    // Call square(7) at compile time -> 49
    auto callExpr = AstNode::makeCallExpr("square", {AstNode::makeLiteralInt(7)});
    auto res = engine.evaluateExpression(*callExpr);
    TEST_ASSERT(res.has_value(), "Constexpr function call must evaluate at compile time");
    TEST_ASSERT(res->iVal == 49, "square(7) must evaluate to 49");

    // Static assert verification
    auto assertCond = AstNode::makeBinaryExpr("==", callExpr, AstNode::makeLiteralInt(49));
    std::string msg;
    bool pass = engine.evaluateStaticAssert(*assertCond, msg);
    TEST_ASSERT(pass, "static_assert(square(7) == 49) must pass");
}

void test_exceptions_stack_unwinding_and_destructors() {
    ExceptionEngine exEngine;
    exEngine.pushScope("outerFunction");
    exEngine.pushScope("innerFunction");

    bool dtor1Ran = false;
    bool dtor2Ran = false;
    exEngine.registerDestructor([&]() { dtor1Ran = true; });
    exEngine.registerDestructor([&]() { dtor2Ran = true; });

    ExceptionRecord ex{"std::runtime_error", "Simulated exception", 42};
    std::vector<std::string> unwoundDtors;
    bool caught = exEngine.unwindStackUntilCatch(ex, {"std::runtime_error"}, unwoundDtors);

    TEST_ASSERT(caught, "Exception must match catch block");
    TEST_ASSERT(dtor1Ran && dtor2Ran, "All stack destructors must execute during unwinding");
    TEST_ASSERT(!unwoundDtors.empty(), "Unwound destructors must be recorded in trace");
}

void test_memory_model_and_data_race_detection() {
    MemoryModelEngine mm;
    MemoryAccessRecord r1{0x2000, 8, true, 1, 100, CppMemoryOrder::Relaxed};
    mm.recordAccess(r1);

    MemoryAccessRecord r2{0x2000, 8, false, 2, 101, CppMemoryOrder::Relaxed};
    std::string raceMsg;
    bool race = mm.checkDataRace(r2, raceMsg);
    TEST_ASSERT(race, "Concurrent relaxed read and write on different threads must be detected as data race");

    // UB Guard test: null dereference and use-after-free
    UbGuard guard;
    guard.registerAllocation(0x5000, 64);
    auto accessOk = guard.verifyAccess(0x5000, 32, false);
    TEST_ASSERT(!accessOk.hasViolation, "Valid memory access must pass");

    guard.registerFree(0x5000);
    auto accessUaf = guard.verifyAccess(0x5000, 32, false);
    TEST_ASSERT(accessUaf.hasViolation, "Access to freed memory must trigger UseAfterFree UB Guard");
    TEST_ASSERT(accessUaf.kind == UbViolationKind::UseAfterFree, "Violation kind must be UseAfterFree");
}

void test_genomic_rule_engine_drives_cpp_semantics() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    TEST_ASSERT(sys.ruleEngine != nullptr, "System must possess a GenomicRuleEngine");
    TEST_ASSERT(sys.ruleEngine->getRuleCount() > 0, "Genomic rules must be loaded from genome strand");

    // Operator + queried from genome
    auto addRule = sys.ruleEngine->resolveBinaryOp("+", TypeKind::Int64, TypeKind::Int64);
    TEST_ASSERT(addRule.has_value(), "Genome must contain binary rule for '+' on int64");
    TEST_ASSERT(addRule->resultKind == TypeKind::Int64, "Result kind from genome rule must be int64");
    TEST_ASSERT(addRule->loweringOpcode == IrOpcode::Add, "Lowering opcode from genome rule must be Add");

    // Concept queried from genome
    bool intIsIntegral = sys.ruleEngine->checkConceptSatisfaction("Integral", TypeKind::Int32);
    TEST_ASSERT(intIsIntegral, "Genome concept rule must satisfy Integral for int32");
    bool floatIsIntegral = sys.ruleEngine->checkConceptSatisfaction("Integral", TypeKind::Float32);
    TEST_ASSERT(!floatIsIntegral, "Genome concept rule must NOT satisfy Integral for float");
}

void test_evolution_recombination_and_mutation() {
    auto parentA = GenomeBootstrap::buildCompleteSystem();
    auto parentB = GenomeBootstrap::buildCompleteSystem();

    auto [offspring, report] = GenomeEvolution::recombine(*parentA.strand, *parentB.strand, 30000012);
    TEST_ASSERT(offspring != nullptr, "Offspring genome must be created");
    TEST_ASSERT(report.isViable, "Recombined genome must be viable with valid reference graph");
    TEST_ASSERT(offspring->isFrozen(), "Offspring genome must be frozen and immutable");

    // Mutate parameter via Evolution API
    auto mutated = GenomeEvolution::mutate(*parentA.strand, MutationType::ParameterMutation, 30000010, {1, 2, 3, 4});
    TEST_ASSERT(mutated != nullptr, "Mutated genome must be created");
    TEST_ASSERT(mutated->getGenomeHash() != parentA.strand->getGenomeHash(), "Mutated genome must have different hash");
}

void test_persistence_snapshot_roundtrip() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    MolecularPool pool;
    pool.setConcentration(0, MoleculeSignatures::EnergyLow, 0.77, "EnergyLow");
    sys.phenotypeBuilder->setStage(NeuralDevelopmentStage::Juvenile);

    auto snap = GenomePersistence::captureSnapshot(*sys.strand, pool, *sys.phenotypeBuilder, 500);
    std::string serialized = GenomePersistence::serialize(snap);
    auto restoredSnap = GenomePersistence::deserialize(serialized);

    TEST_ASSERT(restoredSnap.genomeHash == snap.genomeHash, "Genome hash must roundtrip in persistence");
    TEST_ASSERT(restoredSnap.timestampTick == 500, "Timestamp tick must roundtrip");
    TEST_ASSERT(restoredSnap.developmentalStage == NeuralDevelopmentStage::Juvenile, "Stage must roundtrip");
    TEST_ASSERT(restoredSnap.molecularPoolState.size() == 1, "Molecular pool must roundtrip");
    TEST_ASSERT(restoredSnap.stateHash == snap.stateHash, "Snapshot state hash must match 100% deterministically");
}

void test_genomic_stdlib_algorithms_and_dijkstra() {
    TEST_ASSERT(GenomeStdLib::gcd(48, 18) == 6, "gcd(48, 18) must equal 6");
    TEST_ASSERT(GenomeStdLib::lcm(12, 18) == 36, "lcm(12, 18) must equal 36");
    TEST_ASSERT(GenomeStdLib::clamp(std::int64_t{150}, std::int64_t{0}, std::int64_t{100}) == 100LL, "clamp(150, 0, 100) must equal 100");

    std::vector<int64_t> vec = {5, 2, 8, 1, 9};
    GenomeStdLib::sort(vec);
    TEST_ASSERT(vec[0] == 1 && vec[4] == 9, "Vector must be sorted");
    TEST_ASSERT(GenomeStdLib::binarySearch(vec, 8), "binary_search must find 8");
    TEST_ASSERT(!GenomeStdLib::binarySearch(vec, 42), "binary_search must not find 42");

    // Dijkstra graph: 0 -> 1 (w=2), 1 -> 2 (w=3), 0 -> 2 (w=10)
    std::vector<std::tuple<size_t, size_t, double>> edges = {
        {0, 1, 2.0},
        {1, 2, 3.0},
        {0, 2, 10.0}
    };
    auto path = GenomeStdLib::dijkstraShortestPath(3, edges, 0, 2);
    TEST_ASSERT(path.size() == 3, "Shortest path must have 3 nodes");
    TEST_ASSERT(path[0] == 0 && path[1] == 1 && path[2] == 2, "Path must be 0 -> 1 -> 2 with total cost 5");

    // State machine test
    GenomeStdLib::StateMachine sm(0);
    sm.addTransition(0, 1, 2); // state 0 + event 1 -> state 2
    bool ok = sm.triggerEvent(1);
    TEST_ASSERT(ok, "State machine transition must succeed");
    TEST_ASSERT(sm.getCurrentState() == 2, "State must be 2");
}

void test_genomic_type_system_taxonomy() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    auto infoI32 = sys.ruleEngine->getTypeInfo(TypeKind::Int32);
    TEST_ASSERT(infoI32.has_value(), "Genome must contain type definition for int32_t");
    TEST_ASSERT(infoI32->sizeBytes == 4, "int32_t size must be 4 bytes");
    TEST_ASSERT(infoI32->isIntegral && infoI32->isSigned, "int32_t must be signed integral");

    auto infoF64 = sys.ruleEngine->getTypeInfo(TypeKind::Float64);
    TEST_ASSERT(infoF64.has_value(), "Genome must contain type definition for double");
    TEST_ASSERT(infoF64->sizeBytes == 8, "double size must be 8 bytes");
    TEST_ASSERT(infoF64->isFloatingPoint, "double must be floating point");

    auto infoPtr = sys.ruleEngine->getTypeInfo(TypeKind::Pointer);
    TEST_ASSERT(infoPtr.has_value() && infoPtr->isPointer, "Genome must contain pointer definition");
}

void test_genomic_arithmetic_conversions_and_promotions() {
    auto sys = GenomeBootstrap::buildCompleteSystem();

    // Usual arithmetic conversions (UAC) queried directly from genome
    auto c1 = sys.ruleEngine->getCommonType(TypeKind::Int32, TypeKind::Int64);
    TEST_ASSERT(c1.has_value() && *c1 == TypeKind::Int64, "int32 + int64 must result in int64 via genomic UAC");

    auto c2 = sys.ruleEngine->getCommonType(TypeKind::Int32, TypeKind::Float64);
    TEST_ASSERT(c2.has_value() && *c2 == TypeKind::Float64, "int32 + double must result in double via genomic UAC");

    auto c3 = sys.ruleEngine->getCommonType(TypeKind::Float32, TypeKind::Float64);
    TEST_ASSERT(c3.has_value() && *c3 == TypeKind::Float64, "float + double must result in double via genomic UAC");

    auto c4 = sys.ruleEngine->getCommonType(TypeKind::UInt32, TypeKind::UInt64);
    TEST_ASSERT(c4.has_value() && *c4 == TypeKind::UInt64, "uint32 + uint64 must result in uint64 via genomic UAC");

    // Conversion costs from genome
    TEST_ASSERT(sys.ruleEngine->getConversionCost(TypeKind::Int32, TypeKind::Int64) == 2,
                "int32 to int64 is an integral conversion, not an integral promotion");
    TEST_ASSERT(sys.ruleEngine->getConversionCost(TypeKind::UInt32, TypeKind::UInt64) == 2,
                "uint32 to uint64 is an integral conversion, not an integral promotion");
    TEST_ASSERT(sys.ruleEngine->getConversionCost(TypeKind::Char, TypeKind::Int32) == 1, "Char->Int32 promotion cost must be 1");
    TEST_ASSERT(sys.ruleEngine->getConversionCost(TypeKind::Int32, TypeKind::Float64) == 2, "Int32->Double conversion cost must be 2");
}

void test_genomic_compiler_lowering_rules() {
    auto sys = GenomeBootstrap::buildCompleteSystem();

    // Query lowering rules from genome
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:+") == IrOpcode::Add, "Lowering rule for '+' must be Add");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:-") == IrOpcode::Sub, "Lowering rule for '-' must be Sub");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:*") == IrOpcode::Mul, "Lowering rule for '*' must be Mul");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:/") == IrOpcode::Div, "Lowering rule for '/' must be Div");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:%") == IrOpcode::Mod, "Lowering rule for '%' must be Mod");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("binop:<<") == IrOpcode::BitShl, "Lowering rule for '<<' must be BitShl");
    TEST_ASSERT(sys.ruleEngine->getLoweringOpcode("stmt:return") == IrOpcode::Return, "Lowering rule for return must be Return");

    // Compile an AST function using GenomeCompiler wired to GenomeRuleEngine
    GenomeCompiler compiler(sys.ruleEngine.get());
    auto binExpr = AstNode::makeBinaryExpr("+", AstNode::makeLiteralInt(10), AstNode::makeLiteralInt(20));
    auto fnDecl = AstNode::makeFunctionDecl("compute", CppType::makeInt64(), {}, binExpr);
    auto module = compiler.compile(*fnDecl, "TestBinopModule");
    TEST_ASSERT(module != nullptr, "Compiled module must not be null");
    TEST_ASSERT(!module->functions.empty(), "Module must contain entry function");
}

void test_genomic_concept_subsumption_hierarchy() {
    auto sys = GenomeBootstrap::buildCompleteSystem();

    // Concept satisfaction
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("integral", TypeKind::Int32), "int32 must satisfy integral");
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("signed_integral", TypeKind::Int32), "int32 must satisfy signed_integral");
    TEST_ASSERT(!sys.ruleEngine->checkConceptSatisfaction("signed_integral", TypeKind::UInt32), "uint32 must NOT satisfy signed_integral");
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("unsigned_integral", TypeKind::UInt32), "uint32 must satisfy unsigned_integral");
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("floating_point", TypeKind::Float64), "double must satisfy floating_point");
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("arithmetic", TypeKind::Int64), "int64 must satisfy arithmetic");
    TEST_ASSERT(sys.ruleEngine->checkConceptSatisfaction("arithmetic", TypeKind::Float64), "double must satisfy arithmetic");

    // Concept subsumption
    TEST_ASSERT(sys.ruleEngine->subsumes("signed_integral", "integral"), "signed_integral must subsume integral");
    TEST_ASSERT(sys.ruleEngine->subsumes("unsigned_integral", "integral"), "unsigned_integral must subsume integral");
    TEST_ASSERT(sys.ruleEngine->subsumes("arithmetic", "integral"), "arithmetic must subsume integral");
    TEST_ASSERT(sys.ruleEngine->subsumes("arithmetic", "floating_point"), "arithmetic must subsume floating_point");
    TEST_ASSERT(!sys.ruleEngine->subsumes("integral", "signed_integral"), "integral must NOT subsume signed_integral");
}

void test_genomic_mode_rejects_unknown_operators() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    auto expr = AstNode::makeBinaryExpr("???", AstNode::makeLiteralInt(1), AstNode::makeLiteralInt(2));
    SemanticAnalyzer analyzer(sys.ruleEngine.get());
    TEST_ASSERT(!analyzer.analyze(*expr), "Unknown genomic operator must be rejected semantically");
    GenomeCompiler compiler(sys.ruleEngine.get());
    auto fn = AstNode::makeFunctionDecl("invalid", CppType::makeInt64(), {}, expr);
    bool threw = false;
    try { (void)compiler.compile(*fn, "UnknownOperator"); }
    catch (const std::runtime_error&) { threw = true; }
    TEST_ASSERT(threw, "Unknown genomic operator must not lower as Add");
}

void test_overload_incomparable_conversion_sequences_are_ambiguous() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    SemanticAnalyzer analyzer(sys.ruleEngine.get());
    analyzer.registerFunction(FunctionSignature{"choose", CppType::makeInt32(),
        {CppType::makeInt32(), CppType::makeFloat64()}});
    analyzer.registerFunction(FunctionSignature{"choose", CppType::makeInt64(),
        {CppType::makeInt64(), CppType::makeInt32()}});
    auto match = analyzer.resolveOverload("choose", {CppType::makeInt32(), CppType::makeInt32()});
    TEST_ASSERT(!match.has_value(), "Neither overload is better on all arguments: call must be ambiguous");
}

void test_genomic_rule_decoding_fails_closed() {
    GenomeStrand strand;
    std::string badType = "type:totally_unknown_type:8:8:1:0:1:0:0;";
    strand.addRegion(RegionKind::TypeSystem, "InvalidType", {},
        std::vector<uint8_t>(badType.begin(), badType.end()));
    GenomicRuleEngine engine;
    bool rejected = false;
    try { engine.loadRulesFromGenome(strand); }
    catch (const std::invalid_argument&) { rejected = true; }
    TEST_ASSERT(rejected, "Unknown type cannot silently decode as int64_t");

    GenomeStrand strand2;
    std::string badOp = "lowering:binop:+:UnrecognizedInstruction;";
    strand2.addRegion(RegionKind::CompilerLogic, "InvalidOpcode", {},
        std::vector<uint8_t>(badOp.begin(), badOp.end()));
    rejected = false;
    try { engine.loadRulesFromGenome(strand2); }
    catch (const std::invalid_argument&) { rejected = true; }
    TEST_ASSERT(rejected, "Unknown opcode cannot silently decode as Nop");
}

// -------------------------------------------------------------
// GPI: genome-driven lexical/grammar behavior, not host precedence.
// -------------------------------------------------------------
static GenomeStrand buildGrammarTestStrand(const std::string& payload) {
    GenomeStrand strand;
    const auto bases = GenomeDecoder::encodeRegion(RegionKind::CppGrammar, "TestGrammar",
        std::vector<uint8_t>(payload.begin(), payload.end()));
    strand.addRegion(RegionKind::CppGrammar, "TestGrammar", bases,
        std::vector<uint8_t>(payload.begin(), payload.end()));
    strand.freeze();
    return strand;
}

void test_genomic_parser_reads_frozen_genome_precedence() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    auto tu = Cpp20Parser::parseWithGenome("int x = 1 + 2 * 3;", *sys.ruleEngine);
    TEST_ASSERT(tu && tu->children.size() == 1, "Genomic parser must produce one declaration");
    auto init = tu->children[0]->children.at(0);
    TEST_ASSERT(init->op == "+" && init->children.at(1)->op == "*",
        "Normal C++ precedence must come from the frozen genomic CppGrammar");
    TEST_ASSERT(sys.ruleEngine->getExpressionGrammar().binaryRuleCount() >= 20,
        "Genome must provide binary operator precedence data");
}

void test_genomic_grammar_alteration_changes_ast_without_host_changes() {
    const std::string header =
        "grammar:declaration=function_decl|variable_decl;"
        "grammar:statement=compound_stmt|return_stmt|expression_stmt;"
        "grammar:keywords=int,return;";
    auto conventional = buildGrammarTestStrand(header +
        "precedence:+:10:left;precedence:*:20:left;prefix:-;");
    auto alternate = buildGrammarTestStrand(header +
        "precedence:+:20:left;precedence:*:10:left;prefix:-;");
    TEST_ASSERT(conventional.getGenomeHash() != alternate.getGenomeHash(),
        "Different genomic grammar must change immutable genome identity");
    GenomicRuleEngine conventionalRules, alternateRules;
    conventionalRules.loadRulesFromGenome(conventional);
    alternateRules.loadRulesFromGenome(alternate);
    const auto a = Cpp20Parser::parseWithGenome("int x = 1 + 2 * 3;", conventionalRules);
    const auto b = Cpp20Parser::parseWithGenome("int x = 1 + 2 * 3;", alternateRules);
    const auto exprA = a->children.at(0)->children.at(0);
    const auto exprB = b->children.at(0)->children.at(0);
    TEST_ASSERT(exprA->op == "+" && exprA->children.at(1)->op == "*",
        "Conventional genome must group multiplication first");
    TEST_ASSERT(exprB->op == "*" && exprB->children.at(0)->op == "+",
        "Changing only genome precedence must change the generated AST");
}

void test_genomic_grammar_associativity_and_missing_rule_fail_closed() {
    const std::string header =
        "grammar:declaration=variable_decl;grammar:statement=expression_stmt;"
        "grammar:keywords=int;";
    auto strand = buildGrammarTestStrand(header + "precedence:=:1:right;");
    GenomicRuleEngine rules;
    rules.loadRulesFromGenome(strand);
    const auto tu = Cpp20Parser::parseWithGenome("int x = a = b = c;", rules);
    const auto expr = tu->children.at(0)->children.at(0);
    TEST_ASSERT(expr->op == "=" && expr->children.at(1)->op == "=",
        "Assignment must associate right-to-left from genomic rule");
    bool rejected = false;
    try { (void)Cpp20Parser::parseWithGenome("int x = 1 + 2;", rules); }
    catch (const std::runtime_error&) { rejected = true; }
    TEST_ASSERT(rejected, "Missing genomic operator must not be silently parsed");
}

void test_genomic_declaration_and_keyword_gates() {
    const std::string base =
        "grammar:declaration=variable_decl;grammar:statement=expression_stmt;"
        "grammar:keywords=int,namespace;precedence:+:11:left;";
    auto strand = buildGrammarTestStrand(base);
    GenomicRuleEngine rules;
    rules.loadRulesFromGenome(strand);
    bool rejected = false;
    try { (void)Cpp20Parser::parseWithGenome("namespace N { int x; }", rules); }
    catch (const std::runtime_error&) { rejected = true; }
    TEST_ASSERT(rejected, "Namespace must be forbidden by genome declaration production");

    auto noKeyword = buildGrammarTestStrand(
        "grammar:declaration=variable_decl|namespace_decl;"
        "grammar:statement=expression_stmt;grammar:keywords=int;precedence:+:11:left;");
    GenomicRuleEngine noKeywordRules;
    noKeywordRules.loadRulesFromGenome(noKeyword);
    const auto tokens = Cpp20Lexicon::tokenize("namespace N {}", noKeywordRules.getExpressionGrammar());
    TEST_ASSERT(tokens.at(0).type == TokenType::Identifier,
        "Keyword classification must depend on genomic keyword region");
    rejected = false;
    try { (void)Cpp20Parser::parseWithGenome("namespace N {}", noKeywordRules); }
    catch (const std::runtime_error&) { rejected = true; }
    TEST_ASSERT(rejected, "Removing namespace keyword from genome must disable parsing");
}

void test_genomic_conversion_rules_change_overload_viability() {
    GenomeStrand allowed, denied;
    const std::string conversion = "conv:int32_t:double:2:1:integer_to_floating;";
    const auto bases = GenomeDecoder::encodeRegion(RegionKind::CppConversionRule, "Conversion",
        std::vector<uint8_t>(conversion.begin(), conversion.end()));
    allowed.addRegion(RegionKind::CppConversionRule, "Conversion", bases,
        std::vector<uint8_t>(conversion.begin(), conversion.end()));
    allowed.freeze();
    denied.freeze();
    GenomicRuleEngine rulesAllow, rulesDeny;
    rulesAllow.loadRulesFromGenome(allowed);
    rulesDeny.loadRulesFromGenome(denied);
    SemanticAnalyzer a(&rulesAllow), b(&rulesDeny);
    const FunctionSignature fn{"f", CppType::makeInt32(), {CppType::makeFloat64()}};
    a.registerFunction(fn);
    b.registerFunction(fn);
    TEST_ASSERT(a.resolveOverload("f", {CppType::makeInt32()}).has_value(),
        "Genomic conversion rule must make overload viable");
    TEST_ASSERT(!b.resolveOverload("f", {CppType::makeInt32()}).has_value(),
        "Removing genomic conversion rule must make overload nonviable");
}

void test_genomic_parser_rejects_truncated_syntax() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    bool rejected = false;
    try { (void)Cpp20Parser::parseWithGenome("int f() { return 1;", *sys.ruleEngine); }
    catch (const std::runtime_error&) { rejected = true; }
    TEST_ASSERT(rejected, "Genomic parser must reject unterminated compound statements");
    rejected = false;
    try { (void)Cpp20Parser::parseWithGenome("int x = @;", *sys.ruleEngine); }
    catch (const std::runtime_error&) { rejected = true; }
    TEST_ASSERT(rejected, "Genomic parser must not synthesize a dummy literal on malformed input");
}

void test_genomic_grammar_malformed_payload_rejected() {
    const std::string header =
        "grammar:declaration=variable_decl;grammar:statement=expression_stmt;"
        "grammar:keywords=int;";
    bool rejected = false;
    try { (void)GenomicExpressionGrammar::fromPayload(header + "precedence:+:abc:left;"); }
    catch (const std::invalid_argument&) { rejected = true; }
    TEST_ASSERT(rejected, "Non-numeric precedence must be rejected");
    rejected = false;
    try { (void)GenomicExpressionGrammar::fromPayload(header +
        "precedence:+:10:left;precedence:+:11:right;"); }
    catch (const std::invalid_argument&) { rejected = true; }
    TEST_ASSERT(rejected, "Duplicate genomic operator definitions must be rejected");
}

// -------------------------------------------------------------
// GPI: prompt -> genomic molecules -> regulation -> compositional source
// -------------------------------------------------------------
void test_gpi_prompt_sensor_csv_alarm() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    auto r = resolver.resolve("Überwache 10 Temperatursensoren, speichere Messwerte als CSV und löse Alarm ab 80 aus");
    TEST_ASSERT(r.supported, "Sensor prompt must resolve");
    TEST_ASSERT(r.expectedSamples && *r.expectedSamples == 10, "Sample count must be parsed");
    TEST_ASSERT(r.threshold && *r.threshold == 80, "Alarm threshold must be parsed");
    TEST_ASSERT(r.thresholdMode == gpi::ThresholdMode::GreaterEqual, "'ab' means >=");
    TEST_ASSERT(r.capabilities.size() == 3, "Input/CSV/alarm capabilities required");
    for (const auto& c : r.capabilities) {
        TEST_ASSERT(r.regulation.activeRegions.contains(c.regionId), "Molecules must activate genomic capability locus");
        TEST_ASSERT(r.molecules.getConcentration(0, c.moleculeSignature) > 0.0, "Capability needs molecular signal");
    }
    const auto artifact = gpi::PromptProgramSynthesizer::synthesize(r);
    TEST_ASSERT(artifact.generated, "Sensor application must be synthesized");
    TEST_ASSERT(artifact.source.find("std::ofstream csv") != std::string::npos, "CSV code required");
    TEST_ASSERT(artifact.source.find("sample >= 80LL") != std::string::npos, "Alarm comparator required");
    TEST_ASSERT(artifact.source.find("seen < 10") != std::string::npos, "Fixed sample bound required");
    TEST_ASSERT(sys.strand->isFrozen(), "Prompt resolution must not mutate genome");
    TEST_ASSERT(r.genomeHash == sys.strand->getGenomeHash(), "Provenance must preserve genome hash");
}

void test_gpi_prompt_statistics_independent_program() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto sensor = resolver.resolve("Überwache 10 Sensoren, speichere CSV und löse Alarm ab 80 aus");
    const auto stats = resolver.resolve("Erstelle ein Programm, das 5 Zahlen einliest und den Durchschnitt sowie das Maximum ausgibt");
    TEST_ASSERT(sensor.supported && stats.supported, "Both unrelated tasks must resolve");
    const auto a = gpi::PromptProgramSynthesizer::synthesize(sensor);
    const auto b = gpi::PromptProgramSynthesizer::synthesize(stats);
    TEST_ASSERT(a.generated && b.generated && a.source != b.source, "Different prompts must yield different programs");
    TEST_ASSERT(b.source.find("MEAN=") != std::string::npos, "Mean output required");
    TEST_ASSERT(b.source.find("MAX=") != std::string::npos, "Maximum output required");
    TEST_ASSERT(b.source.find("ALARM sensor=") == std::string::npos, "Unrequested alarm must not be emitted");
    TEST_ASSERT(b.source.find("std::ofstream") == std::string::npos, "Unrequested CSV must not be emitted");
    TEST_ASSERT(stats.expectedSamples && *stats.expectedSamples == 5, "Statistical sample count");
}

void test_gpi_prompt_threshold_count_and_direction() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto above = resolver.resolve("Zähle bei 6 Messwerten alle Werte über 20");
    TEST_ASSERT(above.supported, "Count-above task must resolve");
    TEST_ASSERT(above.threshold && *above.threshold == 20, "Threshold parsed");
    TEST_ASSERT(above.thresholdMode == gpi::ThresholdMode::Greater, "'über' must mean >");
    const auto program = gpi::PromptProgramSynthesizer::synthesize(above);
    TEST_ASSERT(program.generated && program.source.find("sample > 20LL") != std::string::npos,
                "Count predicate must match strict threshold");
    const auto below = resolver.resolve("Zähle 3 Werte unter -5");
    TEST_ASSERT(below.supported, "Negative threshold task must resolve");
    TEST_ASSERT(below.threshold && *below.threshold == -5, "Negative threshold parsed");
    TEST_ASSERT(gpi::PromptProgramSynthesizer::synthesize(below).source.find("sample < -5LL") != std::string::npos,
                "Negative threshold comparison must be emitted");
}

void test_gpi_prompt_fails_closed_on_unsupported_tasks() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    for (const std::string prompt : {
        "Schreibe einen HTTP Server mit 10 Verbindungen",
        "Lies 5 Zahlen und sende sie per E-Mail",
        "Erstelle eine GUI mit Datenbank und Sensoren",
        "Sortiere 10 Zahlen mit Quicksort"
    }) {
        const auto r = resolver.resolve(prompt);
        TEST_ASSERT(!r.supported, "Unsupported requirements must not be silently ignored");
        TEST_ASSERT(!gpi::PromptProgramSynthesizer::synthesize(r).generated, "Unsupported task must emit no code");
    }
}

void test_gpi_prompt_missing_threshold_rejected() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto r = resolver.resolve("Überwache 10 Sensoren und gib Alarm bei kritischen Abweichungen");
    TEST_ASSERT(!r.supported, "No numeric alarm threshold must be rejected");
    TEST_ASSERT(!gpi::PromptProgramSynthesizer::synthesize(r).generated, "No unvalidated alarm code");
}

void test_gpi_prompt_language_and_spelled_number() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto german = resolver.resolve("Überwache zehn Sensoren und speichere Messwerte als CSV");
    TEST_ASSERT(german.supported && german.expectedSamples && *german.expectedSamples == 10,
                "German number words and umlauts must work");
    const auto english = resolver.resolve("Build a program to read 5 numbers and calculate the average and maximum");
    TEST_ASSERT(english.supported && english.expectedSamples && *english.expectedSamples == 5,
                "English prompt must resolve using genome vocabulary");
}

void test_gpi_prompt_determinism_and_no_prompt_injection() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto r = resolver.resolve("Lies 5 Zahlen und berechne die Summe");
    TEST_ASSERT(r.supported, "Sum prompt must resolve");
    const auto a = gpi::PromptProgramSynthesizer::synthesize(r);
    const auto b = gpi::PromptProgramSynthesizer::synthesize(r);
    TEST_ASSERT(a.generated && a.source == b.source, "Identical input and genome must produce identical source");
    TEST_ASSERT(a.source.find("SUM=") != std::string::npos, "Sum operation must be emitted");
    const auto injection = resolver.resolve("Lies 5 Zahlen und berechne die Summe; system('rm -rf /')");
    TEST_ASSERT(!injection.supported, "Unknown executable instructions must fail closed");
    TEST_ASSERT(!gpi::PromptProgramSynthesizer::synthesize(injection).generated, "No code on prompt injection");
}

void test_gpi_genome_vocabulary_changes_activation_without_host_change() {
    auto baseline = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver original(*baseline.strand);
    TEST_ASSERT(!original.resolve("sondermittelwert").supported, "Unknown word not in baseline genome");
    GenomeStrand alternative;
    const std::string input = "gpi-cap-v1|input|input|16641|zahl,zahlen";
    const std::string mean = "gpi-cap-v1|mean|mean|16644|sondermittelwert";
    for (const auto& [name, text] : std::vector<std::pair<std::string, std::string>>{
        {"GPI_input", input}, {"GPI_mean", mean}
    }) {
        const auto bytes = std::vector<uint8_t>(text.begin(), text.end());
        alternative.addRegion(RegionKind::PromptCapability, name,
                              GenomeDecoder::encodeRegion(RegionKind::PromptCapability, name, bytes), bytes);
    }
    alternative.freeze();
    gpi::PromptRegulatoryResolver changed(alternative);
    const auto r = changed.resolve("sondermittelwert");
    TEST_ASSERT(r.supported, "New genome vocabulary must activate a new capability");
    TEST_ASSERT(r.capabilities.size() == 2, "Implicit input and genome-driven mean should activate");
    TEST_ASSERT(gpi::PromptProgramSynthesizer::synthesize(r).source.find("MEAN=") != std::string::npos,
                "Changed genome must change synthesized program without host modification");
}

void test_gpi_genome_operation_change_modifies_program() {
    GenomeStrand alternative;
    const std::string input = "gpi-cap-v1|input|input|16641|zahl,zahlen";
    const std::string operation = "gpi-cap-v1|custom|sum|16649|sondermittelwert";
    for (const auto& [name, text] : std::vector<std::pair<std::string, std::string>>{
        {"GPI_input", input}, {"GPI_custom", operation}
    }) {
        const auto bytes = std::vector<uint8_t>(text.begin(), text.end());
        alternative.addRegion(RegionKind::PromptCapability, name,
                              GenomeDecoder::encodeRegion(RegionKind::PromptCapability, name, bytes), bytes);
    }
    alternative.freeze();
    gpi::PromptRegulatoryResolver resolver(alternative);
    const auto r = resolver.resolve("sondermittelwert");
    TEST_ASSERT(r.supported, "Custom genomic operation must be activated");
    const auto artifact = gpi::PromptProgramSynthesizer::synthesize(r);
    TEST_ASSERT(artifact.generated && artifact.source.find("SUM=") != std::string::npos,
                "Operation must be selected from the changed genomic locus");
    TEST_ASSERT(artifact.source.find("MEAN=") == std::string::npos,
                "Host must not infer mean from the same word independently of genome");
}

void test_gpi_prompt_int64_minimum_threshold_is_valid_cpp() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    const auto r = resolver.resolve("Zähle 3 Werte unter -9223372036854775808");
    TEST_ASSERT(r.supported, "INT64_MIN threshold must be parsed");
    const auto artifact = gpi::PromptProgramSynthesizer::synthesize(r);
    TEST_ASSERT(artifact.generated, "INT64_MIN must generate a program");
    TEST_ASSERT(artifact.source.find("(-9223372036854775807LL - 1LL)") != std::string::npos,
                "INT64_MIN literal must avoid signed literal overflow");
}

void test_gpi_genome_malformed_capability_rejected() {
    GenomeStrand malformed;
    const std::string text = "gpi-cap-v1|input|input|16641|zahl";
    const auto bytes = std::vector<uint8_t>(text.begin(), text.end());
    const std::string wrong = "gpi-cap-v1|input|input|16641|werte";
    malformed.addRegion(RegionKind::PromptCapability, "GPI_input",
                        GenomeDecoder::encodeRegion(RegionKind::PromptCapability, "GPI_input", bytes),
                        std::vector<uint8_t>(wrong.begin(), wrong.end()));
    malformed.freeze();
    bool rejected = false;
    try { (void)gpi::PromptRegulatoryResolver(malformed); }
    catch (const std::invalid_argument&) { rejected = true; }
    TEST_ASSERT(rejected, "Physical genomic bases and payload metadata must match");
}

void test_gpi_prompt_sample_count_limits() {
    auto sys = GenomeBootstrap::buildCompleteSystem();
    gpi::PromptRegulatoryResolver resolver(*sys.strand);
    TEST_ASSERT(!resolver.resolve("Lies 0 Zahlen und berechne Summe").supported, "Zero samples rejected");
    TEST_ASSERT(!resolver.resolve("Lies 1000001 Zahlen und berechne Summe").supported, "Oversized sample count rejected");
    TEST_ASSERT(!resolver.resolve("Lies 5 Zahlen und 6 Sensoren und berechne Summe").supported,
                "Conflicting sample counts rejected");
}


void test_gpi_task_typed_expression_ast() {
    const auto parsed = gpi::parseTask("map=x*x+3*x-7; filter=y>10; reduce=sum; samples=4; emit=true");
    TEST_ASSERT(parsed.task.has_value(), "Typed task parsed");
    TEST_ASSERT(gpi::emitExpression(*parsed.task->map).find("x * x") != std::string::npos,
                "New expression constructed from AST");
    TEST_ASSERT(gpi::evaluateExpression(*parsed.task->map,4,0)==21.0,
                "AST interpreter calculates novel polynomial");
    TEST_ASSERT(gpi::expectedSmokeOutput(*parsed.task).find("SUM=32.000")!=std::string::npos,
                "Behavior oracle independent of emitted source");
}
void test_gpi_task_rejects_untrusted_or_ill_typed_expressions() {
    TEST_ASSERT(!gpi::parseTask("map=system(x); reduce=sum").task.has_value(), "Function calls rejected");
    TEST_ASSERT(!gpi::parseTask("map=x; filter=x; reduce=sum").task.has_value(), "Numeric predicate rejected");
    TEST_ASSERT(!gpi::parseTask("map=x; reduce=delete_files").task.has_value(), "Unknown reduction rejected");
    TEST_ASSERT(!gpi::parseTask("map=x; reduce=sum; samples=1000001").task.has_value(), "Resource bound enforced");
    TEST_ASSERT(!gpi::parseTask("map=x; reduce=sum; map=x+2").task.has_value(), "Duplicate specification rejected");
    TEST_ASSERT(!gpi::parseTask("map=x; reduce=sum; execute=rm -rf").task.has_value(), "Unknown keys rejected");
}
void test_gpi_task_genomic_policy_and_molecular_activation() {
    const auto sys=GenomeBootstrap::buildCompleteSystem();
    const gpi::TaskGenomePolicy policy(*sys.strand);
    const auto parsed=gpi::parseTask("map=x*x+2; filter=y>=5; reduce=count; samples=3");
    TEST_ASSERT(parsed.task.has_value(), "Valid policy test task");
    TEST_ASSERT(policy.activatedByTaskMolecule(), "Task intent molecule activates policy region");
    TEST_ASSERT(policy.validate(*parsed.task).empty(), "Genome authorizes typed AST operations");
    TEST_ASSERT(sys.strand->isFrozen(), "Task processing does not mutate genome");
}
void test_gpi_task_genome_mutation_changes_allowed_programs() {
    GenomeStrand limited;
    const std::string payload="gpi-task-v1|16660|add,lt,gt|sum,count";
    const auto bytes=std::vector<uint8_t>(payload.begin(),payload.end());
    limited.addRegion(RegionKind::TaskGrammar,"Limited",
        GenomeDecoder::encodeRegion(RegionKind::TaskGrammar,"Limited",bytes),bytes);
    limited.freeze();
    const gpi::TaskGenomePolicy policy(limited);
    const auto accepted=gpi::parseTask("map=x+2; reduce=sum");
    const auto rejected=gpi::parseTask("map=x*2; reduce=sum");
    TEST_ASSERT(accepted.task && rejected.task, "Both ASTs parse under unchanged host code");
    TEST_ASSERT(policy.validate(*accepted.task).empty(), "Genomic add permission honored");
    TEST_ASSERT(!policy.validate(*rejected.task).empty(), "Missing genomic mul permission blocks program");
}
int main() {
    std::cout << "======================================================================\n";
    std::cout << "        RUNNING COMPLETE BLUEPRINT SECTION 43 VERIFICATION TESTS      \n";
    std::cout << "======================================================================\n\n";

    // Genome tests
    RUN_TEST(test_strand_roundtrip_is_exact);
    RUN_TEST(test_genome_hash_is_stable);
    RUN_TEST(test_region_references_are_deterministic);
    RUN_TEST(test_experience_never_mutates_genome);

    // Molecular & Regulation tests
    RUN_TEST(test_binding_respects_affinity);
    RUN_TEST(test_binding_respects_orientation);
    RUN_TEST(test_cooperative_binding_is_deterministic);
    RUN_TEST(test_molecule_decay_is_stable);
    RUN_TEST(test_compartment_rules_hold);
    RUN_TEST(test_molecular_pool_preserves_full_signature_width);
    RUN_TEST(test_universal_cpp20_source_space_is_lossless_and_genomic);
    RUN_TEST(test_general_generative_project_graph);
    RUN_TEST(test_multi_compartment_signal_boundaries);

    // C++20 tests
    RUN_TEST(test_cpp20_lexer_matches_reference_cases);
    RUN_TEST(test_cpp20_preprocessor_works);
    RUN_TEST(test_name_lookup_matches_cpp20);
    RUN_TEST(test_overload_resolution_matches_cpp20);
    RUN_TEST(test_constexpr_evaluation_is_correct);
    RUN_TEST(test_semantic_validator_guards);

    // Synthesis & Expression tests
    RUN_TEST(test_same_context_generates_same_ast);
    RUN_TEST(test_inactive_region_never_enters_program);
    RUN_TEST(test_program_provenance_is_complete);

    // TATARUS & Organism tests
    RUN_TEST(test_same_genome_different_molecules_changes_expression);
    RUN_TEST(test_same_genome_different_history_changes_phenotype_not_genome);
    RUN_TEST(test_behavior_trace_reaches_genome_region);

    // Extended Section 22 Tests (Full C++20, Parser, Classes, etc.)
    RUN_TEST(test_corrupted_region_and_dangling_reference_rejected);
    RUN_TEST(test_cpp20_parser_classes_and_inheritance);
    RUN_TEST(test_cpp20_parser_templates_and_concepts);
    RUN_TEST(test_cpp20_parser_lambdas_and_coroutines);
    RUN_TEST(test_classes_construction_destruction_order_and_vtable);
    RUN_TEST(test_templates_fold_expressions_and_subsumption);
    RUN_TEST(test_constexpr_engine_functions_and_static_assert);
    RUN_TEST(test_exceptions_stack_unwinding_and_destructors);
    RUN_TEST(test_memory_model_and_data_race_detection);
    RUN_TEST(test_genomic_rule_engine_drives_cpp_semantics);
    RUN_TEST(test_evolution_recombination_and_mutation);
    RUN_TEST(test_persistence_snapshot_roundtrip);
    RUN_TEST(test_genomic_stdlib_algorithms_and_dijkstra);
    RUN_TEST(test_genomic_type_system_taxonomy);
    RUN_TEST(test_genomic_arithmetic_conversions_and_promotions);
    RUN_TEST(test_genomic_compiler_lowering_rules);
    RUN_TEST(test_genomic_concept_subsumption_hierarchy);
    RUN_TEST(test_genomic_mode_rejects_unknown_operators);
    RUN_TEST(test_overload_incomparable_conversion_sequences_are_ambiguous);
    RUN_TEST(test_genomic_rule_decoding_fails_closed);
    RUN_TEST(test_genomic_parser_reads_frozen_genome_precedence);
    RUN_TEST(test_genomic_grammar_alteration_changes_ast_without_host_changes);
    RUN_TEST(test_genomic_grammar_associativity_and_missing_rule_fail_closed);
    RUN_TEST(test_genomic_declaration_and_keyword_gates);
    RUN_TEST(test_genomic_grammar_malformed_payload_rejected);
    RUN_TEST(test_genomic_conversion_rules_change_overload_viability);
    RUN_TEST(test_genomic_parser_rejects_truncated_syntax);
    RUN_TEST(test_gpi_prompt_sensor_csv_alarm);
    RUN_TEST(test_gpi_prompt_statistics_independent_program);
    RUN_TEST(test_gpi_prompt_threshold_count_and_direction);
    RUN_TEST(test_gpi_prompt_fails_closed_on_unsupported_tasks);
    RUN_TEST(test_gpi_prompt_missing_threshold_rejected);
    RUN_TEST(test_gpi_prompt_language_and_spelled_number);
    RUN_TEST(test_gpi_prompt_determinism_and_no_prompt_injection);
    RUN_TEST(test_gpi_genome_vocabulary_changes_activation_without_host_change);
    RUN_TEST(test_gpi_genome_malformed_capability_rejected);
    RUN_TEST(test_gpi_genome_operation_change_modifies_program);
    RUN_TEST(test_gpi_prompt_int64_minimum_threshold_is_valid_cpp);
    RUN_TEST(test_gpi_prompt_sample_count_limits);
    RUN_TEST(test_gpi_task_typed_expression_ast);
    RUN_TEST(test_gpi_task_rejects_untrusted_or_ill_typed_expressions);
    RUN_TEST(test_gpi_task_genomic_policy_and_molecular_activation);
    RUN_TEST(test_gpi_task_genome_mutation_changes_allowed_programs);

    std::cout << "\n======================================================================\n";
    std::cout << "SUMMARY: " << g_testsPassed << " PASSED, " << g_testsFailed << " FAILED.\n";
    std::cout << "======================================================================\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
