# C++20-Abdeckung

Status: **PARTIAL**.

Implementierungsnachtrag: Compiler- und Linkerdiagnosen besitzen jetzt ein strukturiertes Orts- und Fehlerklassenmodell. Dies verbessert Reparaturzuordnung, ist aber kein Ersatz für eine ISO-Konformitätssuite.

Die 74 Einzeltests decken Parser/AST, Typanalyse, Templates/Deduktion, Concepts/Constraints, Overload-Auflösung, Referenzkollaps, Perfect Forwarding, Move/RAII, `constexpr`, `switch`, Lebensdauer, Exceptions und Memory-Model-Modellierung ab. Unabhängige Reparaturprojekte wurden mit dem realen GCC 16.1.0 kompiliert und gelinkt.

Nicht belegt sind vollständige Module-Unterstützung, die gesamte Standardbibliothek, alle Diagnoseklassen oder ISO-C++20-Konformität. Es wurde keine etablierte Konformitätssuite ausgeführt; deshalb wird keine vollständige C++20-Abdeckung behauptet.
