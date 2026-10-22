#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "development/ontology.hpp"
#include "development/development_rule.hpp"
#include "development/lineage_context.hpp"
#include "regulation/molecular_pool.hpp"
#include "runtime/expressed_program.hpp"

namespace genomic {

struct PhenotypeEntityInstance {
    uint64_t instanceId{0};
    GenomeSymbolId entityType{0};
    EntityCategory category{EntityCategory::Program};
    std::string name;
    double health{1.0};
    uint64_t birthTick{0};
};

class PhenotypeBuilder {
public:
    explicit PhenotypeBuilder(NeuralDevelopmentStage initialStage = NeuralDevelopmentStage::Neonatal);

    void setStage(NeuralDevelopmentStage stage) noexcept;
    NeuralDevelopmentStage getStage() const noexcept;

    void setResourceLevel(double level) noexcept;
    double getResourceLevel() const noexcept;

    void registerEntityDefinition(const GenomeEntityDefinition& def);
    void registerDevelopmentRule(const DevelopmentRule& rule);

    bool canDevelop(GenomeSymbolId entityType,
                    const LineageContext& lineage,
                    const MolecularPool& pool) const;

    std::shared_ptr<PhenotypeEntityInstance> developEntity(
        GenomeSymbolId entityType,
        const LineageContext& lineage,
        const MolecularPool& pool,
        uint64_t currentTick
    );

    const std::vector<std::shared_ptr<PhenotypeEntityInstance>>& getInstances() const noexcept;
    size_t countByCategory(EntityCategory cat) const;

private:
    NeuralDevelopmentStage currentStage_{NeuralDevelopmentStage::Neonatal};
    double resourceLevel_{1.0};
    uint64_t nextInstanceId_{1};

    std::unordered_map<GenomeSymbolId, GenomeEntityDefinition> definitions_;
    std::unordered_map<GenomeSymbolId, DevelopmentRule> rules_;
    std::vector<std::shared_ptr<PhenotypeEntityInstance>> instances_;
};

} // namespace genomic
