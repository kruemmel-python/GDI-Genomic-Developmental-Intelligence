#include "genome/genome_bootstrap.hpp"
#include "genome/genome_decoder.hpp"
#include "cpp20/genomic_expression_grammar.hpp"
#include "regulation/molecule.hpp"
#include <sstream>

namespace genomic {

BootstrappedSystem GenomeBootstrap::buildCompleteSystem() {
    BootstrappedSystem sys;
    sys.strand = std::make_shared<GenomeStrand>();
    sys.resolver = std::make_shared<RegulatoryResolver>();
    sys.spliceGraph = std::make_shared<SpliceGraph>();
    sys.phenotypeBuilder = std::make_shared<PhenotypeBuilder>();

    // -------------------------------------------------------------
    // 1. Region 0001: Boot Dictionary
    // -------------------------------------------------------------
    std::string bootDict = "GISA_VERSION=2.0;ALPHABET=ACGT;CODONS=64;ENDIAN=LITTLE;";
    auto bootBases = GenomeDecoder::encodeRegion(RegionKind::BootDictionary, "BootDictionary",
        std::vector<uint8_t>(bootDict.begin(), bootDict.end()));
    GenomeRegionId bootId = sys.strand->addRegion(RegionKind::BootDictionary, "BootDictionary", bootBases,
        std::vector<uint8_t>(bootDict.begin(), bootDict.end()), {}, "System bootstrap dictionary and GISA instruction definitions");

    // -------------------------------------------------------------
    // 2. Region 0002: Symbol Table
    // -------------------------------------------------------------
    std::string symbols = "sym:void=0;sym:bool=1;sym:char=2;sym:int8_t=3;sym:int16_t=4;sym:int32_t=5;"
                          "sym:int64_t=6;sym:uint8_t=7;sym:uint16_t=8;sym:uint32_t=9;sym:uint64_t=10;"
                          "sym:float=11;sym:double=12;sym:pointer=13;sym:emitMolecule=14;";
    auto symBases = GenomeDecoder::encodeRegion(RegionKind::SymbolTable, "SymbolTable",
        std::vector<uint8_t>(symbols.begin(), symbols.end()));
    GenomeRegionId symId = sys.strand->addRegion(RegionKind::SymbolTable, "SymbolTable", symBases,
        std::vector<uint8_t>(symbols.begin(), symbols.end()), {bootId}, "Core symbols and type indices");

    // -------------------------------------------------------------
    // 3. Region 0100: C++ Grammar & Lexicon (Full productions)
    // -------------------------------------------------------------
    std::string grammar = "grammar:translation_unit=declaration*;"
                          "grammar:declaration=function_decl|variable_decl|class_decl|enum_decl|concept_decl|template_decl|namespace_decl|module_decl|import_decl|export_decl;"
                          "grammar:statement=compound_stmt|if_stmt|for_stmt|while_stmt|do_stmt|return_stmt|try_stmt|coroutine_stmt|break_stmt|continue_stmt|expression_stmt;"
                          "grammar:expression=assignment_expr|binary_expr|unary_expr|call_expr|literal_expr|identifier_expr|lambda_expr;"
                          "grammar:keywords=alignas,alignof,and,and_eq,asm,auto,bitand,bitor,bool,break,case,catch,char,class,co_await,co_return,co_yield,compl,concept,const,const_cast,consteval,constexpr,constinit,continue,decltype,default,delete,do,double,dynamic_cast,else,enum,explicit,export,extern,false,float,for,friend,goto,if,import,inline,int,long,module,mutable,namespace,new,noexcept,not,not_eq,nullptr,operator,or,or_eq,private,protected,public,register,reinterpret_cast,requires,return,short,signed,sizeof,static,static_assert,static_cast,struct,switch,template,this,thread_local,throw,true,try,typedef,typeid,typename,union,unsigned,using,virtual,void,volatile,wchar_t,while,xor,xor_eq;";
    grammar += GenomicExpressionGrammar::standardRulesPayload();
    auto gramBases = GenomeDecoder::encodeRegion(RegionKind::CppGrammar, "Cpp20Grammar",
        std::vector<uint8_t>(grammar.begin(), grammar.end()));
    GenomeRegionId gramId = sys.strand->addRegion(RegionKind::CppGrammar, "Cpp20Grammar", gramBases,
        std::vector<uint8_t>(grammar.begin(), grammar.end()), {symId}, "C++20 lexical tokens, keywords, and grammar production rules");

    const std::string sourceSpace="format=cpp20-source-space-v1;encoding=all-octets;"
        "unit=relative-path+exact-source;integrity=fnv1a64;provenance=genome-hash+region-id;"
        "edit-operators=token-integer-delta,empirical-numeric-calibration,insert-standard-header,infer-link-provider,coerce-numeric-string-literal,local-integer-neighborhood;"
        "header-symbols=cout@iostream,cin@iostream,cerr@iostream,vector@vector,string@string,sort@algorithm,"
        "unordered_map@unordered_map,unordered_set@unordered_set,priority_queue@queue,setprecision@iomanip,"
        "isfinite@cmath,fmod@cmath,optional@optional,tuple@tuple;"
        "semantic-oracle=external-cpp20-compiler;";
    const std::vector<uint8_t> sourceSpaceBytes(sourceSpace.begin(),sourceSpace.end());
    const auto sourceSpaceBases=GenomeDecoder::encodeRegion(RegionKind::CppGenerativeRule,
        "Cpp20UniversalSourceSpace",sourceSpaceBytes);
    const GenomeRegionId sourceSpaceId=sys.strand->addRegion(RegionKind::CppGenerativeRule,
        "Cpp20UniversalSourceSpace",sourceSpaceBases,sourceSpaceBytes,{gramId},
        "Lossless generative representation for every possible C++20 source translation unit");

    // -------------------------------------------------------------
    // 4. Region 0200: Type System (Full taxonomy with sizes, alignments, and attributes)
    // -------------------------------------------------------------
    std::string typeSys = "type:void:0:0:0:0:0:0:0;"
                          "type:bool:1:1:1:0:0:0:0;"
                          "type:char:1:1:1:0:1:0:0;"
                          "type:char8_t:1:1:1:0:0:0:0;"
                          "type:char16_t:2:2:1:0:0:0:0;"
                          "type:char32_t:4:4:1:0:0:0:0;"
                          "type:wchar_t:2:2:1:0:0:0:0;"
                          "type:int8_t:1:1:1:0:1:0:0;"
                          "type:int16_t:2:2:1:0:1:0:0;"
                          "type:int32_t:4:4:1:0:1:0:0;"
                          "type:int64_t:8:8:1:0:1:0:0;"
                          "type:uint8_t:1:1:1:0:0:0:0;"
                          "type:uint16_t:2:2:1:0:0:0:0;"
                          "type:uint32_t:4:4:1:0:0:0:0;"
                          "type:uint64_t:8:8:1:0:0:0:0;"
                          "type:float:4:4:0:1:1:0:0;"
                          "type:double:8:8:0:1:1:0:0;"
                          "type:pointer:8:8:0:0:0:1:0;"
                          "type:lvalue_ref:8:8:0:0:0:0:1;"
                          "type:rvalue_ref:8:8:0:0:0:0:1;"
                          "type:record:8:8:0:0:0:0:0;";
    auto typeBases = GenomeDecoder::encodeRegion(RegionKind::TypeSystem, "TypeSystem",
        std::vector<uint8_t>(typeSys.begin(), typeSys.end()));
    GenomeRegionId typeId = sys.strand->addRegion(RegionKind::TypeSystem, "TypeSystem", typeBases,
        std::vector<uint8_t>(typeSys.begin(), typeSys.end()), {symId, gramId, sourceSpaceId}, "Implemented C++20 type taxonomy, sizes, and alignments");

    // -------------------------------------------------------------
    // 5. Region 0202: C++ Conversion Rules (Promotions, Widening, UAC)
    // -------------------------------------------------------------
    std::string convRules = "conv:char:int32_t:1:1:integral_promotion;"
                            "conv:int8_t:int32_t:1:1:integral_promotion;"
                            "conv:int16_t:int32_t:1:1:integral_promotion;"
                            "conv:uint8_t:int32_t:1:1:integral_promotion;"
                            "conv:uint16_t:int32_t:1:1:integral_promotion;"
                            "conv:bool:int32_t:1:1:integral_promotion;"
                            "conv:float:double:1:1:floating_promotion;"
                            "conv:int32_t:int64_t:2:1:integral_conversion;"
                            "conv:uint32_t:uint64_t:2:1:integral_conversion;"
                            "conv:int32_t:uint64_t:2:1:signed_unsigned_conversion;"
                            "conv:int32_t:double:2:1:integer_to_floating;"
                            "conv:int64_t:double:2:1:integer_to_floating;"
                            "conv:uint32_t:double:2:1:integer_to_floating;"
                            "conv:uint64_t:double:2:1:integer_to_floating;"
                            "conv:int32_t:float:2:1:integer_to_floating;"
                            "conv:int64_t:float:2:1:integer_to_floating;"
                            "conv:array:pointer:1:1:pointer_decay;"
                            "conv:function:pointer:1:1:pointer_decay;"
                            "uac:double:double:double;"
                            "uac:double:float:double;"
                            "uac:double:int64_t:double;"
                            "uac:double:int32_t:double;"
                            "uac:double:uint64_t:double;"
                            "uac:double:uint32_t:double;"
                            "uac:float:float:float;"
                            "uac:float:int64_t:float;"
                            "uac:float:int32_t:float;"
                            "uac:uint64_t:uint64_t:uint64_t;"
                            "uac:uint64_t:int64_t:uint64_t;"
                            "uac:uint64_t:int32_t:uint64_t;"
                            "uac:int64_t:int64_t:int64_t;"
                            "uac:int64_t:int32_t:int64_t;"
                            "uac:int64_t:int16_t:int64_t;"
                            "uac:int64_t:int8_t:int64_t;"
                            "uac:uint32_t:uint32_t:uint32_t;"
                            "uac:uint32_t:int32_t:uint32_t;"
                            "uac:int32_t:int32_t:int32_t;"
                            "uac:int32_t:int16_t:int32_t;"
                            "uac:int32_t:int8_t:int32_t;"
                            "uac:int32_t:bool:int32_t;";
    auto convBases = GenomeDecoder::encodeRegion(RegionKind::CppConversionRule, "CppConversionRules",
        std::vector<uint8_t>(convRules.begin(), convRules.end()));
    GenomeRegionId convId = sys.strand->addRegion(RegionKind::CppConversionRule, "CppConversionRules", convBases,
        std::vector<uint8_t>(convRules.begin(), convRules.end()), {typeId}, "Genomic numeric conversions, promotions, and usual arithmetic conversions");

    // -------------------------------------------------------------
    // 6. Region 0302: C++ Name Lookup Rules
    // -------------------------------------------------------------
    std::string lookupRules = "lookup:scopes=local,block,function,class,base_class,namespace,global,adl:shadow=1:twophase=1;";
    auto lookupBases = GenomeDecoder::encodeRegion(RegionKind::CppLookupRule, "CppLookupRules",
        std::vector<uint8_t>(lookupRules.begin(), lookupRules.end()));
    GenomeRegionId lookupId = sys.strand->addRegion(RegionKind::CppLookupRule, "CppLookupRules", lookupBases,
        std::vector<uint8_t>(lookupRules.begin(), lookupRules.end()), {convId}, "Scope search order and two-phase lookup rules");

    // -------------------------------------------------------------
    // 7. Region 0303: C++ Overload Resolution Rules
    // -------------------------------------------------------------
    std::string ovlRules = "overload:exact=0:prom=1:conv=2:user=3:prefer_nontemplate=1:prefer_constrained=1;";
    auto ovlBases = GenomeDecoder::encodeRegion(RegionKind::CppOverloadRule, "CppOverloadRules",
        std::vector<uint8_t>(ovlRules.begin(), ovlRules.end()));
    GenomeRegionId ovlId = sys.strand->addRegion(RegionKind::CppOverloadRule, "CppOverloadRules", ovlBases,
        std::vector<uint8_t>(ovlRules.begin(), ovlRules.end()), {lookupId}, "Overload ranking, standard conversion scoring, and constraint priority");

    // -------------------------------------------------------------
    // 8. Region 0301: C++ Semantic Rules (Complete Operator Table)
    // -------------------------------------------------------------
    std::string semRules =
        // int64_t binary ops
        "binop:+:int64_t:int64_t:int64_t:PRValue:Add:1;"
        "binop:-:int64_t:int64_t:int64_t:PRValue:Sub:1;"
        "binop:*:int64_t:int64_t:int64_t:PRValue:Mul:1;"
        "binop:/:int64_t:int64_t:int64_t:PRValue:Div:1;"
        "binop:%:int64_t:int64_t:int64_t:PRValue:Mod:1;"
        "binop:&:int64_t:int64_t:int64_t:PRValue:BitAnd:1;"
        "binop:|:int64_t:int64_t:int64_t:PRValue:BitOr:1;"
        "binop:^:int64_t:int64_t:int64_t:PRValue:BitXor:1;"
        "binop:<<:int64_t:int64_t:int64_t:PRValue:BitShl:1;"
        "binop:>>:int64_t:int64_t:int64_t:PRValue:BitShr:1;"
        "binop:==:int64_t:int64_t:bool:PRValue:CmpEQ:1;"
        "binop:!=:int64_t:int64_t:bool:PRValue:CmpNE:1;"
        "binop:<:int64_t:int64_t:bool:PRValue:CmpLT:1;"
        "binop:<=:int64_t:int64_t:bool:PRValue:CmpLE:1;"
        "binop:>:int64_t:int64_t:bool:PRValue:CmpGT:1;"
        "binop:>=:int64_t:int64_t:bool:PRValue:CmpGE:1;"
        "binop:<=>:int64_t:int64_t:int8_t:PRValue:CmpEQ:1;"
        "binop:=:int64_t:int64_t:int64_t:LValue:Add:1;"
        "binop:+=:int64_t:int64_t:int64_t:LValue:Add:1;"
        "binop:-=:int64_t:int64_t:int64_t:LValue:Sub:1;"
        "binop:*=:int64_t:int64_t:int64_t:LValue:Mul:1;"
        "binop:/=:int64_t:int64_t:int64_t:LValue:Div:1;"

        // int32_t binary ops
        "binop:+:int32_t:int32_t:int32_t:PRValue:Add:1;"
        "binop:-:int32_t:int32_t:int32_t:PRValue:Sub:1;"
        "binop:*:int32_t:int32_t:int32_t:PRValue:Mul:1;"
        "binop:/:int32_t:int32_t:int32_t:PRValue:Div:1;"
        "binop:%:int32_t:int32_t:int32_t:PRValue:Mod:1;"
        "binop:&:int32_t:int32_t:int32_t:PRValue:BitAnd:1;"
        "binop:|:int32_t:int32_t:int32_t:PRValue:BitOr:1;"
        "binop:^:int32_t:int32_t:int32_t:PRValue:BitXor:1;"
        "binop:<<:int32_t:int32_t:int32_t:PRValue:BitShl:1;"
        "binop:>>:int32_t:int32_t:int32_t:PRValue:BitShr:1;"
        "binop:==:int32_t:int32_t:bool:PRValue:CmpEQ:1;"
        "binop:!=:int32_t:int32_t:bool:PRValue:CmpNE:1;"
        "binop:<:int32_t:int32_t:bool:PRValue:CmpLT:1;"
        "binop:<=:int32_t:int32_t:bool:PRValue:CmpLE:1;"
        "binop:>:int32_t:int32_t:bool:PRValue:CmpGT:1;"
        "binop:>=:int32_t:int32_t:bool:PRValue:CmpGE:1;"
        "binop:=:int32_t:int32_t:int32_t:LValue:Add:1;"

        // uint64_t binary ops
        "binop:+:uint64_t:uint64_t:uint64_t:PRValue:Add:1;"
        "binop:-:uint64_t:uint64_t:uint64_t:PRValue:Sub:1;"
        "binop:*:uint64_t:uint64_t:uint64_t:PRValue:Mul:1;"
        "binop:/:uint64_t:uint64_t:uint64_t:PRValue:Div:1;"
        "binop:==:uint64_t:uint64_t:bool:PRValue:CmpEQ:1;"
        "binop:!=:uint64_t:uint64_t:bool:PRValue:CmpNE:1;"
        "binop:<:uint64_t:uint64_t:bool:PRValue:CmpLT:1;"
        "binop:=:uint64_t:uint64_t:uint64_t:LValue:Add:1;"

        // double binary ops
        "binop:+:double:double:double:PRValue:Add:1;"
        "binop:-:double:double:double:PRValue:Sub:1;"
        "binop:*:double:double:double:PRValue:Mul:1;"
        "binop:/:double:double:double:PRValue:Div:1;"
        "binop:==:double:double:bool:PRValue:CmpEQ:1;"
        "binop:!=:double:double:bool:PRValue:CmpNE:1;"
        "binop:<:double:double:bool:PRValue:CmpLT:1;"
        "binop:<=:double:double:bool:PRValue:CmpLE:1;"
        "binop:>:double:double:bool:PRValue:CmpGT:1;"
        "binop:>=:double:double:bool:PRValue:CmpGE:1;"
        "binop:=:double:double:double:LValue:Add:1;"

        // float binary ops
        "binop:+:float:float:float:PRValue:Add:1;"
        "binop:-:float:float:float:PRValue:Sub:1;"
        "binop:*:float:float:float:PRValue:Mul:1;"
        "binop:/:float:float:float:PRValue:Div:1;"
        "binop:==:float:float:bool:PRValue:CmpEQ:1;"
        "binop:!=:float:float:bool:PRValue:CmpNE:1;"
        "binop:=:float:float:float:LValue:Add:1;"

        // bool binary ops
        "binop:&&:bool:bool:bool:PRValue:BitAnd:1;"
        "binop:||:bool:bool:bool:PRValue:BitOr:1;"
        "binop:==:bool:bool:bool:PRValue:CmpEQ:1;"
        "binop:!=:bool:bool:bool:PRValue:CmpNE:1;"
        "binop:=:bool:bool:bool:LValue:Add:1;"

        // Unary operations
        "unop:+:int64_t:int64_t:1:PRValue:Nop:1;"
        "unop:-:int64_t:int64_t:1:PRValue:Sub:1;"
        "unop:++:int64_t:int64_t:1:LValue:Add:1;"
        "unop:--:int64_t:int64_t:1:LValue:Sub:1;"
        "unop:~:int64_t:int64_t:1:PRValue:BitXor:1;"
        "unop:+:int32_t:int32_t:1:PRValue:Nop:1;"
        "unop:-:int32_t:int32_t:1:PRValue:Sub:1;"
        "unop:++:int32_t:int32_t:1:LValue:Add:1;"
        "unop:--:int32_t:int32_t:1:LValue:Sub:1;"
        "unop:~:int32_t:int32_t:1:PRValue:BitXor:1;"
        "unop:!:bool:bool:1:PRValue:CmpEQ:1;";

    auto semBases = GenomeDecoder::encodeRegion(RegionKind::CppSemanticRule, "CppSemanticRules",
        std::vector<uint8_t>(semRules.begin(), semRules.end()));
    GenomeRegionId semId = sys.strand->addRegion(RegionKind::CppSemanticRule, "CppSemanticRules", semBases,
        std::vector<uint8_t>(semRules.begin(), semRules.end()), {ovlId}, "Executable semantic operator rules and lowering targets");

    // -------------------------------------------------------------
    // 9. Region 0305: C++ Constraint & Concept Rules
    // -------------------------------------------------------------
    std::string conceptRules =
        "concept:same_as:void,bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double,pointer,record:;"
        "concept:integral:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t:;"
        "concept:signed_integral:char,int8_t,int16_t,int32_t,int64_t:integral;"
        "concept:unsigned_integral:uint8_t,uint16_t,uint32_t,uint64_t:integral;"
        "concept:floating_point:float,double:;"
        "concept:arithmetic:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double:integral,floating_point;"
        "concept:destructible:void,bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double,pointer,record:;"
        "concept:default_initializable:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double,pointer:;"
        "concept:equality_comparable:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double,pointer:;"
        "concept:totally_ordered:char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double:equality_comparable;"
        "concept:Integral:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t:;"
        "concept:SignedIntegral:char,int8_t,int16_t,int32_t,int64_t:Integral;"
        "concept:FloatingPoint:float,double:;"
        "concept:Arithmetic:bool,char,int8_t,int16_t,int32_t,int64_t,uint8_t,uint16_t,uint32_t,uint64_t,float,double:Integral,FloatingPoint;";

    auto conceptBases = GenomeDecoder::encodeRegion(RegionKind::CppConstraintRule, "CppConstraintRules",
        std::vector<uint8_t>(conceptRules.begin(), conceptRules.end()));
    GenomeRegionId conceptId = sys.strand->addRegion(RegionKind::CppConstraintRule, "CppConstraintRules", conceptBases,
        std::vector<uint8_t>(conceptRules.begin(), conceptRules.end()), {semId}, "Genomic concept definitions and constraint satisfaction sets");

    // -------------------------------------------------------------
    // 10. Region 0306: C++ Lifetime & Memory Safety Rules
    // -------------------------------------------------------------
    std::string lifetimeRules = "lifetime:raii=1:temp_lifetime=1:uaf_guard=1:prvalue_materialize=1;";
    auto lifeBases = GenomeDecoder::encodeRegion(RegionKind::CppLifetimeRule, "CppLifetimeRules",
        std::vector<uint8_t>(lifetimeRules.begin(), lifetimeRules.end()));
    GenomeRegionId lifeId = sys.strand->addRegion(RegionKind::CppLifetimeRule, "CppLifetimeRules", lifeBases,
        std::vector<uint8_t>(lifetimeRules.begin(), lifetimeRules.end()), {conceptId}, "Lifetime rules, RAII destructor enforcement, and temporary extension");

    // -------------------------------------------------------------
    // 11. Region 0400 & 0401: Compiler Logic & Explicit Lowering Rules
    // -------------------------------------------------------------
    std::string compLogic = "compiler:stage0_decoder,stage1_ir_builder,stage2_ssa_optimizer,stage3_target_codegen;"
                            "lowering:binop:+:Add;"
                            "lowering:binop:-:Sub;"
                            "lowering:binop:*:Mul;"
                            "lowering:binop:/:Div;"
                            "lowering:binop:%:Mod;"
                            "lowering:binop:&:BitAnd;"
                            "lowering:binop:|:BitOr;"
                            "lowering:binop:^:BitXor;"
                            "lowering:binop:<<:BitShl;"
                            "lowering:binop:>>:BitShr;"
                            "lowering:binop:==:CmpEQ;"
                            "lowering:binop:!=:CmpNE;"
                            "lowering:binop:<:CmpLT;"
                            "lowering:binop:<=:CmpLE;"
                            "lowering:binop:>:CmpGT;"
                            "lowering:binop:>=:CmpGE;"
                            "lowering:stmt:return:Return;"
                            "lowering:stmt:branch:Branch;"
                            "lowering:stmt:branch_cond:BranchCond;"
                            "lowering:stmt:coroutine_suspend:Suspend;"
                            "lowering:expr:call:Call;"
                            "lowering:expr:emit_molecule:EmitMolecule;"
                            "lowering:stmt:halt:Halt;";
    auto compBases = GenomeDecoder::encodeRegion(RegionKind::CompilerLogic, "CompilerLogic",
        std::vector<uint8_t>(compLogic.begin(), compLogic.end()));
    GenomeRegionId compId = sys.strand->addRegion(RegionKind::CompilerLogic, "CompilerLogic", compBases,
        std::vector<uint8_t>(compLogic.begin(), compLogic.end()), {lifeId}, "Genomic C++20 compiler pipeline specifications and lowering rules");

    // -------------------------------------------------------------
    // 12. Region 0307: C++ Memory Model & Concurrency Rules
    // -------------------------------------------------------------
    std::string memModel = "memmodel:race_detector=1:default_order=seq_cst;";
    auto memBases = GenomeDecoder::encodeRegion(RegionKind::CppMemoryModelRule, "CppMemoryModelRules",
        std::vector<uint8_t>(memModel.begin(), memModel.end()));
    GenomeRegionId memId = sys.strand->addRegion(RegionKind::CppMemoryModelRule, "CppMemoryModelRules", memBases,
        std::vector<uint8_t>(memModel.begin(), memModel.end()), {compId}, "C++20 memory consistency orders and data race guards");

    // -------------------------------------------------------------
    // 13. Region 0500: Runtime Library
    // -------------------------------------------------------------
    std::string runtimeLib = "runtime:substrate_kernel,capability_guard,molecule_bridge,deterministic_prng;";
    auto rtBases = GenomeDecoder::encodeRegion(RegionKind::RuntimeLibrary, "RuntimeLibrary",
        std::vector<uint8_t>(runtimeLib.begin(), runtimeLib.end()));
    GenomeRegionId rtId = sys.strand->addRegion(RegionKind::RuntimeLibrary, "RuntimeLibrary", rtBases,
        std::vector<uint8_t>(runtimeLib.begin(), runtimeLib.end()), {memId}, "Runtime support primitives and capability sandboxing");

    // -------------------------------------------------------------
    // 14. Region 1000: Regulatory Network Definition
    // -------------------------------------------------------------
    std::string regNet = "regulation:enhancers,repressors,cooperative_binding,affinity_models,decay_diffusion;";
    auto regBases = GenomeDecoder::encodeRegion(RegionKind::RegulatoryNetwork, "RegulatoryNetwork",
        std::vector<uint8_t>(regNet.begin(), regNet.end()));
    GenomeRegionId regNetId = sys.strand->addRegion(RegionKind::RegulatoryNetwork, "RegulatoryNetwork", regBases,
        std::vector<uint8_t>(regNet.begin(), regNet.end()), {rtId}, "Regulatory definitions and enhancer networks");

    // -------------------------------------------------------------
    // 15. Region 2000: Developmental Programs
    // -------------------------------------------------------------
    std::string devPrograms = "development:ontogeny,lineages,stages_neonatal_juvenile_mature,tissue_contexts;";
    auto devBases = GenomeDecoder::encodeRegion(RegionKind::DevelopmentalPrograms, "DevelopmentalPrograms",
        std::vector<uint8_t>(devPrograms.begin(), devPrograms.end()));
    GenomeRegionId devId = sys.strand->addRegion(RegionKind::DevelopmentalPrograms, "DevelopmentalPrograms", devBases,
        std::vector<uint8_t>(devPrograms.begin(), devPrograms.end()), {regNetId}, "Developmental progression constraints and organism ontogeny");

    // -------------------------------------------------------------
    // 16. Region 3001-3005: Functional Navigation Locus Exons
    // -------------------------------------------------------------
    auto regA_bases = GenomeDecoder::encodeString("EXON_A_POSITION_LOCUS");
    GenomeRegionId regA_Id = sys.strand->addRegion(RegionKind::DomainModules, "Exon_A_Position", regA_bases, {}, {devId}, "Exon A: Spatial position and origin tracking");

    auto regB_bases = GenomeDecoder::encodeString("EXON_B_OBSTACLE_DETECTION_LOCUS");
    GenomeRegionId regB_Id = sys.strand->addRegion(RegionKind::DomainModules, "Exon_B_Obstacle", regB_bases, {}, {regA_Id}, "Exon B: Obstacle detection and proximity radar");

    auto regC_bases = GenomeDecoder::encodeString("EXON_C_SHORTEST_PATH_LOCUS");
    GenomeRegionId regC_Id = sys.strand->addRegion(RegionKind::DomainModules, "Exon_C_ShortestPath", regC_bases, {}, {regB_Id}, "Exon C: Direct evasive shortest path calculation");

    auto regD_bases = GenomeDecoder::encodeString("EXON_D_ENERGY_COST_LOCUS");
    GenomeRegionId regD_Id = sys.strand->addRegion(RegionKind::DomainModules, "Exon_D_EnergyCost", regD_bases, {}, {regB_Id}, "Exon D: Energy optimization and speed throttling");

    auto regE_bases = GenomeDecoder::encodeString("EXON_E_THREAT_AVOIDANCE_LOCUS");
    GenomeRegionId regE_Id = sys.strand->addRegion(RegionKind::DomainModules, "Exon_E_ThreatAvoidance", regE_bases, {}, {regD_Id}, "Exon E: Threat avoidance and molecular recovery emission");

    // -------------------------------------------------------------
    // GPI capability loci: prompt vocabulary and regulatory signatures.
    // Each locus is encoded as physical A/C/G/T bases and can be replaced
    // in a new genome without changing the prompt interpreter host code.
    // Grammar: gpi-cap-v1|id|operation|signature|comma-separated triggers
    // -------------------------------------------------------------
    const std::vector<std::string> gpiCapabilities = {
        "gpi-cap-v1|input|input|16641|sensor,sensoren,temperatursensoren,temperatursensor,temperatur,temperaturen,temperaturueberwachung,messwert,messwerte,messwerten,wert,werte,werten,zahl,zahlen,einlesen,einliest,lese,lies,liest,ueberwache,ueberwachung,monitor,monitoring,read,input,sample,samples,number,numbers,value,values,readings,sensors,temperature,sensorwerte",
        "gpi-cap-v1|csv|csv|16642|csv,speichere,speichern,speichert,gespeichert,datei,exportiere,export,save,store,storage,protokolliere,protokoll,aufzeichnen,aufzeichnung",
        "gpi-cap-v1|alarm|alarm|16643|alarm,alarme,alarmiere,alarmsignal,warnung,warne,warnt,alert,warning,critical,kritisch,kritische,ausloesen,loese",
        "gpi-cap-v1|mean|mean|16644|durchschnitt,mittelwert,durchschnittswert,average,mean,arithmetisch,gemittelt,berechne",
        "gpi-cap-v1|max|max|16645|maximum,max,maximalwert,groessten,groesste,hoechsten,hoechste,largest,highest",
        "gpi-cap-v1|min|min|16646|minimum,min,minimalwert,kleinsten,kleinste,niedrigsten,lowest,smallest",
        "gpi-cap-v1|sum|sum|16647|summe,summiere,summieren,sum,gesamtwert,gesamt",
        "gpi-cap-v1|count|count|16648|zaehle,zaehlen,anzahl,count,counts,howmany,zaehlt"
    };
    for (const auto& capability : gpiCapabilities) {
        const auto splitAt = capability.find('|', 11);
        const std::string id = capability.substr(11, splitAt - 11);
        const std::string regionName = "GPI_" + id;
        const auto bytes = std::vector<uint8_t>(capability.begin(), capability.end());
        const auto bases = GenomeDecoder::encodeRegion(RegionKind::PromptCapability, regionName, bytes);
        sys.strand->addRegion(RegionKind::PromptCapability, regionName, bases, bytes,
                              {regNetId, gramId}, "Genomically encoded prompt capability and molecular binding signature");
    }

    // Genomic policy for the general expression/task planner. This is a
    // whitelist of allowed AST operations, not an encoded full C++ compiler.
    const std::string taskRules =
        "gpi-task-v1|16660|add,sub,mul,div,mod,lt,gt,le,ge,eq,ne,and,or,neg,pos,not|sum,mean,min,max,count";
    const auto taskBytes = std::vector<uint8_t>(taskRules.begin(),taskRules.end());
    sys.strand->addRegion(RegionKind::TaskGrammar, "GPI_TypedTaskGrammar",
                          GenomeDecoder::encodeRegion(RegionKind::TaskGrammar,"GPI_TypedTaskGrammar",taskBytes),
                          taskBytes,{regNetId,gramId},"Typed expression/reduction policy activated by task-intent molecule");

    // Research-development loci. The host may compose only strategies whose
    // loci are expressed by the regulatory state; experience changes molecule
    // concentrations, never this immutable sequence.
    const std::vector<std::string> researchCapabilities = {
        "gpi-research-v1|sort|vector+insertion_sort|16701|basal",
        "gpi-research-v1|sort|vector+introsort|16702|developed",
        "gpi-research-v1|unique|vector+sort_unique|16703|basal",
        "gpi-research-v1|unique|unordered_set+sorted_projection|16704|developed",
        "gpi-research-v1|median|vector+full_sort|16705|basal",
        "gpi-research-v1|median|two_heaps+streaming_balance|16706|developed",
        "gpi-research-v1|frequency|map+ordered_count|16707|basal",
        "gpi-research-v1|frequency|unordered_map+sorted_keys|16708|developed",
        "gpi-research-v1|composite|vector+component_pipeline|16709|basal",
        "gpi-research-v1|composite|hash+component_pipeline|16710|developed",
        "gpi-operation-v1|filter_gt|16801|NumberStream|FilteredSequence",
        "gpi-operation-v1|deduplicate|16802|NumberSequence|UniqueSequence",
        "gpi-operation-v1|sort_ascending|16803|NumberSequence|OrderedSequence",
        "gpi-operation-v1|median|16804|NumberSequence|Scalar",
        "gpi-operation-v1|sum|16805|NumberSequence|Scalar",
        "gpi-operation-v1|frequency|16806|NumberSequence|FrequencyTable"
    };
    for (std::size_t i=0;i<researchCapabilities.size();++i) {
        const auto& capability=researchCapabilities[i];
        const auto bytes=std::vector<uint8_t>(capability.begin(),capability.end());
        const auto name="GPI_Research_"+std::to_string(i+1);
        sys.strand->addRegion(RegionKind::ResearchCapability,name,
            GenomeDecoder::encodeRegion(RegionKind::ResearchCapability,name,bytes),bytes,
            {regNetId,typeId,semId,devId},"Genomic research strategy and developmental activation condition");
    }

    // Freeze Genome Strand: Strictly immutable from now on!
    sys.strand->freeze();

    // Initialize Genomic Rule Engine from the frozen genome strand
    sys.ruleEngine = std::make_shared<GenomicRuleEngine>();
    sys.ruleEngine->loadRulesFromGenome(*sys.strand);

    // -------------------------------------------------------------
    // Wire Regulatory Resolver with Enhancers & Repressors
    // -------------------------------------------------------------
    // Enhancer E-101: Basal Position & Obstacle (Activated by basal NutrientRich or default presence)
    GenomeEnhancer enhBasal;
    enhBasal.id = 101;
    enhBasal.targetRegion = regA_Id;
    enhBasal.name = "Basal_Navigation_Enhancer";
    enhBasal.activationThreshold = 0.1;
    enhBasal.cooperativity = 1.0;
    enhBasal.sites.push_back(GenomeBindingSite{MoleculeSignatures::NutrientRich, 0.05, 1.0, 0.0, 0, false, true});
    enhBasal.sites.push_back(GenomeBindingSite{MoleculeSignatures::ThreatDetected, 0.05, 1.0, 0.0, 2, false, true});
    enhBasal.sites.push_back(GenomeBindingSite{MoleculeSignatures::EnergyLow, 0.05, 1.0, 0.0, 4, false, true});
    sys.resolver->addEnhancer(enhBasal);

    // Enhancer E-102: Obstacle Detection Enhancer
    GenomeEnhancer enhObstacle;
    enhObstacle.id = 102;
    enhObstacle.targetRegion = regB_Id;
    enhObstacle.name = "Obstacle_Radar_Enhancer";
    enhObstacle.activationThreshold = 0.1;
    enhObstacle.cooperativity = 1.0;
    enhObstacle.sites.push_back(GenomeBindingSite{MoleculeSignatures::NutrientRich, 0.05, 1.0, 0.0, 0, false, true});
    enhObstacle.sites.push_back(GenomeBindingSite{MoleculeSignatures::EnergyLow, 0.1, 1.0, 0.0, 5, false, true});
    enhObstacle.sites.push_back(GenomeBindingSite{MoleculeSignatures::ThreatDetected, 0.1, 1.0, 0.0, 8, false, true});
    sys.resolver->addEnhancer(enhObstacle);

    // Enhancer E-201: Threat Evasion -> Activates Exon C (Shortest Path)
    GenomeEnhancer enhThreat;
    enhThreat.id = 201;
    enhThreat.targetRegion = regC_Id;
    enhThreat.name = "Threat_Evasion_Enhancer";
    enhThreat.activationThreshold = 0.35;
    enhThreat.cooperativity = 1.5;
    enhThreat.sites.push_back(GenomeBindingSite{MoleculeSignatures::ThreatDetected, 0.2, 1.5, 0.0, 10, true, true});
    sys.resolver->addEnhancer(enhThreat);

    // Enhancer E-202: Energy Low -> Activates Exon D (Energy Cost Optimization)
    GenomeEnhancer enhEnergy;
    enhEnergy.id = 202;
    enhEnergy.targetRegion = regD_Id;
    enhEnergy.name = "Energy_Optimization_Enhancer";
    enhEnergy.activationThreshold = 0.40;
    enhEnergy.cooperativity = 2.2; // High cooperativity
    enhEnergy.maxBindingDistance = 30;
    enhEnergy.sites.push_back(GenomeBindingSite{MoleculeSignatures::EnergyLow, 0.25, 1.0, 0.0, 12, true, true});
    enhEnergy.sites.push_back(GenomeBindingSite{MoleculeSignatures::EnergyLow, 0.25, 1.0, 0.0, 24, true, true});
    sys.resolver->addEnhancer(enhEnergy);

    // Enhancer E-204: Safe Route Recovery -> Activates Exon E (Threat Avoidance & Signal Emission)
    GenomeEnhancer enhRecovery;
    enhRecovery.id = 204;
    enhRecovery.targetRegion = regE_Id;
    enhRecovery.name = "Safe_Route_Recovery_Enhancer";
    enhRecovery.activationThreshold = 0.45;
    enhRecovery.cooperativity = 1.8;
    enhRecovery.maxBindingDistance = 40;
    enhRecovery.sites.push_back(GenomeBindingSite{MoleculeSignatures::EnergyLow, 0.25, 1.0, 0.0, 15, false, true});
    enhRecovery.sites.push_back(GenomeBindingSite{MoleculeSignatures::ThreatDetected, 0.15, 1.0, 0.0, 30, false, true});
    sys.resolver->addEnhancer(enhRecovery);

    // -------------------------------------------------------------
    // Wire SpliceGraph with Exons & AST fragment generators
    // -------------------------------------------------------------
    SpliceExon exonA;
    exonA.id = "NAV_A";
    exonA.name = "Exon_A_Position";
    exonA.regionId = regA_Id;
    exonA.inputType = CppType::makeInt64();
    exonA.outputType = CppType::makeInt64();
    exonA.compatibleNextExons = {"NAV_B"};
    exonA.astFragmentBuilder = []() {
        // state = state + 10;
        return AstNode::makeBinaryExpr(
            "=",
            AstNode::makeIdentifier("state"),
            AstNode::makeBinaryExpr("+", AstNode::makeIdentifier("state"), AstNode::makeLiteralInt(10))
        );
    };
    sys.spliceGraph->addExon(exonA);

    SpliceExon exonB;
    exonB.id = "NAV_B";
    exonB.name = "Exon_B_Obstacle";
    exonB.regionId = regB_Id;
    exonB.inputType = CppType::makeInt64();
    exonB.outputType = CppType::makeInt64();
    exonB.compatibleNextExons = {"NAV_C", "NAV_D"};
    exonB.astFragmentBuilder = []() {
        // state = state * 2;
        return AstNode::makeBinaryExpr(
            "=",
            AstNode::makeIdentifier("state"),
            AstNode::makeBinaryExpr("*", AstNode::makeIdentifier("state"), AstNode::makeLiteralInt(2))
        );
    };
    sys.spliceGraph->addExon(exonB);

    SpliceExon exonC;
    exonC.id = "NAV_C";
    exonC.name = "Exon_C_ShortestPath";
    exonC.regionId = regC_Id;
    exonC.inputType = CppType::makeInt64();
    exonC.outputType = CppType::makeInt64();
    exonC.compatibleNextExons = {};
    exonC.astFragmentBuilder = []() {
        // state = state + 100;
        return AstNode::makeBinaryExpr(
            "=",
            AstNode::makeIdentifier("state"),
            AstNode::makeBinaryExpr("+", AstNode::makeIdentifier("state"), AstNode::makeLiteralInt(100))
        );
    };
    sys.spliceGraph->addExon(exonC);

    SpliceExon exonD;
    exonD.id = "NAV_D";
    exonD.name = "Exon_D_EnergyCost";
    exonD.regionId = regD_Id;
    exonD.inputType = CppType::makeInt64();
    exonD.outputType = CppType::makeInt64();
    exonD.compatibleNextExons = {"NAV_E"};
    exonD.astFragmentBuilder = []() {
        // state = state - 5;
        return AstNode::makeBinaryExpr(
            "=",
            AstNode::makeIdentifier("state"),
            AstNode::makeBinaryExpr("-", AstNode::makeIdentifier("state"), AstNode::makeLiteralInt(5))
        );
    };
    sys.spliceGraph->addExon(exonD);

    SpliceExon exonE;
    exonE.id = "NAV_E";
    exonE.name = "Exon_E_ThreatAvoidance";
    exonE.regionId = regE_Id;
    exonE.inputType = CppType::makeInt64();
    exonE.outputType = CppType::makeInt64();
    exonE.compatibleNextExons = {};
    exonE.astFragmentBuilder = []() {
        // emitMolecule(0x1005, 0.75); state = state + 42;
        auto compound = std::make_shared<AstNode>();
        compound->kind = AstNodeKind::CompoundStmt;

        auto emitNode = AstNode::makeEmitMolecule(MoleculeSignatures::RecoverySignal, 0.75);
        compound->children.push_back(emitNode);

        auto assignNode = AstNode::makeBinaryExpr(
            "=",
            AstNode::makeIdentifier("state"),
            AstNode::makeBinaryExpr("+", AstNode::makeIdentifier("state"), AstNode::makeLiteralInt(42))
        );
        compound->children.push_back(assignNode);
        return compound;
    };
    sys.spliceGraph->addExon(exonE);

    // -------------------------------------------------------------
    // Register Developmental Entities and Rules in PhenotypeBuilder
    // -------------------------------------------------------------
    GenomeEntityDefinition defNeuron;
    defNeuron.entityType = 1;
    defNeuron.name = "CorticalNeuron";
    defNeuron.category = EntityCategory::Neuron;
    sys.phenotypeBuilder->registerEntityDefinition(defNeuron);

    GenomeEntityDefinition defSynapse;
    defSynapse.entityType = 2;
    defSynapse.name = "ExcitatorySynapse";
    defSynapse.category = EntityCategory::Synapse;
    sys.phenotypeBuilder->registerEntityDefinition(defSynapse);

    GenomeEntityDefinition defAssembly;
    defAssembly.entityType = 3;
    defAssembly.name = "NeuralAssembly";
    defAssembly.category = EntityCategory::Assembly;
    sys.phenotypeBuilder->registerEntityDefinition(defAssembly);

    GenomeEntityDefinition defProgram;
    defProgram.entityType = 4;
    defProgram.name = "MatureProgram";
    defProgram.category = EntityCategory::Program;
    sys.phenotypeBuilder->registerEntityDefinition(defProgram);

    // Development constraints
    DevelopmentRule ruleNeuron;
    ruleNeuron.targetEntity = 1;
    ruleNeuron.constraint.minimumStage = NeuralDevelopmentStage::Neonatal;
    ruleNeuron.constraint.requiredLineage = TissueLineage::AnteriorNervous;
    sys.phenotypeBuilder->registerDevelopmentRule(ruleNeuron);

    DevelopmentRule ruleProgram;
    ruleProgram.targetEntity = 4;
    ruleProgram.constraint.minimumStage = NeuralDevelopmentStage::Mature;
    ruleProgram.constraint.requiredLineage = TissueLineage::AnteriorNervous;
    sys.phenotypeBuilder->registerDevelopmentRule(ruleProgram);

    return sys;
}

} // namespace genomic
