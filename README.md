# GDI – Genomic Developmental Intelligence

GDI ist ein in C++20 implementiertes Entwicklungs- und Forschungssystem für Programme. Ein unveränderliches lineares Genom beschreibt grammatische, regulatorische und generative Möglichkeiten. Lokale molekulare Zustände, Entwicklungszustände und verifizierte Erfahrung bestimmen, welche Fähigkeiten in einem Lauf exprimiert werden. Das Genom selbst wird dabei nicht verändert.

Das Repository enthält den reinen Projektquellcode. Der verwendete C++20-Compiler ist eine externe Abhängigkeit und gehört nicht zum Projekt.

## Was das System ausführt

- Es verfügt über ein **vollständiges genomisch gesteuertes C++20-Sprachsynthese- und Semantik-Regelwerk** als stabilen Fixpunkt:
  - **Eigenständiger `switch`-Syntax- und Synthesepfad**: Genomische Grammatik-, Parser-, AST-, Printer-, `constexpr`-Auswertungs-, Semantik-, IR-Lowering- und VM-Unterstützung für `switch (init; cond)`, `case`-Konstantenausdrücke, `default`, `[[fallthrough]]` sowie generative Multi-Way-`switch`-Synthese in `.goal`- und `.behavior`-Pipelines.
  - **Vollständige C++20-Typanalyse**: Mehrwort-Spezifizierer (`unsigned long long`, `long int`, `unsigned short`, Fixed-Width-Typen `int8_t`..`uint64_t`), `const`/`volatile`-Qualifizierer (führend und nachgestellt), Zeiger, LValue-/RValue-/Forwarding-Referenzen, Referenzkollabierung (`T& & -> T&`, `T&& && -> T&&`), `removeCVRef`, `decay`, Arrays, Funktionszeiger, `enum class` und Template-Spezialisierungen.
  - **Vollständige C++20-Template-Deduktion & Spezialisierung**: Rekursive Musterdeduktion über beliebig viele Template-Parameter (`const T*`, `U&`, `T[N]`, `Record<T, U>`), Forwarding-Referenzen (`T&&` mit LValue $\rightarrow$ `T&`), Parameter-Packs (`Ts...`), Non-Deduced Contexts (`std::type_identity_t<T>`), explizite und Default-Template-Argumente, Class Template Argument Deduction (CTAD), partielle Funktions- und Klassentemplate-Ordnung sowie SFINAE-Substitutionsprüfung.
  - **Vollständige C++20-Constraints & Concept-Subsumption**: Normalisierung beliebiger Konjunktionen (`&&`) und Disjunktionen (`||`) atomarer Constraints in Disjunktive Normalform (DNF) mit logischer Implikations- und Subsumptionsprüfung (`isMoreConstrained`) sowie `requires`-Ausdrucksauswertung.
  - **Vollständige C++20-Overload-Auflösung**: Exakte Implicit Conversion Sequence (ICS) Rangordnung (`ExactMatch` $\rightarrow$ `Promotion` $\rightarrow$ `StandardConversion` $\rightarrow$ `UserDefinedConversion` $\rightarrow$ `EllipsisConversion`), LValue/RValue-Referenzbindungsregeln (`T&&` vs. `const T&` vs. `T&`), Qualifikationsanpassungen (`T*` vs. `const T*`) und vollständige Tie-Breaker-Kaskade (Nicht-Template vor Template, partielle Template-Ordnung, Concept-Subsumption).
  - **Vollständige C++20-Lebensdauer-, RAII- & Borrow-Analyse**: Deterministische RAII-Destruktor-Reihenfolge (umgekehrte Deklarationsreihenfolge bei Blockende und `return`), Temporary Materialization (`PRValue` $\rightarrow$ `XValue`), Referenz-Lebensdauerverlängerung (`const T&` / `T&&`), Erkennung hängender Referenzen und Zeiger (`dangling reference/pointer` bei `return` lokaler Objekte), `std::move`-Tracking mit Use-after-Move-Schutz sowie Schutz vor uninitialisierten Lesezugriffen.
- Es erzeugt aus unterstützten deutsch- und englischsprachigen Aufgaben einen expliziten Zielvertrag.
- Es zerlegt Ziele in typisierte Operationen und Abhängigkeiten.
- Es entwickelt und verifiziert C++20-Kandidaten in begrenzten Entwicklungszyklen.
- Es modelliert Analyse, Synthese und Forschung als logisch getrennte Kompartimente über demselben eingefrorenen Genom.
- Es repräsentiert beliebige C++20-Quelldateien bytegenau als genomisch hergeleitete Artefakte.
- Es verarbeitet allgemeine mehrdateiige Projektgraphen mit Headern, Quellen, Modulen, Tests, Ressourcen, Build-Manifesten, Abhängigkeiten und Targets.
- Es synthetisiert aus typisierten Projekt-Blueprints (`.blueprint` V1 & V2) neue Header-, Implementierungs-, Anwendungs-, Modul- und Teststrukturen inklusive eigener Datentypen (`struct`, Methoden, `operator==`), `enum class`, C++20-`concept`s und eingeschränkter Funktions-Templates (`requires`).
- Es leitet aus typisierten Ziel-Operationsgraphen (`.goal` V1 & V2) eigenständig mehrkomponentige Funktionsausdrücke, Typkonstruktionen, Feld-/Methodenzugriffe, Komponentenabhängigkeiten, Tests und einen ausführbaren Mehrdatei-Projektgraphen mit Domänenmodulen ab.
- Es induziert aus mehreren Verhaltensanforderungen (`.behavior` V1 & V2) zusammenhängende Funktionen, eigene Datentypen mit Methoden sowie modulübergreifende Softwareprojekte, indem früher synthetisierte Typen, Methoden und Funktionen als genomisch autorisierte Bausteine für nachgelagerte Komponenten wiederverwendet werden.
- Es führt mit `gdi_intelligence` eine **eigenständig entwicklungsfähige, genomisch organisierte Programmierintelligenz** aus:
  - **Fremde, unbekannte C++20-Softwareprojekte erschließen & weiterentwickeln (`--foreign-project`)**: Lädt beliebige externe C++20-Projekte (mit oder ohne `project.gpi`) zunächst strikt **Read-Only**, analysiert AST, Semantik, Typen, Templates, Module, Schnittstellen, Datenflüsse und Verträge, bildet **gleichzeitig während der Analyse Architekturhypothesen**, leitet Entwicklungsziele ab, synthetisiert fehlende Modulfunktionen oder Reparaturen in einem isolierten Arbeitsbereich, verifiziert sie in der Sandbox und konsolidiert ein **doppeltes Gedächtnis**: das **Projektgedächtnis (`ProjectMemoryStore` / `GDIPM1`)** für Fakten über das konkrete Projekt und das **Entwicklungsgedächtnis (`DevelopmentalPrincipleStore` / `GDIDP1`)** für allgemeingültige, auf völlig andere Projekte übertragbare Lösungsprinzipien.
  - **Autonomer ontogenetischer Entwicklungsplaner (`--objective` / `--control-experiment`)**: Prüft aus einem übergeordneten Ziel eigenständig vorhandene Fähigkeiten im Gedächtnis, identifiziert Wissenslücken und leitet sein Entwicklungs-Curriculum mit unabhängigen Verhaltensprüfungen selbst ab.
  - **Mehrstufige ontogenetische Curricula (`.curriculum`: `Neonatal` $\rightarrow$ `Juvenile` $\rightarrow$ `Mature`)**.
- Es kompiliert, linkt und prüft erzeugte Programme gegen unabhängige Ein-/Ausgabe- und Exit-Code-Verträge.
- Es kann belegte Fehlerklassen genomisch autorisiert reparieren, darunter fehlende Standard-Header, fehlende Link-Abhängigkeiten sowie bestimmte Typ- und Zahlenfehler. Nachweislich fortgeschrittene Zwischenstände bleiben über mehrere Entwicklungszyklen erhalten.
- Es speichert ausschließlich vollständig verifizierte Transformationsgruppen atomar in einem verketteten Erfahrungsspeicher und prüft sie bei Transferaufgaben erneut.
- Unter Windows führt es unbekannten Programmcode in einer AppContainer-Sandbox ohne deklarierte Netzwerkfähigkeiten aus.

## Build

Voraussetzungen:

- CMake 3.20 oder neuer
- Ninja oder ein anderer von CMake unterstützter Generator
- ein C++20-Compiler

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_CXX_COMPILER="F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe"
cmake --build build
ctest --test-dir build --output-on-failure
```

Der Referenzstand wird mit Warnungen als Fehler (`-Wall -Wextra -Wpedantic -Werror`) gebaut. Die aktuelle Windows-Regression umfasst neun Unit- und End-to-End-Tests, darunter reale externe Kompilierungen und sandbox-isolierte Ausführungen.

## Desktop-Oberfläche – GDI v1.4.0

Unter Windows baut CMake die **aktualisierte native GDI-Oberfläche** als `gpi_gui.exe` und
legt daneben automatisch eine identische `gdi_gui.exe` an. Die alte ausführbare Datei
bleibt aus Kompatibilitätsgründen erhalten. **Der KR-Compiler wird nicht verändert.**

Die Oberfläche besitzt sieben Arbeitsbereiche in derselben Anwendung:

1. **GDI – Fremdes C++20-Projekt erschließen & weiterentwickeln (Read-Only + Dual-Memory)**:
   Lädt ein fremdes, bislang unbekanntes C++20-Projektverzeichnis (z. B. `examples/foreign_telemetry_project`), führt eine vollständige Read-Only-Sprach-, AST-, Modul- und Schnittstellenanalyse durch, bildet gleichzeitig Architekturhypothesen und Entwicklungsziele, synthetisiert fehlende Fähigkeiten isoliert in der Sandbox und speichert sowohl das **Projektgedächtnis (`GDIPM1`)** als auch das **übertragbare Entwicklungsgedächtnis (`GDIDP1`)**. Das Originalprojekt bleibt unangetastet (`ORIGINAL_UNTOUCHED=YES`).
2. **GDI – Autonomer ontogenetischer Entwicklungsplaner (Ziel $\rightarrow$ eigenes Curriculum)**:
   Nimmt nur ein übergeordnetes Ziel entgegen (z. B. *„Entwickle eine C++20-Anwendung, die zweidimensionale geometrische Objekte verwaltet, deren Eigenschaften berechnet und die Ergebnisse speichert.“*), prüft das Komponentengedächtnis (`GDIC1`), bestimmt vorhandene Fähigkeiten und Wissenslücken und leitet das mehrstufige Entwicklungs-Curriculum eigenständig ab.
3. **GDI – Autonome Entwicklungsintelligenz (`.curriculum` / Ontogenese)**: Mehrstufige
   ontogenetische Entwicklungs-Curricula (`Neonatal` $\rightarrow$ `Juvenile` $\rightarrow$ `Mature`)
   laden (`ontogeny.curriculum`), prüfen und mit `gdi_intelligence.exe` ausführen. Verifizierte
   Funktionen, Methoden und Datentypen früherer Entwicklungsstufen werden im verketteten
   Komponenten-Erfahrungsspeicher (`GDIC1`) abgelegt und in späteren Stufen automatisch als
   Synthesebausteine wiederverwendet.
4. **GDI – Software aus Verhaltensbeispielen (`.behavior` V1/V2)**: Mehrkomponenten- und
   Mehrtyp-Verhaltensdateien öffnen, editieren, Beispiel laden (`geometry.behavior` / `max.behavior`),
   speichern, Zielstruktur (Typen, Module, Methoden, Funktionen, Samples/Checks) prüfen,
   kompositionale verhaltensbasierte Synthese mit `gdi_create.exe` starten, Compiler- und
   Sandbox-Ergebnisse im Live-Protokoll verfolgen und Ergebnisordner öffnen.
5. **GDI – Typisierte Zielarchitektur (`.goal` V1/V2)**: Typisierte Architekturziele
   mit eigenen Datentypen (`struct`, Felder, Methoden), `enum class`, C++20-`concept`s,
   Domänenmodulen und Operationsgraphen öffnen, analysieren und über `gpi_architect.exe`
   in einen Mehrdatei-Projektgraphen überführen und in der Sandbox verifizieren.
6. **GDI – Projekt-Blueprint Synthese (`.blueprint` V1/V2)**: Vollständige
   Mehrdatei-Blueprints inklusive `struct`-Typen, `enum class`, C++20-`concept`s,
   Templates (`requires`), Domänenmodulen (`mod_<module>`) und unabhängigen Tests
   laden, prüfen und über `gpi_synthesize.exe` kompilieren und in der Sandbox testen.
7. **Bisherige integrierte Forschungsmodi**: A–D-Vergleich, Einzelkontext,
   isolierte oder kommunizierende Kompartimente und Erfahrung. Diese Modi
   verwenden weiterhin `gpi_integrated.exe` und die sichere typisierte
   Aufgabenanalyse.

Die Oberfläche erkennt `g++.exe` neben der Anwendung, in typischen
Toolchain-Unterverzeichnissen, im `PATH` oder unter dem bekannten
KR-Toolchain-Pfad. Bei abweichender Installation kann der Pfad ausgewählt werden.
Die `.behavior`-Beispieldatei wird relativ zum Quellprojekt gesucht; ist sie
nicht auffindbar, bietet die GUI ein integriertes Maximum-Beispiel.

**Windows-Start:**

```powershell
# Nach dem Build, beispielsweise aus D:\GENOMIC_C_SYSTEM\KR_BUILD
.\gdi_gui.exe
# Alternativ unverändert kompatibel:
.\gpi_gui.exe
```

Im Modus „Software aus Verhaltensbeispielen“: „Beispiel laden“ →
„Ziel analysieren“ → „Entwicklung starten“. Im rechten Bereich erscheinen
Compiler-/Prüfausgaben und `RESULT=PASS` oder die Fehlermeldung. Mit
„Ergebnisordner öffnen“ gelangt man zum erzeugten C++20-Projekt und zur
Provenienz. Jeder Lauf erhält einen neuen `gdi_run_*`-Ordner.

**Wichtig:** Die `.behavior`-Eingabe ist ein strikt typisiertes Format für
begrenzte ganzzahlige Funktionen, **kein freier natürlichsprachlicher
Softwareauftrag**. „Ziel analysieren“ prüft das Eingabeformat und die
Grundverträge; die eigentliche genomische Suche und die unabhängige
Verifikation erfolgen erst beim Start. Widersprüchliche oder nicht erfüllbare
Beispiele können dabei zu einem gesperrten Lauf führen. Die Windows-Sandbox
ist im neuen Modus verpflichtend; fehlt `gpi_sandbox.exe`, wird nicht gestartet.

Abbruch erfolgt über ein Windows-Jobobjekt für den gestarteten Prozessbaum.
`gdi_create.exe`, `gpi_integrated.exe` und `gpi_sandbox.exe` müssen neben der
GUI liegen. Für eine schnelle Windows-Buildkontrolle:

```powershell
.\gdi_gui.exe --self-test
$LASTEXITCODE  # 0 bedeutet erfolgreich
```

## Allgemeine C++20-Projekte entwickeln

`gpi_develop` lädt ein vorhandenes Projekt aus einem strikten Manifest, bildet es als genomischen Projektgraphen ab und prüft oder repariert es unter einem unveränderten Zielvertrag.

```powershell
.\build\gpi_develop.exe `
  --manifest .\examples\develop_project\project.gpi `
  --project-root .\examples\develop_project `
  --workspace .\generated\develop_run `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --memory .\generated\project.memory `
  --cycles 8
```

Der Beispielquelltext enthält absichtlich einen fehlenden Standard-Header. GDI muss die Ausgangsversion zunächst ablehnen, die Reparatur aus Compiler-Evidenz und genomischen Regeln ableiten und das Ergebnis anschließend unabhängig in der Sandbox bestätigen.

## Neue Projekte aus einem Blueprint synthetisieren

`gpi_synthesize` liest typisierte Funktionssignaturen, Parameter, Ausdrücke, Einstiegspunkt und unabhängige Tests. Genomische Strukturregeln autorisieren daraus die Erzeugung eines vollständigen Mehrdatei-Projektgraphen. Eine Sandbox ist für diese CLI verpflichtend.

```powershell
.\build\gpi_synthesize.exe `
  --blueprint .\examples\blueprint_fibonacci\project.blueprint `
  --workspace .\generated\fibonacci_run `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --memory .\generated\blueprint.memory `
  --cycles 8
```

Das Beispiel beschreibt nur eine rekursive Fibonacci-Funktion, ihren Einstieg und drei semantische Tests. GDI erzeugt daraus Header, Implementierung, ausführbare Anwendung, Testprogramm und Buildmanifest. Der externe Compiler und die Sandbox-Verhaltensverträge prüfen das Ergebnis.

## Architektur aus einem typisierten Ziel entwickeln

`gpi_architect` trennt die Anforderung von ihrer C++20-Implementierung. Eine Zieldatei beschreibt Funktionen, Parameter und einen gerichteten Graph elementarer Operationen. GDI prüft Datentypen, Operandenanzahl, Referenzen und Zyklen, ermittelt Funktionsabhängigkeiten und komponiert daraus den Projekt-Blueprint. Nur im unveränderlichen Genom freigegebene Kompositionsoperatoren dürfen verwendet werden.

```powershell
.\build\gpi_architect.exe `
  --goal .\examples\typed_factorial\factorial.goal `
  --workspace .\generated\factorial_run `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --memory .\generated\architecture.memory `
  --cycles 8
```

Das Fakultätsbeispiel enthält keinen fertigen C++-Ausdruck. Die rekursive Funktion entsteht aus Vergleich, Subtraktion, rekursivem Aufruf, Multiplikation und bedingter Auswahl. Das zweite Beispiel `typed_components` weist die automatische Zusammensetzung zweier abhängiger Funktionen nach. Erwartete Einstiegsergebnisse und unabhängige Fälle bleiben Teil des unveränderlichen Zielvertrags; jede Ausführung erfolgt verpflichtend in der Sandbox.

## Zentrale Programme

| Programm | Aufgabe |
| --- | --- |
| `gpi_gui` / `gdi_gui` | native Windows-Oberfläche für ontogenetische Entwicklungsintelligenz, `.behavior`, `.goal`, `.blueprint` und Forschung |
| `gdi_intelligence` | eigenständig entwicklungsfähige, genomisch organisierte Programmierintelligenz (`.curriculum`, Ontogenese & Erfahrungsspeicher) |
| `gdi_create` | verhaltensbasierte Mehrkomponenten- und Mehrtyp-Synthese (`.behavior` V1/V2) |
| `gpi_integrated` | Zielvertrag, Kompartimente, Forschung und A–D-Architekturvergleich |
| `gpi_develop` | allgemeine manifestbasierte Mehrdatei-Projekte, Reparatur und Erfahrungstransfer |
| `gpi_synthesize` | genomisch autorisierte Mehrdatei-Projektsynthese aus typisierten Blueprints |
| `gpi_architect` | typisierte Zielgraphen in Komponentenarchitektur, Projekt und Verifikation entwickeln |
| `gpi_research` | begrenzte Hypothesen-/Experimentzyklen mit Evidenz |
| `gpi_project` | natürlichsprachlich erzeugte Mehrdatei-Projekte im typisierten Aufgabenraum |
| `gpi_agent` | typisierte Ausdruckssynthese mit Compiler-/Verhaltensprüfung |
| `gpi_sandbox` | Windows-AppContainer-Ausführungsdienst |
| `genomic_parse` | genomisch konfigurierte Parser-/AST-Inspektion |

## Sicherheits- und Vertrauensgrenzen

- Der externe Compiler wird nicht modifiziert.
- Prozesse werden ohne Shell-Interpolation mit festen Argumentlisten gestartet.
- Zeitlimits und Prozessbaum-Beendigung begrenzen Compiler- und Programmläufe.
- Projekt- und Manifestpfade dürfen den vorgesehenen Wurzelordner nicht verlassen; aufgelöste Symlink-Ziele werden ebenfalls geprüft.
- Eine verlangte Sandbox darf nicht stillschweigend durch unisolierte Ausführung ersetzt werden.
- Kompiliererfolg allein gilt nie als Zielerfüllung.
- FNV-1a/CRC und die Erfahrungskette erkennen versehentliche oder einfache nachträgliche Änderungen, sind aber keine kryptografische Signatur.

## Wichtige fachliche Grenze

GDI besitzt eine verlustfreie Repräsentation des gesamten C++20-Quellraums und kann solche Projekte an einen konformen Compiler übergeben. Seine eigene Parser-/Semantikschicht bildet bewusst nur einen geprüften Teil der ISO-C++20-Semantik nach. Vollständige Sprachsemantik wird nicht behauptet; für allgemeine Projekte sind der externe Compiler und die unabhängigen Verhaltensverträge die maßgebliche Instanz.

Die ausführliche Architektur-, Manifest-, Sicherheits- und Verifikationsdokumentation steht in [DOCUMENTATION.md](DOCUMENTATION.md).

### Behavioral program development (extension)

`gdi_create` can infer a small typed C++20 function from integer input/output
examples, without a prewritten operations graph. See
`examples/behavioral_max/max.behavior`, `examples/behavioral_square/square.behavior`,
`examples/behavioral_absolute/absolute.behavior` and the *Behavioral induction*
section in `DOCUMENTATION.md`. This bounded feature is not general free-text
software creation; it reuses the immutable genome, typed architecture generator,
project development, and existing sandbox contract.
