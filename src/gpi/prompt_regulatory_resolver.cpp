#include "gpi/prompt_regulatory_resolver.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "genome/genome_decoder.hpp"

namespace genomic::gpi {
namespace {

std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto end = value.find(separator, begin);
        parts.push_back(value.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return parts;
}

bool validWord(const std::string& word) {
    if (word.empty()) return false;
    for (const auto ch : word) {
        if (!(ch >= 'a' && ch <= 'z') && !(ch >= '0' && ch <= '9') && ch != '_') return false;
    }
    return true;
}

bool validOperation(const std::string& operation) {
    static const std::unordered_set<std::string> allowed{
        "input", "csv", "alarm", "mean", "max", "min", "sum", "count"
    };
    return allowed.contains(operation);
}

void replaceAll(std::string& input, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = input.find(from, pos)) != std::string::npos) {
        input.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::vector<std::string> tokenize(const std::string& text) {
    std::string folded = text;
    replaceAll(folded, "Ä", "Ae"); replaceAll(folded, "Ö", "Oe");
    replaceAll(folded, "Ü", "Ue"); replaceAll(folded, "ä", "ae");
    replaceAll(folded, "ö", "oe"); replaceAll(folded, "ü", "ue");
    replaceAll(folded, "ß", "ss");
    std::vector<std::string> tokens;
    std::string word;
    for (const unsigned char ch : folded) {
        if (std::isalnum(ch) != 0 || ch == '_' || (ch == '-' && word.empty())) {
            word.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) tokens.push_back(word);
    return tokens;
}

std::optional<int64_t> number(const std::string& token) {
    static const std::unordered_map<std::string, int64_t> named{
        {"ein",1},{"eins",1},{"eine",1},{"zwei",2},{"drei",3},{"vier",4},
        {"fuenf",5},{"sechs",6},{"sieben",7},{"acht",8},{"neun",9},
        {"zehn",10},{"elf",11},{"zwoelf",12},{"dreizehn",13},
        {"vierzehn",14},{"fuenfzehn",15},{"sechzehn",16},{"siebzehn",17},
        {"achtzehn",18},{"neunzehn",19},{"zwanzig",20}
    };
    if (const auto it = named.find(token); it != named.end()) return it->second;
    int64_t value = 0;
    const auto* begin = token.data();
    const auto* end = begin + token.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    if (error == std::errc{} && ptr == end) return value;
    return std::nullopt;
}

bool sampleUnit(const std::string& token) {
    static const std::unordered_set<std::string> units{
        "sensor", "sensoren", "temperatursensor", "temperatursensoren",
        "messwert", "messwerte", "messwerten", "wert", "werte", "werten",
        "zahl", "zahlen", "sample", "samples", "number", "numbers", "value", "values"
    };
    return units.contains(token);
}

bool ignorable(const std::string& token) {
    static const std::unordered_set<std::string> stop{
        "ein", "eins", "eine", "einen", "einem", "einer", "der", "die", "das", "den", "dem",
        "und", "oder", "sowie", "mit", "von", "vom", "zum", "zur", "zu", "im", "in", "am", "an", "auf",
        "aus", "fuer", "als", "mehr", "weniger", "bei", "ab", "bis", "alle", "allen", "aller",
        "ueber", "unter", "groesser", "kleiner", "mindestens", "hoechstens", "gleich", "wenn",
        "sobald", "ist", "sind", "wird", "werden", "soll", "sollen", "kann", "koennen", "dann",
        "erzeuge", "erstelle", "entwickle", "programmiere", "schreibe", "baue", "generiere",
        "programm", "anwendung", "tool", "funktion", "bitte", "mir", "ich", "moechte", "dass",
        "es", "sie", "er", "pro", "je", "jedem", "jeden", "jeweils", "berechne", "berechnen",
        "gib", "gibt", "ausgibt", "ausgeben", "zeige", "anzeigen", "aus", "loese", "wertes",
        "the", "a", "an", "and", "or", "for", "of", "to", "with", "that", "which", "is", "are",
        "create", "build", "write", "program", "app", "application", "please", "me", "calculate",
        "print", "output", "show", "all", "each", "from", "when", "if", "on", "as", "at", "by",
        "above", "below", "over", "under", "greater", "less", "than", "threshold", "limit",
        "grenzwert", "schwellwert", "limitwert", "ueberschreitung", "unterschreitung",
        "kritischen", "abweichungen", "ausloest", "ausloesen", "alarmiert", "sensorwerte"
    };
    return stop.contains(token);
}

struct Marker {
    ThresholdMode mode;
    std::size_t distance;
};
std::optional<Marker> thresholdMarker(const std::vector<std::string>& tokens, std::size_t pos) {
    const auto& token = tokens[pos];
    if (token == "ueber" || token == "above" || token == "over" || token == "groesser")
        return Marker{ThresholdMode::Greater, token == "groesser" && pos + 1 < tokens.size() && tokens[pos + 1] == "als" ? 2u : 1u};
    if (token == "unter" || token == "below" || token == "under" || token == "kleiner")
        return Marker{ThresholdMode::Less, token == "kleiner" && pos + 1 < tokens.size() && tokens[pos + 1] == "als" ? 2u : 1u};
    if (token == "mehr" && pos + 1 < tokens.size() && tokens[pos + 1] == "als")
        return Marker{ThresholdMode::Greater, 2};
    if (token == "weniger" && pos + 1 < tokens.size() && tokens[pos + 1] == "als")
        return Marker{ThresholdMode::Less, 2};
    if (token == "ab" || token == "mindestens" || token == "grenzwert" || token == "schwellwert" ||
        token == "threshold" || token == "bei")
        return Marker{ThresholdMode::GreaterEqual, 1};
    if (token == "bis" || token == "hoechstens") return Marker{ThresholdMode::LessEqual, 1};
    return std::nullopt;
}

} // namespace

PromptRegulatoryResolver::PromptRegulatoryResolver(const GenomeStrand& genome)
    : genomeHash_(genome.getGenomeHash()) {
    if (!genome.isFrozen() || !genome.verifyIntegrity().passed)
        throw std::invalid_argument("GPI requires an intact, frozen genome");
    std::unordered_set<std::string> ids;
    std::unordered_set<uint64_t> signatures;
    for (const auto& region : genome.getAllRegions()) {
        if (region.kind != RegionKind::PromptCapability) continue;
        // Read the encoded physical bases, not an unverified metadata copy.
        const auto decoded = GenomeDecoder::parseRegionsFromStrand(region.bases);
        if (decoded.size() != 1 || decoded.front().kind != RegionKind::PromptCapability ||
            decoded.front().name != region.name || decoded.front().payload != region.payload)
            throw std::invalid_argument("GPI capability region payload/base mismatch");
        const auto bytes = decoded.front().payload;
        const auto fields = split(std::string(bytes.begin(), bytes.end()), '|');
        if (fields.size() != 5 || fields[0] != "gpi-cap-v1" ||
            !validWord(fields[1]) || !validOperation(fields[2]) || fields[4].empty())
            throw std::invalid_argument("Malformed GPI capability region: " + region.name);
        uint64_t signature = 0;
        const auto [ptr, err] = std::from_chars(fields[3].data(), fields[3].data() + fields[3].size(), signature);
        if (err != std::errc{} || ptr != fields[3].data() + fields[3].size() || signature == 0 ||
            !ids.insert(fields[1]).second || !signatures.insert(signature).second)
            throw std::invalid_argument("Duplicate or invalid GPI capability signature/id");
        PromptCapability capability{region.id, fields[1], fields[2], signature, split(fields[4], ',')};
        std::unordered_set<std::string> terms;
        for (const auto& trigger : capability.triggers) {
            if (!validWord(trigger) || !terms.insert(trigger).second)
                throw std::invalid_argument("Invalid GPI trigger in region " + region.name);
        }
        capabilities_.push_back(std::move(capability));
    }
    if (capabilities_.empty()) throw std::invalid_argument("Genome has no GPI capabilities");
}

PromptResolution PromptRegulatoryResolver::resolve(const std::string& prompt) const {
    PromptResolution result;
    result.originalPrompt = prompt;
    result.genomeHash = genomeHash_;
    if (prompt.empty() || prompt.size() > 8192) {
        result.diagnostics.push_back("Prompt must contain 1..8192 bytes");
        return result;
    }
    const auto tokens = tokenize(prompt);
    if (tokens.empty()) {
        result.diagnostics.push_back("Prompt has no recognizable words");
        return result;
    }
    std::unordered_set<std::string> recognized;
    for (const auto& capability : capabilities_) {
        std::string matched;
        for (const auto& token : tokens) {
            if (std::find(capability.triggers.begin(), capability.triggers.end(), token) != capability.triggers.end()) {
                recognized.insert(token);
                if (matched.empty()) matched = token;
            }
        }
        if (!matched.empty()) {
            result.molecules.setConcentration(0, capability.signature, 0.9, capability.id, MoleculeClass::Activator);
            result.capabilities.push_back({capability.id, capability.operation, capability.regionId,
                                           capability.signature, matched, 0.0});
        }
    }
    // Every synthesized application requires a data-input path. This is a
    // regulatory dependency, not a predefined navigation-exon chain.
    if (!result.capabilities.empty() &&
        std::none_of(result.capabilities.begin(), result.capabilities.end(),
                     [](const ActivatedCapability& c) { return c.operation == "input"; })) {
        const auto it = std::find_if(capabilities_.begin(), capabilities_.end(),
                                     [](const PromptCapability& c) { return c.operation == "input"; });
        if (it == capabilities_.end()) {
            result.diagnostics.push_back("Genomic input capability is absent");
        } else {
            result.molecules.setConcentration(0, it->signature, 0.75, it->id, MoleculeClass::Activator);
            result.capabilities.push_back({it->id, it->operation, it->regionId,
                                           it->signature, "implicit_dependency", 0.0});
        }
    }

    // Interpret sample count only when a number precedes a known data unit.
    std::unordered_set<std::size_t> sampleNumberPositions;
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        const auto n = number(tokens[i]);
        if (!n || !sampleUnit(tokens[i + 1])) continue;
        sampleNumberPositions.insert(i);
        if (*n < 1 || *n > 1000000) {
            result.diagnostics.push_back("Sample count must be between 1 and 1000000");
        } else if (result.expectedSamples && *result.expectedSamples != static_cast<std::size_t>(*n)) {
            result.diagnostics.push_back("Conflicting sample counts");
        } else {
            result.expectedSamples = static_cast<std::size_t>(*n);
        }
    }
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto marker = thresholdMarker(tokens, i);
        if (!marker || i + marker->distance >= tokens.size()) continue;
        const auto index = i + marker->distance;
        const auto n = number(tokens[index]);
        if (!n || sampleNumberPositions.contains(index)) continue;
        if (result.threshold && (*result.threshold != *n || result.thresholdMode != marker->mode)) {
            result.diagnostics.push_back("Conflicting threshold conditions");
        } else {
            result.threshold = *n;
            result.thresholdMode = marker->mode;
        }
    }

    for (const auto& token : tokens) {
        if (recognized.contains(token) || number(token) || ignorable(token)) continue;
        // Unknown content is not silently discarded: doing so would produce
        // a plausible-looking but incomplete program for unsupported prompts.
        result.diagnostics.push_back("Unsupported or ambiguous requirement: " + token);
    }
    if (result.capabilities.empty()) result.diagnostics.push_back("No genomic programming capability matched");
    const bool alarm = std::any_of(result.capabilities.begin(), result.capabilities.end(),
                                   [](const ActivatedCapability& c) { return c.operation == "alarm"; });
    if (alarm && !result.threshold)
        result.diagnostics.push_back("Alarm requires an explicit numeric threshold (e.g. 'ab 80')");

    RegulatoryResolver regulation;
    for (const auto& capability : capabilities_) {
        GenomeEnhancer enhancer;
        enhancer.id = capability.regionId;
        enhancer.targetRegion = capability.regionId;
        enhancer.name = "GPI_ENH_" + capability.id;
        enhancer.activationThreshold = 0.4;
        enhancer.cooperativity = 1.0;
        enhancer.sites.push_back(GenomeBindingSite{capability.signature, 0.2, 1.0, 0.0, 0, false, true});
        regulation.addEnhancer(enhancer);
    }
    result.regulation = regulation.resolve(result.molecules);
    for (auto& capability : result.capabilities) {
        if (!result.regulation.activeRegions.contains(capability.regionId)) {
            result.diagnostics.push_back("Molecular regulation did not activate " + capability.id);
        } else {
            capability.activity = result.regulation.regionActivityScores.at(capability.regionId);
        }
    }
    result.supported = result.diagnostics.empty();
    return result;
}

} // namespace genomic::gpi
