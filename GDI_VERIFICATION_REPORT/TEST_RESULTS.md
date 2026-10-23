# Testergebnisse

Abschließender Lauf nach der Reparatur: **PASS**, 10/10, Exit 0.

Bestanden: `GDIBehavioralInductionEndToEnd`, `GPIv03UnitTests`, `GPIProjectEndToEnd`, `ComprehensiveSection43Tests`, `GPIPromptEndToEnd`, `GPIAgentEndToEnd`, `GPIResearchEndToEnd`, `GPIIntegratedEndToEnd`, `GPIGuiSelfTest`, `GPIGenerativeProjectEndToEnd`, `GDIKrProjectEndToEnd`.

Zusätzlich einzeln ausgeführt: `test_genomic_system` (74/74), `test_behavioral_induction`, `test_generative_project`; jeweils Exit 0. Der frühere `std::bad_alloc`/`basic_string::append`-Fehler war nicht reproduzierbar. Die Graphiteration verwendet vor Transformationen eine stabile Knotenkopie (`stableNodes`), wodurch Ersetzung während der Iteration vermieden wird.

Neuer externer Test: `GDIKrProjectEndToEnd` bestand einzeln in 72,36 Sekunden und innerhalb der vollständigen 11-Test-Suite in 129,22 Sekunden. Er erzeugte MDRAFTNE vollständig neu über die KR-Compiler-Engine, linkte Rust, Go und SDL, bestand den generierten ABI-Funktionstest und erreichte im gestarteten SDL-Programm den ersten gerenderten Frame. Dieser Test ist über `GDI_ENABLE_KR_E2E_TEST` reproduzierbar opt-in. Gesamtsuite: 11/11, 582,42 Sekunden.
