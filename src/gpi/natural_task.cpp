#include "gpi/natural_task.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <regex>
#include <string>
#include <vector>
namespace genomic::gpi {
namespace {
std::string lower(std::string s) {
    for (char& c : s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string trim(const std::string& s) {
    const auto a=s.find_first_not_of(" \r\n\t");
    if (a==std::string::npos) return {};
    const auto b=s.find_last_not_of(" \r\n\t");
    return s.substr(a,b-a+1);
}
bool any(const std::string& s, const std::vector<std::string>& terms) {
    for (const auto& term:terms) if (s.find(term)!=std::string::npos) return true;
    return false;
}
}
NaturalTaskResult translateNaturalTask(const std::string& request) {
    if (request.empty() || request.size()>2048 || request.find('\n')!=std::string::npos)
        return {{},"Natural task must be a single line of at most 2048 bytes"};
    const auto s=lower(trim(request));
    // A small, explicit grammar: the complete sentence must match. No hidden
    // keyword fallback or fabricated requirements.
    const std::regex de(R"(^(?:berechne|ermittle) (?:die |den )?(summe|durchschnitt|mittelwert|minimum|maximum|anzahl) (?:der |von )?(quadrate|zahlen|werte) (?:der |von )?(?:ersten |den ersten )?([0-9]+) (?:zahlen|werten)(?: und gib die werte aus)?[.!]?$)");
    const std::regex en(R"(^(?:calculate|compute) (?:the )?(sum|mean|average|minimum|maximum|count) of (?:the )?(?:first )?([0-9]+) (squares|numbers|values)(?: and print values)?[.!]?$)");
    std::smatch match;
    std::string reduction, map, count;
    bool emit=false;
    // Novel expressions can be supplied in a strictly delimited natural-language
    // sentence. ExpressionParser and TaskGenomePolicy remain authoritative.
    const std::regex deFormula(R"(^(?:berechne|ermittle) (?:die |den )?(summe|durchschnitt|mittelwert|minimum|maximum|anzahl) (?:der |von )?(?:ersten )?([0-9]+) (?:zahlen|werten) mit formel ([a-z0-9+*/%()._ !<>=&|-]+?)(?: mit filter ([a-z0-9+*/%()._ !<>=&|-]+))?$)");
    const std::regex enFormula(R"(^(?:calculate|compute) (?:the )?(sum|mean|average|minimum|maximum|count) of (?:the )?first ([0-9]+) (?:numbers|values) using formula ([a-z0-9+*/%()._ !<>=&|-]+?)(?: with filter ([a-z0-9+*/%()._ !<>=&|-]+))?$)");
    if (std::regex_match(s,match,deFormula) || std::regex_match(s,match,enFormula)) {
        reduction=match[1].str(); count=match[2].str(); map=trim(match[3].str());
        std::string filter=match[4].matched?trim(match[4].str()):"";
        if (reduction=="summe") reduction="sum";
        else if (reduction=="durchschnitt" || reduction=="mittelwert" || reduction=="average") reduction="mean";
        else if (reduction=="minimum") reduction="min";
        else if (reduction=="maximum") reduction="max";
        else if (reduction=="anzahl") reduction="count";
        unsigned n=0;
        const auto [p,ec]=std::from_chars(count.data(),count.data()+count.size(),n);
        if (ec!=std::errc{} || p!=count.data()+count.size() || n<1 || n>32)
            return {{},"Natural-language sample count must be 1..32 for verified execution"};
        return {"map="+map+(filter.empty()?"":"; filter="+filter)+"; reduce="+reduction+
                "; samples="+std::to_string(n)+"; emit=false",{}};
    }
    if (std::regex_match(s,match,de)) {
        reduction=match[1].str(); map=match[2].str(); count=match[3].str();
        emit=s.find("und gib die werte aus")!=std::string::npos;
        if (reduction=="summe") reduction="sum";
        else if (reduction=="durchschnitt" || reduction=="mittelwert") reduction="mean";
        else if (reduction=="minimum") reduction="min";
        else if (reduction=="maximum") reduction="max";
        else if (reduction=="anzahl") reduction="count";
        map=map=="quadrate"?"x*x":"x";
    } else if (std::regex_match(s,match,en)) {
        reduction=match[1].str(); count=match[2].str(); map=match[3].str();
        emit=s.find("and print values")!=std::string::npos;
        if (reduction=="average") reduction="mean";
        else if (reduction=="minimum") reduction="min";
        else if (reduction=="maximum") reduction="max";
        map=map=="squares"?"x*x":"x";
    } else {
        return {{},"Unsupported natural-language specification; use explicit map=...; reduce=... or a supported exact sentence"};
    }
    unsigned n=0;
    const auto [p,ec]=std::from_chars(count.data(),count.data()+count.size(),n);
    if (ec!=std::errc{} || p!=count.data()+count.size() || n<1 || n>32)
        return {{},"Natural-language sample count must be 1..32 for verified execution"};
    if (any(s,{"datei","network","internet","file","system("}))
        return {{},"Unsupported side-effect request"};
    return {"map="+map+"; reduce="+reduction+"; samples="+std::to_string(n)+"; emit="+(emit?"true":"false"),{}};
}
}
