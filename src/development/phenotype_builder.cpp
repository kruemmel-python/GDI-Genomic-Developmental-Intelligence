#include "development/phenotype_builder.hpp"
#include <stdexcept>

namespace genomic {

PhenotypeBuilder::PhenotypeBuilder(NeuralDevelopmentStage initialStage)
    : currentStage_(initialStage) {}

void PhenotypeBuilder::setStage(NeuralDevelopmentStage stage) noexcept {
    currentStage_ = stage;
}

NeuralDevelopmentStage PhenotypeBuilder::getStage() const noexcept {
    return currentStage_;
}

void PhenotypeBuilder::setResourceLevel(double level) noexcept {
    resourceLevel_ = level;
}

double PhenotypeBuilder::getResourceLevel() const noexcept {
    return resourceLevel_;
}

void PhenotypeBuilder::registerEntityDefinition(const GenomeEntityDefinition& def) {
    definitions_[def.entityType] = def;
}

void PhenotypeBuilder::registerDevelopmentRule(const DevelopmentRule& rule) {
    rules_[rule.targetEntity] = rule;
}

bool PhenotypeBuilder::canDevelop(GenomeSymbolId entityType,
                                  const LineageContext& lineage,
                                  const MolecularPool& pool) const {
    auto ruleIt = rules_.find(entityType);
    if (ruleIt == rules_.end()) {
        return true; // No special constraints
    }

    const auto& c = ruleIt->second.constraint;

    // 1. Stage check
    if (static_cast<uint8_t>(currentStage_) < static_cast<uint8_t>(c.minimumStage)) {
        return false;
    }

    // 2. Resource check
    if (resourceLevel_ < c.minimumResourceLevel) {
        return false;
    }

    // 3. Lineage check
    if (lineage.lineage != c.requiredLineage) {
        return false;
    }

    // 4. Required molecules
    for (uint64_t sig : c.requiredMolecules) {
        if (!pool.hasMolecule(lineage.compartment, sig)) {
            return false;
        }
    }

    return true;
}

std::shared_ptr<PhenotypeEntityInstance> PhenotypeBuilder::developEntity(
    GenomeSymbolId entityType,
    const LineageContext& lineage,
    const MolecularPool& pool,
    uint64_t currentTick
) {
    if (!canDevelop(entityType, lineage, pool)) {
        throw std::runtime_error("Development constraint check failed for entity " + std::to_string(entityType));
    }

    std::string name = "Entity_" + std::to_string(entityType);
    EntityCategory cat = EntityCategory::Program;

    auto defIt = definitions_.find(entityType);
    if (defIt != definitions_.end()) {
        name = defIt->second.name;
        cat = defIt->second.category;
    }

    auto inst = std::make_shared<PhenotypeEntityInstance>();
    inst->instanceId = nextInstanceId_++;
    inst->entityType = entityType;
    inst->category = cat;
    inst->name = name;
    inst->health = 1.0;
    inst->birthTick = currentTick;

    instances_.push_back(inst);
    return inst;
}

const std::vector<std::shared_ptr<PhenotypeEntityInstance>>& PhenotypeBuilder::getInstances() const noexcept {
    return instances_;
}

size_t PhenotypeBuilder::countByCategory(EntityCategory cat) const {
    size_t count = 0;
    for (const auto& inst : instances_) {
        if (inst->category == cat) count++;
    }
    return count;
}

} // namespace genomic
