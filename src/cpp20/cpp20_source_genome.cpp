#include "cpp20/cpp20_source_genome.hpp"

#include "genome/genome_decoder.hpp"
#include "genome/genome_strand.hpp"

#include <algorithm>
#include <stdexcept>

namespace genomic {
namespace {
uint64_t hashBytes(const std::string& value) noexcept {
    uint64_t hash=1469598103934665603ULL;
    for(const unsigned char byte:value){hash^=byte;hash*=1099511628211ULL;}
    return hash;
}

bool safeRelativePath(const std::filesystem::path& path) {
    if(path.empty()||path.is_absolute()||path.has_root_name()||path.has_root_directory())return false;
    for(const auto& part:path)if(part==".."||part==".")return false;
    return true;
}
}

Cpp20SourceGenome Cpp20SourceGenome::fromGenome(const GenomeStrand& genome) {
    if(!genome.isFrozen()||!genome.verifyIntegrity().passed)
        throw std::invalid_argument("C++20 source generation requires an intact frozen genome");
    for(const auto& region:genome.getAllRegions()) {
        if(region.kind!=RegionKind::CppGenerativeRule)continue;
        const std::string payload(region.payload.begin(),region.payload.end());
        if(payload.find("format=cpp20-source-space-v1;")==std::string::npos||
           payload.find("encoding=all-octets;")==std::string::npos||
           payload.find("semantic-oracle=external-cpp20-compiler;")==std::string::npos)
            throw std::invalid_argument("Malformed genomic C++20 generative rule");
        Cpp20SourceGenome result;result.genomeHash_=genome.getGenomeHash();
        result.ruleRegion_=region.id;result.format_="cpp20-source-space-v1";
        const std::string prefix="edit-operators=";const auto begin=payload.find(prefix);
        if(begin==std::string::npos)throw std::invalid_argument("Genomic C++20 source space has no edit operators");
        const auto end=payload.find(';',begin);std::size_t cursor=begin+prefix.size();
        while(cursor<end) {
            const auto comma=payload.find(',',cursor);const auto stop=std::min(comma==std::string::npos?end:comma,end);
            if(stop>cursor)result.mutationOperators_.insert(payload.substr(cursor,stop-cursor));
            cursor=stop+1;
        }
        if(result.mutationOperators_.empty())throw std::invalid_argument("Genomic C++20 source space has empty edit operators");
        const std::string structurePrefix="project-structures=";const auto structureBegin=payload.find(structurePrefix);
        if(structureBegin==std::string::npos)throw std::invalid_argument("Genomic C++20 source space has no project structures");
        const auto structureEnd=payload.find(';',structureBegin);cursor=structureBegin+structurePrefix.size();
        while(cursor<structureEnd) {
            const auto comma=payload.find(',',cursor);const auto stop=std::min(comma==std::string::npos?structureEnd:comma,structureEnd);
            if(stop>cursor)result.projectStructures_.insert(payload.substr(cursor,stop-cursor));
            cursor=stop+1;
        }
        if(result.projectStructures_.empty())throw std::invalid_argument("Genomic C++20 source space has empty project structures");
        const std::string headerPrefix="header-symbols=";const auto headerBegin=payload.find(headerPrefix);
        if(headerBegin==std::string::npos)throw std::invalid_argument("Genomic C++20 source space has no header-symbol rules");
        const auto headerEnd=payload.find(';',headerBegin);cursor=headerBegin+headerPrefix.size();
        while(cursor<headerEnd) {
            const auto comma=payload.find(',',cursor);const auto stop=std::min(comma==std::string::npos?headerEnd:comma,headerEnd);
            const auto separator=payload.find('@',cursor);
            if(separator==std::string::npos||separator>=stop||separator==cursor||separator+1==stop)
                throw std::invalid_argument("Malformed genomic header-symbol rule");
            result.symbolHeaders_.emplace(payload.substr(cursor,separator-cursor),payload.substr(separator+1,stop-separator-1));
            cursor=stop+1;
        }
        return result;
    }
    throw std::invalid_argument("Genome has no universal C++20 source-space rule");
}

GenomicSourceArtifact Cpp20SourceGenome::encode(const std::filesystem::path& relativePath,
                                                 const std::string& source) const {
    if(!safeRelativePath(relativePath))throw std::invalid_argument("Generated source path must be safe and relative");
    if(source.empty())throw std::invalid_argument("Generated source must not be empty");
    GenomicSourceArtifact result;result.relativePath=relativePath;
    result.encodedSource=GenomeDecoder::encodeString(source);result.sourceBytes=source.size();
    result.sourceHash=hashBytes(source);result.immutableGenomeHash=genomeHash_;
    result.generativeRuleRegion=ruleRegion_;return result;
}

std::string Cpp20SourceGenome::decode(const GenomicSourceArtifact& artifact) const {
    if(artifact.immutableGenomeHash!=genomeHash_||artifact.generativeRuleRegion!=ruleRegion_)
        throw std::invalid_argument("Source artifact provenance does not match the immutable genome");
    const auto source=GenomeDecoder::decodeString(artifact.encodedSource);
    if(source.size()!=artifact.sourceBytes||hashBytes(source)!=artifact.sourceHash)
        throw std::invalid_argument("Source artifact failed byte-integrity verification");
    return source;
}

bool Cpp20SourceGenome::verifies(const GenomicSourceArtifact& artifact) const noexcept {
    try{return decode(artifact).size()==artifact.sourceBytes;}catch(...){return false;}
}

bool Cpp20SourceGenome::mutationEnabled(std::string_view operation) const {
    return mutationOperators_.find(std::string(operation))!=mutationOperators_.end();
}

bool Cpp20SourceGenome::structureEnabled(std::string_view structure) const {
    return projectStructures_.find(std::string(structure))!=projectStructures_.end();
}

std::vector<std::string> Cpp20SourceGenome::suggestedHeaders(std::string_view diagnostic) const {
    std::vector<std::string> result;
    for(const auto& [symbol,header]:symbolHeaders_)
        if(diagnostic.find(symbol)!=std::string_view::npos&&std::find(result.begin(),result.end(),header)==result.end())
            result.push_back(header);
    std::sort(result.begin(),result.end());return result;
}
} // namespace genomic
