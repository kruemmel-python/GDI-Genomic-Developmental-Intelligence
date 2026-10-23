# Technische Gesamtdokumentation

## 1. Systemziel

GDI untersucht, wie sich Programmierfähigkeit aus einem unveränderlichen linearen Genom, regulatorischen Zuständen, Entwicklung und überprüfter Erfahrung entfalten kann. Das System trennt drei Dinge ausdrücklich:

1. die ursprüngliche Anforderung,
2. die intern akzeptierte Interpretation als Zielvertrag,
3. das tatsächlich gemessene Verhalten des erzeugten Programms.

Nur wenn alle erforderlichen Prüfungen erfolgreich sind, gilt ein Entwicklungsartefakt als verifiziert.

## 2. Unveränderliches Genom

`GenomeStrand` enthält Regionen für Grammatik, Semantik, Fähigkeiten, regulatorische Regeln und generative Quelloperationen. Nach `freeze()` sind Änderungen verboten. Ein stabiler Genom-Hash wird durch alle Entwicklungs-, Verifikations- und Erfahrungsartefakte getragen.

Lernen verändert nicht die Genomsequenz. Es verändert ausschließlich:

- lokale Molekülkonzentrationen,
- regulatorische Zugänglichkeit,
- Entwicklungszustände,
- externe, verifizierte Erfahrung.

## 3. C++20-Repräsentation

Das System besitzt zwei sich ergänzende Ebenen.

### 3.1 Vollständige genomisch gesteuerte C++20-Sprach- und Semantikebene (Fixpunkt)

Lexer, Präprozessor, Parser, Typanalyse, Klassen, Templates, Concepts, Overload-Auflösung, Lebensdauer-/RAII-Analyse, `constexpr`-Auswertung, Laufzeitmerkmale und Speicherregeln bilden das vollständige C++20-Regelwerk für GDI ab. Alle Regeln werden aus den eingefrorenen Genomregionen (`CppGrammar`, `TypeSystem`, `CppConversionRule`, `CppLookupRule`, `CppOverloadRule`, `CppSemanticRule`, `CppConstraintRule`, `CppLifetimeRule`, `CompilerLogic`) geladen:

1. **Eigenständiger Syntax-, Semantik-, `constexpr`- und IR/VM-Pfad für `switch`-Anweisungen**:
   - **Genom & Parser (`Cpp20Parser::parseSwitchStmt`)**: Unterstützt `switch (cond)`, C++17/20-Initialisierungsanweisungen `switch (init; cond)`, `case expr:`, `default:`, `[[fallthrough]];`, `break` sowie verschachtelte Blöcke.
   - **Semantische Prüfung (`SemanticAnalyzer::checkSwitchStmt`)**: Prüft integrale oder `enum`-Bedingungstypen, wertet `case`-Labels als Compile-Time-Konstanten (`evaluateConstexprInt`) aus, weist doppelte `case`-Werte und mehrfache `default`-Labels fehlersicher zurück und überwacht die Gültigkeit von `break`/`continue`.
   - **`constexpr`-Engine & `GenomeCompiler` / `GenomeVm`**: Führt `switch`-Anweisungen sowohl zur Compile-Zeit (`ConstexprEngine::executeStatement`) als auch im IR-Bytecode (`GenomeCompiler::compileStatement`) inklusive C++-konformem Fallthrough und `break`-Sprungzielen deterministisch aus.
   - **Generative Synthese (`GoalArchitect` & `induceArchitectureGoal`)**: Unterstützt `switch` als genomisch autorisierten Kompositionsoperator (`switch-dispatch`) sowohl in `.goal`-Spezifikationen als auch bei der verhaltensbasierten Induktion aus Ein-/Ausgabebeispielen (`.behavior`).
2. **Vollständige C++20-Typanalyse (`CppType`, `TypeSystem`, `Cpp20Parser::parseType`)**:
   - **Typspezifizierer**: Unterstützt alle mehrwortigen und vorzeichenbehafteten/vorzeichenlosen C++20-Integraltypen (`unsigned long long`, `long int`, `unsigned short`, `signed char`, `char8_t`, `char16_t`, `char32_t`, `wchar_t`, `int8_t`..`uint64_t`, `size_t`, `ptrdiff_t`, `nullptr_t`), führende und nachgestellte `const`/`volatile`-Qualifizierer, Zeiger (`T*`, `const T* const`), LValue-Referenzen (`T&`), RValue-/Forwarding-Referenzen (`T&&`), Arrays (`T[N]`), Funktionszeiger und qualifizierte Template-Spezialisierungen (`ns::Type<Arg1, Arg2>`).
   - **Referenzkollabierung & Transformationen**: Implementiert die C++20-Referenzkollabierungsregeln (`& + & -> &`, `& + && -> &`, `&& + & -> &`, `&& + && -> &&`) über `CppType::collapseReference` sowie `removeReference()`, `removeCV()`, `removeCVRef()`, `decay()` und `withCV()`.
3. **Vollständige C++20-Template-Deduktion, CTAD, Partielle Ordnung & SFINAE (`TemplateEngine`)**:
   - **Rekursive Musterdeduktion (`deduceFromTypePattern`, `deduceAndSubstituteFunctionTemplate`)**: Deduziert beliebig viele Typ- und Non-Type-Template-Parameter aus komplexen Parametermustern (`const T*`, `T&`, `T[N]`, `Record<T, U>`), prüft Konsistenz bei mehrfach auftretenden Parametern und unterstützt explizite sowie Default-Template-Argumente.
   - **Forwarding-Referenzen (`T&&`)**: Deduziert bei LValue-Argumenten `T = U&` (mit Referenzkollabierung des Parameters zu `U&`) und bei RValue-Argumenten `T = U` (Parameter `U&&`).
   - **Parameter-Packs (`Ts...`) & Non-Deduced Contexts (`std::type_identity_t<T>`)**: Deduziert variadische Argumentlisten und schließt nicht-deduzierbare Kontexte (`isNonDeduced`) von der Deduktion aus, bevor sie nachgelagert substituiert werden.
   - **Partielle Ordnung (`compareFunctionTemplateSpecialization`) & CTAD (`deduceClassTemplateArguments`)**: Bestimmt über wechselseitige synthetische Typdeduktion das spezialisiertere Funktions- oder Klassentemplate (z. B. `f(T*)` spezieller als `f(T)`) und löst Class Template Argument Deduction (CTAD) über Deduktionsguides auf.
4. **Vollständige C++20-Constraints & Concept-Normalisierung / Subsumption (`normalizeConstraint`, `isMoreConstrained`)**:
   - Zerlegt zusammengesetzte Concept-Ausdrücke mit Konjunktionen (`&&`) und Disjunktionen (`||`) rekursiv in die Disjunktive Normalform (`NormalizedConstraint`: Disjunktion von Konjunktionsklauseln atomarer Constraints `AtomicConstraint`).
   - Entscheidet Subsumption nach ISO C++20 `[temp.constr.order]`: Constraint $A$ subsumiert Constraint $B$ genau dann, wenn jede Konjunktionsklausel der DNF von $A$ mindestens eine Konjunktionsklausel der DNF von $B$ logisch impliziert (inklusive `requires`-Ausdrucksauswertung über `evaluateRequiresExpression`).
5. **Vollständige C++20-Overload-Auflösung (`SemanticAnalyzer::resolveOverload`, `computeImplicitConversionSequence`)**:
   - Berechnet für jedes Argumentpaar die exakte `ImplicitConversionSequence` mit den fünf C++20-Rängen (`ExactMatch` < `Promotion` < `StandardConversion` < `UserDefinedConversion` < `EllipsisConversion`).
   - Berücksichtigt Wertkategorien (`LValue`, `PRValue`, `XValue`) bei der Referenzbindung (RValues bevorzugen `T&&` vor `const T&` und lehnen nicht-konstante `T&` ab; nicht-konstante LValues bevorzugen `T&` vor `const T&`) sowie Qualifikationsanpassungen (`T*` vor `const T*`).
   - Vollständige C++20-Tie-Breaker-Kaskade: Argument-Dominanz $\rightarrow$ Nicht-Template vor Template $\rightarrow$ partielle Template-Spezialisierungsordnung $\rightarrow$ Concept-Subsumption (`isMoreConstrained`), sowie C++20-Synthese für `<=>` und `==`.
6. **Vollständige C++20-Lebensdauer-, RAII-, Temporary-Materialization- & Borrow/Move-Analyse (`LifetimeEvent`)**:
   - **Deterministische RAII-Destruktion**: Protokolliert Konstruktion (`ConstructLocal`) und Destruktion (`DestroyLocalAtScopeExit`, `DestroyExtendedTemporaryAtScopeExit`) in strikt umgekehrter Deklarationsreihenfolge beim Verlassen jedes Blocks sowie über alle aktiven Funktionsblöcke bei vorzeitigem `return`.
   - **Temporary Materialization & Lifetime Extension**: Modelliert die implizite Umwandlung `PRValue` $\rightarrow$ `XValue` (`MaterializeTemporary`) und verlängert die Lebensdauer bei Bindung an `const T&` oder `T&&` (`ExtendTemporaryLifetime`) auf den Gültigkeitsbereich der Referenz.
   - **Dangling-Reference-/Pointer- & Move-Schutz**: Verhindert das Zurückgeben von Referenzen oder Zeigern auf lokale automatische Variablen oder temporäre Objekte bei `return`, verfolgt `std::move(x)` (`MoveFromObject`) und lehnt Lesezugriffe auf verschobene (`use-after-move`) oder uninitialisierte Variablen fehlersicher ab.

### 3.2 Verlustfreie Quellebene

`Cpp20SourceGenome` kodiert alle 256 möglichen Bytewerte verlustfrei. Dadurch lassen sich beliebige Text- und Binärinhalte eines C++20-Projekts exakt als `GenomicSourceArtifact` speichern und wiederherstellen. Jedes Artefakt trägt:

- relativen Pfad,
- Genom-Hash,
- generative Regelregion,
- Quell-Hash,
- genomisch kodierte Nutzlast.

Die Quellebene behauptet keine eigene vollständige ISO-Semantik. Sie stellt den vollständigen Quellraum dar; ein externer C++20-Compiler und unabhängige Verhaltensverträge entscheiden über Übersetzbarkeit und Verhalten.

## 4. Zielvertrag

Ein `GoalContract` enthält das Originalziel, normalisierte Absicht, Ein-/Ausgabetypen, erforderliche Operationen, ungeklärte Anforderungen, Widersprüche und einen stabilen semantischen Hash. Unbekannte oder widersprüchliche Anforderungen werden nicht stillschweigend umgedeutet.

Für allgemeine Projekte beschreibt `ProjectVerificationContract`:

- zu bauende Targets,
- Include-Wurzeln,
- Verhaltensfälle,
- exakte Standardeingabe,
- erwartete Standardausgabe und Standardfehlerausgabe,
- erwarteten Exit-Code,
- Zeitlimit,
- Sandbox-Pflicht.

## 5. Generativer Projektgraph

`GenerativeProjectGraph` repräsentiert:

- Header,
- Quelldateien,
- Modul-Interfaces,
- Modul-Implementierungen,
- Tests,
- Ressourcen,
- Build-Manifeste.

Typisierte Kanten unterscheiden `includes`, `imports`, `links`, `tests`, `generates` und `uses-resource`. Ungültige Rollenkombinationen, doppelte Kanten, unbekannte Knoten und Zyklen werden abgelehnt. Targets können Bibliotheken, Programme oder Tests sein. Ihre transitiven Abhängigkeiten werden deterministisch aufgelöst.

Der Graph-Hash ist von der Einfügereihenfolge unabhängig und bindet Genom, Knotentypen, Pfade, Quellen, Kanten und Targets.

## 6. Projektmanifest

`gpi_develop` verwendet ein tabulatorgetrenntes UTF-8-Manifest. Die erste wirksame Zeile muss lauten:

```text
GPI_PROJECT_CONTRACT_V1
```

Unterstützte Datensätze:

```text
node<TAB>ID<TAB>KIND<TAB>RELATIVER_PFAD
dependency<TAB>CONSUMER<TAB>PROVIDER<TAB>KIND
target<TAB>NAME<TAB>KIND<TAB>KNOTEN_ID[,KNOTEN_ID...]
build<TAB>TARGET_NAME
include<TAB>RELATIVER_PFAD
behavior<TAB>NAME<TAB>TARGET<TAB>STDIN_DATEI|-<TAB>STDOUT_DATEI|-<TAB>STDERR_DATEI|-<TAB>EXIT_CODE
timeout-seconds<TAB>1..60
require-sandbox<TAB>true|false
```

Knotentypen:

```text
header source module-interface module-implementation test resource build-manifest
```

Abhängigkeitstypen:

```text
includes imports links tests generates uses-resource
```

Targettypen:

```text
library executable test
```

Alle Knoten- und Fixture-Dateien werden bytegenau aus der Projektwurzel geladen. Absolute Pfade, `..`-Ausbrüche und aufgelöste Symlink-Ausbrüche werden abgelehnt. Leere Fixture-Inhalte werden mit `-` angegeben.

## 7. Projekt-Blueprint (V1 & V2)

`gpi_synthesize` erzeugt einen neuen Projektgraphen, statt vorhandene C++-Dateien zu importieren. Die Eingabe beginnt mit `GPI_PROJECT_BLUEPRINT_V1` oder `GPI_PROJECT_BLUEPRINT_V2`.

Unterstützte Datensätze in V1 und V2:

```text
GPI_PROJECT_BLUEPRINT_V2
project<TAB>PROJEKTNAME
namespace<TAB>QUALIFIZIERTER_NAMESPACE
module<TAB>MODULNAME
enum<TAB>NAME<TAB>WERT_1<TAB>WERT_2...
concept<TAB>NAME<TAB>TYPPARAMETER<TAB>CONSTRAINT_AUSDRUCK
type<TAB>TYPNAME
field<TAB>TYPNAME<TAB>FELDTYP<TAB>FELDNAME
method<TAB>TYPNAME<TAB>METHODENNAME<TAB>RÜCKGABETYP<TAB>AUSDRUCK[<TAB>PARAMETER:TYP...]
function<TAB>NAME<TAB>RÜCKGABETYP<TAB>AUSDRUCK[<TAB>module:MODUL][<TAB>template:T][<TAB>requires:CONCEPT]
parameter<TAB>FUNKTIONSNAME<TAB>TYP<TAB>NAME
entry<TAB>TYP<TAB>AUSDRUCK
test<TAB>NAME<TAB>BOOLSCHER_AUSDRUCK
include<TAB>STANDARD_HEADER
input<TAB>FIXTURE_DATEI|-
expected-output<TAB>FIXTURE_DATEI|-
expected-error<TAB>FIXTURE_DATEI|-
expected-exit<TAB>EXIT_CODE
timeout-seconds<TAB>1..60
require-sandbox<TAB>true|false
```

Der Parser validiert Bezeichner, Typfragmente, eindeutige Typen/Funktionen/Parameter/Tests, ausbalancierte Klammern und geschweifte Klammern (für C++20-Aggregatinitialisierung wie `Point2D{3, 4}`) sowie Fixture-Pfade. Semikolons und Präprozessoranweisungen sind in Ausdrucksfeldern verboten. `gpi_synthesize` erzwingt unabhängig vom Dateiwert die Sandbox.

Die Synthese ist nur möglich, wenn das eingefrorene Genom die benötigten Strukturen (`typed-functions`, `header-source-executable-test`, `independent-behavior-tests`, sowie bei V2-Merkmalen `user-defined-types`, `enum-types`, `type-methods`, `multi-module-components`, `cpp20-concepts`, `cpp20-templates`) freigibt. Sie erzeugt deterministisch:

- `include/generated/api.hpp` (mit Typen, Enums, Concepts, Templates und Modul-Includes),
- für jedes Domänenmodul `M`: `include/generated/M.hpp`, `src/M.cpp` und das Bibliotheks-Target `mod_M`,
- `src/generated.cpp`,
- `src/main.cpp`,
- `tests/generated_test.cpp`,
- `CMakeLists.txt`,
- die Targets `core`, `mod_*`, `app` und `behavior`,
- zwei unabhängige Verhaltensverträge für Anwendung und Tests.

## 8. Typisierte Zielarchitektur (V1 & V2)

`gpi_architect` setzt vor dem Projekt-Blueprint eine unabhängige Anforderungsebene ein. Das Format beginnt mit `GPI_TYPED_ARCHITECTURE_GOAL_V1` oder `GPI_TYPED_ARCHITECTURE_GOAL_V2`.

Die wesentlichen Datensätze sind:

```text
GPI_TYPED_ARCHITECTURE_GOAL_V2
project<TAB>NAME
namespace<TAB>NAME
module<TAB>MODULNAME
enum<TAB>NAME<TAB>WERT_1<TAB>WERT_2...
concept<TAB>NAME<TAB>TYPPARAMETER<TAB>CONSTRAINT_AUSDRUCK
type<TAB>TYPNAME
field<TAB>TYPNAME<TAB>FELDTYP<TAB>FELDNAME
method<TAB>TYPNAME<TAB>METHODENNAME<TAB>RÜCKGABETYP
method-param<TAB>TYPNAME<TAB>METHODENNAME<TAB>TYP<TAB>NAME
method-op<TAB>TYPNAME<TAB>METHODENNAME<TAB>ERGEBNIS<TAB>OPERATOR<TAB>OPERAND...
method-return<TAB>TYPNAME<TAB>METHODENNAME<TAB>ERGEBNIS
function<TAB>NAME<TAB>RÜCKGABETYP[<TAB>module:MODUL][<TAB>template:T][<TAB>requires:CONCEPT]
parameter<TAB>FUNKTION<TAB>TYP<TAB>NAME
operation<TAB>FUNKTION<TAB>ERGEBNIS<TAB>OPERATOR<TAB>OPERAND...
return<TAB>FUNKTION<TAB>ERGEBNIS
entry<TAB>FUNKTION<TAB>ERWARTET<TAB>ARGUMENT...
case<TAB>NAME<TAB>FUNKTION<TAB>ERWARTET<TAB>ARGUMENT...
timeout-seconds<TAB>1..60
require-sandbox<TAB>true|false
```

Ganzzahlliterale beginnen mit `#`, boolesche Literale sind `true` und `false`, Aggregatliterale für eigene Datentypen haben die Form `Typname{#1,#2}`. Verfügbare Kompositionen umfassen neben Kopie, Arithmetik (`add`, `sub`, `mul`, `div`, `mod`, `neg`, `abs`, `min`, `max`, `clamp`), Bitoperationen (`bitwise-and`, `bitwise-or`, `bitwise-xor`, `shift-left`, `shift-right`), Vergleichen, booleschen Operationen und `select` auch:

- `construct:Typname` (Konstruktion eines benutzerdefinierten Datentyps aus Feldwerten),
- `field:Feldname` (typisierter Feldzugriff auf eine Strukturinstanz oder `self` in Methoden),
- `method:Methodenname` (typisierter Methodenaufruf auf einer Strukturinstanz),
- `call:Funktionsname` (typisierter Aufruf einer anderen Funktion im selben oder einem anderen Modul).

Diese Menge steht im eingefrorenen Genom unter `composition-operators`; ein unbekannter Operator wird vor der Quellerzeugung abgelehnt. Der Architekt prüft Feldanzahl, Feldtypen, Methodensignaturen, Zyklenfreiheit und Funktions-/Modulabhängigkeiten und überführt das Ziel in einen `ProjectBlueprint`.

`architecture.json` hält den deterministischen semantischen Ziel-Hash, die abgeleiteten Komponenten, Operationsanzahl, Funktionsabhängigkeiten, den Einstieg und die Zahl unabhängiger Fälle fest. Widersprüchliche Erwartungen für denselben Funktionsaufruf werden abgelehnt. Der Zielvertrag bleibt während Reparatur- und Erfahrungstransferzyklen unverändert. Die CLI erzwingt eine vorhandene Sandbox, auch wenn eine Eingabedatei etwas anderes verlangt.

Die End-to-End-Nachweise umfassen:

- rekursive Fakultät, aus fünf Operationen statt aus einem fertigen C++-Ausdruck entwickelt,
- zwei automatisch verbundene Komponenten `square` und `sum_squares`,
- unabhängige Fälle zusätzlich zur erwarteten Programmausgabe,
- Ablehnung eines zyklischen Operationsgraphen,
- reale Kompilierung mit dem externen unveränderten C++20-Compiler und sandboxierte Ausführung.

Das Format ist eine sichere, deterministische Zwischenrepräsentation. Es behauptet nicht, beliebige freie natürliche Sprache ohne Rückfragen eindeutig in einen solchen Vertrag übersetzen zu können.

## 9. Verifikation

`verifyGenerativeProject` materialisiert den Graphen in einen neuen leeren Arbeitsordner und führt kontrollierte Compilerprozesse aus. Bibliotheksziele werden syntaktisch geprüft; Programm- und Testziele werden gelinkt. Danach werden alle Verhaltensfälle ausgeführt.

Ein `ProjectVerificationEvidence`-Bericht enthält:

- Genom-Hash,
- Graph-Hash,
- Erfolg des Gesamtvertrags,
- jeden Compile-/Verhaltenscheck,
- Exit-Code,
- Timeoutstatus,
- Sandboxstatus,
- Laufzeit,
- Diagnosepfade.

Eine negative Orakelprüfung im End-to-End-Test beweist, dass ein erfolgreich kompiliertes Programm mit falschem Verhalten abgelehnt wird.

## 10. Autonome Entwicklungszyklen

`runAutonomousProjectRepair` beginnt immer mit dem unveränderten Projekt. Aus der ersten fehlgeschlagenen Evidenz wählt es ausschließlich genomisch freigegebene Operatoren. Implementiert und end-to-end geprüft sind:

- Standard-Header aus einer echten Fehlendeclarations-Diagnose ergänzen,
- einen zuvor nicht eingebundenen Provider anhand von Linkersymbolen verbinden,
- einen numerischen Stringliteral in einen typisierten Zahlenliteral umwandeln,
- Zahlenliterale anhand beobachteter Ausgabedifferenzen kalibrieren,
- bei fehlender numerischer Ausgabe eine begrenzte lokale Ganzzahlnachbarschaft durchsuchen.

Jeder Kandidat wird erneut vollständig kompiliert und verhaltensgeprüft. Der Entwicklungszyklus besitzt ein festes Budget von höchstens 16 Versuchen. Nicht erkannte Fehler werden nicht willkürlich umgeschrieben.

`runCumulativeAutonomousProjectRepair` erweitert diesen Ablauf um persistente Entwicklungszustände. Ein Zwischenstand wird nur übernommen, wenn die messbare Verifikationsfront voranschreitet: von fehlender Deklaration über Typprüfung und Linken bis zur Verhaltensprüfung. Graph-Hashes verhindern die Wiederholung bereits versuchter Zustände. Das End-to-End-Szenario weist eine Kette aus Header-Ergänzung, typisierter Stringliteral-Korrektur und Link-Provider-Erkennung in vier Verifikationszyklen nach. Alle tatsächlich ausgeführten Verifikationen zählen gegen dasselbe feste Zyklusbudget.

## 11. Erfahrung und Transfer

Erfolgreiche Reparaturen werden als `VerifiedProjectTransformation` gespeichert. Ein Datensatz bindet:

- vollständigen Genom-Hash,
- Ursprungsgraph-Hash,
- verifizierten Graph-Hash,
- Transformationstyp,
- Zielknoten und Hilfsdaten,
- Vorher-/Nachher-Token,
- Hash des ursprünglichen Evidenzberichts.

Der Speicher ist append-only und hashverkettet. Mehrteilige Entwicklungserfahrungen werden als ein einzelner `GPIG2`-Gruppendatensatz geschrieben, damit kein lesbarer Teilzustand zwischen zwei Records entstehen kann. Ältere `GPIT1`-Einzelrecords bleiben lesbar. Beim Laden werden Format, Gruppengröße, Kette, gemeinsame Provenienz und transformationsspezifische Pflichtfelder geprüft. Eine Transformation eines anderen Genoms wird abgelehnt.

Transfer gilt erst als erfolgreich, wenn die gesamte gespeicherte Transformationsgruppe auf einen verwandten neuen Graphen anwendbar ist und das Ergebnis den neuen Zielvertrag unabhängig besteht. Regeln dürfen dabei strukturell generalisieren: Eine belegte Umwandlung eines eindeutigen numerischen Stringliterals wird beispielsweise auf einen anderen eindeutigen Zahlenwert angewandt, anstatt den alten Wert zu kopieren. Mehrdeutige Kontexte werden abgelehnt. Die frühere Evidenz ersetzt niemals die neue Prüfung.

## 12. Multi-Kompartiment-Regulation

Analyse, Synthese und Forschung arbeiten logisch getrennt mit demselben Genom. Jedes Kompartiment besitzt lokale Moleküle, zugängliche Regionen und Erfahrungserlaubnisse. Typisierte Signale verlangen zulässige Sender-/Empfängerrollen und einen nichtleeren Zielvertrag-Hash.

Der Molekülpool verwendet einen strukturierten Schlüssel aus vollständiger Kompartiment-ID und vollständiger 64-Bit-Signatur; Signaturen mit gleichen unteren 48 Bit bleiben verschieden.

Der A–D-Vergleich misst mit demselben Ziel und Genom:

- einen Einzelkontext,
- isolierte Kompartimente,
- kommunizierende Kompartimente,
- kommunizierende Kompartimente mit verifizierter Erfahrung.

## 13. Forschungsentscheidungen

Eine fehlende gespeicherte Komponente ist nicht automatisch eine fehlende Fähigkeit. GDI misst zunächst einen Basiskandidaten. Die Diagnose unterscheidet vorhandene wiederverwendbare Fähigkeit, ohne Forschung lösbare Aufgabe, messbare Optimierungsmöglichkeit und fehlende ausführbare Fähigkeit.

Hypothesen werden kompiliert, durch unabhängige Fixtures geprüft und anhand aufgezeichneter Evidenz verglichen. Forschungsziele dürfen keine unbegrenzte Selbstmodifikation auslösen; Genom, Zyklusbudget, Dateigrenzen und Ausführungsgrenzen bleiben invariant.

## 14. Sandbox

Unter Windows startet `gpi_sandbox` den Kandidaten in einem AppContainer:

- kein deklarierter Netzwerkzugriff,
- eigener Container-Speicher,
- nur Kandidat und erforderliche Laufzeit-DLLs werden bereitgestellt,
- Job-Objekt beendet den gesamten Prozessbaum,
- festes Timeout,
- keine unisolierte Rückfallausführung, wenn Sandboxpflicht gesetzt ist.

Die Sandbox reduziert das Risiko unbekannten Programmcodes, ist aber kein Beweis absoluter Betriebssystemsicherheit. Betriebssystem- und Compileraktualisierungen bleiben Teil des Vertrauensankers.

## 14. Programme und Bibliotheksmodule

Die öffentliche Bibliothek liegt unter `include/`, die Implementierung unter `src/`. Wichtige Module:

| Modul | Zweck |
| --- | --- |
| `genome/` | Strang, Regionen, Decoder, Integrität und Bootstrap |
| `regulation/` | Moleküle und regulatorische Auflösung |
| `cpp20/` | interpretierte Sprachebene und exakte Quellebene |
| `gpi/generative_project` | allgemeiner typisierter Projektgraph |
| `gpi/project_manifest` | sicherer Dateiimport und Zielvertrag |
| `gpi/project_blueprint` | typisierte generative Programmbeschreibung und Mehrdatei-Synthese |
| `gpi/project_verifier` | Compiler-, Linker-, Sandbox- und Verhaltensprüfung |
| `gpi/autonomous_project_developer` | evidenzgeleitete Entwicklungszyklen |
| `gpi/project_experience` | persistente Transformationen und Transfer |
| `gpi/integrated_development` | Zielvertrag, Kompartimente und Vergleich |
| `runtime/` | begrenzte IR-/VM-Ausführung |

## 15. Tests

CTest führt neun Tests aus:

1. Kern-Unit-Tests der GPI-Pipeline,
2. natürlichsprachliche Mehrdatei-Projekte,
3. umfassende Genom-/C++-/Regulationsregression,
4. Prompt-End-to-End,
5. Agent-End-to-End,
6. Forschung, Gedächtnis und Manipulationsablehnung,
7. integrierter Zielvertrag und A–D-Vergleich,
8. GUI-Selbsttest,
9. allgemeiner Projektgraph, Manifest, externe Kompilierung, Sandbox, Reparatur, Erfahrungslernen und Transfer.

Der aktuelle Windows-Referenzlauf besteht alle neun Tests. Da mehrere Tests echte Compiler- und Sandbox-Prozesse starten, ist eine Laufzeit von mehreren Minuten normal.

## 16. Reproduzierbarkeit und Veröffentlichung

Das Developer-Paket enthält ausschließlich:

- `apps/`, `include/`, `src/`, `tests/`, `examples/`,
- `CMakeLists.txt`,
- `README.md`,
- `DOCUMENTATION.md`,
- `LICENSE`.

Build-Verzeichnisse, erzeugte Programme, Laufprotokolle, Gedächtnisdateien, lokale Compiler und historische Entwicklungsnotizen gehören nicht in das Paket.

## Behavioral induction (GDI V1 & V2 development extension)

`gdi_create` adds a bounded **behavior-to-architecture** route (`GDI_BEHAVIORAL_GOAL_V1` and `GDI_BEHAVIORAL_GOAL_V2`). Unlike `gpi_architect`, its input does not contain an operation graph. The developer supplies types, methods, function signatures, modules, and observed input/output examples; GDI enumerates genome-authorized arithmetic, comparison, selection, field-access, type-construction, method-invocation, and function-composition expressions, induces a typed dependency graph across types and functions, and passes it through `architectTypedGoal` and the autonomous project verification pipeline.

### Compositional Multi-Component & Custom-Type Induction (`GDI_BEHAVIORAL_GOAL_V2`)

In `GDI_BEHAVIORAL_GOAL_V2`, a single `.behavior` specification can define:
- **Domain modules** (`module <name>`), emitted into separate headers (`include/generated/<module>.hpp`), sources (`src/<module>.cpp`), and library targets (`mod_<module>`).
- **Enums and C++20 Concepts** (`enum`, `concept`).
- **User-defined data types** (`type`, `field`, `method`, `method-param`, `method-depth`, `method-sample`, `method-check`), where struct methods are induced first from `self` field accesses and parameters.
- **Multiple cohesive functions** (`function`, `parameter`, `max-depth`, `sample`, `check`), induced in topological order so that earlier synthesized functions, struct constructors (`construct:Type`), field accessors (`field:f`), and struct methods (`method:m`) become genome-authorized candidate building blocks for subsequent functions.

Example (`examples/behavioral_geometry/geometry.behavior`):

```text
GDI_BEHAVIORAL_GOAL_V2
project	developed_geometry
namespace	generated
module	math_ops
module	geom
enum	Axis	Horizontal	Vertical
concept	IntegralMetric	T	std::integral<T>
type	Point2D
field	Point2D	long long	x
field	Point2D	long long	y
method	Point2D	norm1	long long
method-depth	2
method-sample	3,4	7
method-sample	0,5	5
method-check	6,8	14
function	square	long long	module:math_ops
parameter	v	long long
max-depth	2
sample	3	9
sample	-4	16
check	6	36
function	point_energy	long long	module:geom
parameter	p	Point2D
max-depth	3
sample	3,4	25
sample	1,2	5
check	6,8	100
entry	point_energy
```

Fields are **tab-separated**. Aggregate values for custom types are written as comma-separated field tuples (`3,4` or `Point2D{3,4}`). `sample` / `method-sample` observations drive synthesis; `check` / `method-check` observations are withheld from synthesis and must all pass the *first* fitting candidate. GDI writes `induction.json`, `inferred.goal`, `architecture.json`, and the multi-module C++20 project artifacts.

## 17. Eigenständig entwicklungsfähige, genomisch organisierte Programmierintelligenz (`gdi_intelligence`)

Mit `gdi_intelligence` (`include/gpi/developmental_intelligence.hpp`, `src/gpi/developmental_intelligence.cpp`) vereint GDI sämtliche Schichten des Projekts zu einer **eigenständig entwicklungsfähigen, genomisch organisierten Programmierintelligenz**:

1. **Unveränderliches Genom (`GenomeStrand` & `Cpp20SourceGenome`)**: Alle zulässigen C++20-Projektstrukturen (`user-defined-types`, `type-methods`, `multi-module-components`, `cpp20-concepts`, `cpp20-templates`), Kompositionsoperatoren und Reparaturregeln stammen ausschließlich aus dem eingefrorenen Genom. Nach jeder Entwicklungsstufe wird die Unveränderlichkeits-Invariante (`getGenomeHash() == initialGenomeHash && isFrozen()`) hart geprüft.
2. **Molekulare Regulation & Ontogenetische Stufen (`RegulatoryResolver` & `PhenotypeBuilder`)**: Ein Entwicklungs-Curriculum (`GDI_DEVELOPMENTAL_CURRICULUM_V1`, `.curriculum`) gliedert komplexe Softwareentwicklung in ontogenetische Reifungsstufen (`Neonatal` $\rightarrow$ `Juvenile` $\rightarrow$ `Mature`). Vor jeder Stufe prüft `RegulatoryResolver::resolve` anhand des `MolecularPool` die aktive Expressionsfreigabe; mit fortschreitender Reifung entwickelt `PhenotypeBuilder` die entsprechenden phänotypischen Entitäten.
3. **Multi-Kompartiment-Koordination (`MultiCompartmentSystem`)**: Drei logisch getrennte Kompartimente (`Analysis`, `Synthesis`, `Research`) tauschen typisierte Entwicklungssignale (`GoalAccepted`, `CapabilityProbeRequested`, `ComponentReady`, `EvidenceVerified`, `SynthesisCompleted`, `NegativeEvidence`) mit Vertrags-Hash-Bindung über denselben Genom-Hash aus.
4. **Kumulativer Komponenten-Erfahrungsspeicher (`ComponentExperienceStore` / `GDIC1`)**: Sobald eine Entwicklungsstufe in der Sandbox verifiziert wurde, werden alle neu induzierten Datentypen (`struct` inkl. Methoden) und Funktionen als verifizierte Architekturfragmente mit Genom-Hash, Ziel-Hash, verifiziertem Graph-Hash und Evidenz-Hash in einer FNV-1a-hashverketteten `GDIC1`-Datei persistiert. Nachfolgende Entwicklungsstufen (sowie spätere Läufe) importieren diese verifizierten Komponenten automatisch in `induceArchitectureGoal`, sodass komplexe Softwareprojekte kompositional auf bereits gelernten Typen, Methoden und Funktionen aufbauen, ohne das Genom zu verändern.
5. **Kausale Provenienzkette & Deterministischer System-Snapshot (`GenomePersistence`)**: Für jede Entwicklungsstufe wird die vollständige Kausalkette (`GENOME` $\rightarrow$ `ONTOGENY` $\rightarrow$ `COMPARTMENTS` $\rightarrow$ `INDUCTION` $\rightarrow$ `VERIFICATION`) in `developmental_intelligence.json` protokolliert und der Gesamtzustand des entwickelten Organismus als deterministischer Snapshot (`organism_snapshot.gdi`) gesichert.

Beispiel-Aufruf (vorgegebenes Curriculum):

```powershell
.\build\gdi_intelligence.exe `
  --curriculum .\examples\developmental_curriculum\ontogeny.curriculum `
  --workspace .\generated\ontogeny_run `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --memory .\generated\components.memory `
  --repair-memory .\generated\repairs.memory `
  --cycles 8
```

### 17.1 Autonomer ontogenetischer Entwicklungsplaner (`planAutonomousOntogenyFromObjective`) & Kontrollversuch (`runOntogeneticControlExperiment`)

Über das Ausführen vorgegebener Curricula hinaus besitzt GDI einen **autonomen ontogenetischen Entwicklungsplaner** (`planAutonomousOntogenyFromObjective`, `runAutonomousGoalDirectedOntogeny`, `runOntogeneticControlExperiment`), der den Übergang von einer programmierbaren Entwicklungsfolge zu einer **zielgerichtet selbstorganisierten Entwicklungsintelligenz** vollzieht:

1. **Übergeordnetes Ziel ohne vorgegebenes Curriculum**: GDI erhält lediglich ein übergeordnetes Ziel (z. B. *„Entwickle eine C++20-Anwendung, die zweidimensionale geometrische Objekte verwaltet, deren Eigenschaften berechnet und die Ergebnisse speichert.“*).
2. **Fähigkeits- und Wissenslücken-Analyse (`OntogeneticCapabilityAudit`)**: Der Planer bestimmt die benötigten Domänenmodule (`math`, `geometry`, `analytics`, `storage`), Datentypen (`struct Point2D`), Methoden (`Point2D::manhattan`) und Funktionen (`square`, `norm_sq`, `point_energy`, `store_geometry_record`) und gleicht sie mit dem persistenten `ComponentExperienceStore` (`GDIC1`) ab:
   - Bereits verifizierte Komponenten im Gedächtnis werden als `satisfiedFromMemory` markiert.
   - Fehlende Fähigkeiten werden als explizite `knowledgeGaps` ausgewiesen.
3. **Eigenständige Ableitung des Entwicklungs-Curriculums**:
   - **Naiver Zustand (ohne Komponentengedächtnis)**: GDI erkennt alle 6 Komponenten als Wissenslücken und plant eigenständig ein **3-stufiges ontogenetisches Curriculum** (`Neonatal`: `square` $\rightarrow$ `Juvenile`: `Point2D`, `manhattan`, `norm_sq` $\rightarrow$ `Mature`: `point_energy`, `store_geometry_record`).
   - **Erfahrener Zustand (mit Komponentengedächtnis aus früheren Läufen)**: GDI erkennt, dass `square`, `Point2D`, `Point2D::manhattan`, `norm_sq` und `point_energy` bereits verifiziert im Gedächtnis vorliegen, überspringt die Stufen `Neonatal` und `Juvenile` vollständig und plant ein **1-stufiges `Mature`-Curriculum**, das gezielt nur die noch fehlende Speicherfähigkeit (`store_geometry_record`) unter Wiederverwendung der Gedächtnisbausteine induziert und in der Sandbox verifiziert.
4. **Wissenschaftlicher Kontrollversuch (`--control-experiment`)**: Führt dasselbe übergeordnete Ziel einmal ohne und einmal mit Komponentengedächtnis aus und weist in `ontogenetic_control_experiment.json` nach, dass Erfahrung nicht nur passiv wiederverwendet wird, sondern den abgeleiteten Entwicklungsweg (`STAGES_SAVED_BY_EXPERIENCE`, `MEMORY_ALTERED_PATH=YES`) aktiv verändert.

Beispiel-Aufruf (Autonome Planung & Kontrollversuch):

```powershell
.\build\gdi_intelligence.exe `
  --objective "Entwickle eine C++20-Anwendung, die zweidimensionale geometrische Objekte verwaltet, deren Eigenschaften berechnet und die Ergebnisse speichert." `
  --control-experiment `
  --workspace .\generated\control_experiment `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --memory .\generated\components.memory
```

### 17.2 Fremde C++20-Softwareprojekte erschließen, gleichzeitig Hypothesen bilden & Duales Gedächtnis (`foreign_project_intelligence`)

Mit `include/gpi/foreign_project_intelligence.hpp` und `src/gpi/foreign_project_intelligence.cpp` ist GDI nicht mehr auf Projekte beschränkt, die es selbst erzeugt hat, sondern kann **fremde, bislang unbekannte C++20-Softwareprojekte laden, deren Architektur eigenständig erschließen und aus der Analyse neue Entwicklungsaufgaben, Funktionen und übertragbare Entwicklungsfähigkeiten ableiten**:

1. **Fremdprojekt-Import & Vollständige Sprach-/Projektanalyse (`discoverOrLoadForeignProject`, `analyzeForeignCpp20Project`)**:
   - Importiert beliebige C++20-Projektverzeichnisse (Header in `include/`, Quellen in `src/`, Tests in `tests/`, `CMakeLists.txt`, `fixtures/expected.txt`) sowohl mit als auch **ohne** vorhandenes `project.gpi`-Manifest.
   - Parst alle C++20-Übersetzungseinheiten mit dem genomischen C++20-Parser (`Cpp20Parser::parseWithGenome`) und `SemanticAnalyzer` und extrahiert `DiscoveredSymbol`-Einträge (Namespaces, `struct`-Typen mit Feldern und Methoden, `enum class`, `concept`s, Funktionsdeklarationen, Funktionsdefinitionen, Templates) sowie `DiscoveredModuleInterface`-Strukturen und Aufruf-/Datenflusskanten (`dataflowEdges`).
2. **Gleichzeitige Analyse & Hypothesengeleitete Entwicklungsplanung (`ArchitecturalHypothesis`)**:
   - Projektanalyse und Entwicklungsplanung sind **nicht** in zwei getrennte Phasen aufgespalten: Bereits während der AST-, Schnittstellen- und Abhängigkeitsanalyse bildet GDI explizite Architekturhypothesen (`UnimplementedInterfaceContract`, `MissingStandardHeader`, `MissingModuleLinkDependency`, `TypeMismatchOrConversionDefect`, `UntestedPublicCapability`), untersucht fehlende Informationen gezielt (z. B. Aufrufstellen und Test-Assertions für deklarierte, aber nicht implementierte Modulfunktionen) und leitet daraus unmittelbar überprüfbare Entwicklungsziele ab.
   - Der iterative Kreislauf läuft geschlossen über: *Analysieren $\rightarrow$ Verstehen $\rightarrow$ Hypothesen bilden $\rightarrow$ Entwicklungsziel ableiten $\rightarrow$ Implementieren (`synthesizeMissingModuleFunction` / autonome Reparatur) $\rightarrow$ Verifizieren (Compiler + AppContainer-Sandbox) $\rightarrow$ Weiterlernen*.
3. **Doppeltes Gedächtnis: Projektgedächtnis (`GDIPM1`) vs. Entwicklungsgedächtnis (`GDIDP1`)**:
   - **Projektgedächtnis (`ProjectMemoryStore` / `GDIPM1`)**: Speichert FNV-1a-hashverkettet, was GDI über **dieses konkrete fremde Projekt** herausgefunden hat (`ForeignProjectKnowledgeRecord`: entdeckte Module, Typen, Funktionen, Hypothesen, Projekt-Fingerprint und verifizierter Graph-Hash).
   - **Entwicklungsgedächtnis (`DevelopmentalPrincipleStore` / `GDIDP1`)**: Speichert FNV-1a-hashverkettet **allgemeingültige, übertragbare Lösungs- und Syntheseprinzipien** (`TransferableDevelopmentalPrinciple`: z. B. `principle:interface_synthesis:struct_field_weighted_sum`, Header- und Link-Reparaturmuster mit struktureller Trigger-Signatur `triggerSignature` und `actionTemplate`). Wird GDI später auf ein **völlig anderes fremdes Projekt** (z. B. `inventory::StockItem` statt `telemetry::SensorReading`) angesetzt, erkennt es die strukturelle Signatur der Wissenslücke und überträgt das gelernte Entwicklungsprinzip direkt (`TRANSFERRED_PRINCIPLE=YES`).
4. **Strikte Read-Only-Sicherheitsgrenze (`computeDirectoryTreeHash`)**:
   - Fremder Quellcode wird standardmäßig ausschließlich **Read-Only** analysiert. Vor und nach dem gesamten Entwicklungszyklus berechnet GDI den kryptographischen/FNV-1a-Verzeichnisbaum-Hash des Originalprojekts (`originalDirectoryHashBefore == originalDirectoryHashAfter`, `originalProjectUntouched == true`).
   - Alle Synthesen, Builds, Tests und Änderungen erfolgen kontrolliert im isolierten Arbeitsbereich (`isolatedWorkspace`) und in der Windows-AppContainer-Sandbox (`gpi_sandbox.exe`). Das Originalprojekt bleibt unverändert, bis eine Rückschreibung ausdrücklich über `--allow-writeback` (`allowWritebackToOriginal = true`) freigegeben wird.

Beispiel-Aufruf (Fremdes C++20-Projekt analysieren & weiterentwickeln):

```powershell
.\build\gdi_intelligence.exe `
  --foreign-project .\examples\foreign_telemetry_project `
  --workspace .\generated\foreign_telemetry_run `
  --compiler "F:\KR-Compiler-Windows-x64-v4.3.1\toolchain\bin\g++.exe" `
  --sandbox .\build\gpi_sandbox.exe `
  --project-memory .\generated\foreign_projects.gdipm `
  --dev-memory .\generated\developmental_principles.gdidp `
  --cycles 6
```

## 18. GDI v1.4.0 – Integrierte Windows-Entwicklungsoberfläche

Die native Win32-Anwendung `gpi_gui.exe` (sowie der Alias `gdi_gui.exe`) unterstützt die Fremdprojekt-Erschließung, den autonomen Entwicklungsplaner, die ontogenetische Entwicklungsintelligenz und sämtliche generativen Pipelines direkt aus einer Oberfläche:

- **Modus 0 – GDI Fremdes C++20-Projekt erschließen & weiterentwickeln (`gdi_intelligence.exe --foreign-project`)**: Lädt standardmäßig das Fremdprojekt `examples/foreign_telemetry_project` (oder einen per „Ordner öffnen“ gewählten C++20-Projektordner), zeigt im Zielvertragsfenster die Read-Only-Architekturanalyse sowie die gleichzeitig gebildeten Hypothesen und Entwicklungsziele an und führt den isolierten Entwicklungs- und Verifikationszyklus mit Projektgedächtnis (`GDIPM1`) und Entwicklungsgedächtnis (`GDIDP1`) aus.
- **Modus 1 – GDI Autonomer ontogenetischer Entwicklungsplaner (Ziel $\rightarrow$ eigenes Curriculum → `gdi_intelligence.exe --objective`)**: Nimmt ein übergeordnetes Ziel entgegen, analysiert vorhandene Komponenten im Gedächtnis sowie Wissenslücken, zeigt das eigenständig abgeleitete Curriculum im Zielvertragsfenster an und führt den selbstgeplanten Entwicklungslauf aus.
- **Modus 2 – GDI Autonome Entwicklungsintelligenz (`.curriculum` / Ontogenese → `gdi_intelligence.exe --curriculum`)**: Lädt standardmäßig das 3-stufige ontogenetische Curriculum `examples/developmental_curriculum/ontogeny.curriculum` (`Neonatal` $\rightarrow$ `Juvenile` $\rightarrow$ `Mature`), analysiert alle Entwicklungsstufen, Typen und Funktionen und führt die kumulative Mehrstufen-Entwicklung mit `ComponentExperienceStore` und Sandbox-Verifikation aus.
- **Modus 3 – GDI Verhaltensbeispiele (`.behavior` V1/V2 → `gdi_create.exe`)**: Lädt standardmäßig das mehrkomponentige Mehrtyp-Beispiel `examples/behavioral_geometry/geometry.behavior` (Fallback `max.behavior`), analysiert Typen, Enums, Concepts, Domänenmodule, Methoden und Funktionen mit `parseBehavioralGoalText` und startet die verhaltensbasierte Mehrdatei-Synthese.
- **Modus 4 – GDI Typisierte Zielarchitektur (`.goal` V1/V2 → `gpi_architect.exe`)**: Lädt `examples/typed_geometry/geometry.goal` (Fallback `components.goal`), validiert und analysiert den typisierten Operationsgraphen über `parseTypedArchitectureGoalText` und führt `gpi_architect.exe` aus.
- **Modus 5 – GDI Projekt-Blueprint Synthese (`.blueprint` V1/V2 → `gpi_synthesize.exe`)**: Lädt `examples/blueprint_geometry/project.blueprint` (Fallback `fibonacci`), prüft den Blueprint über `parseProjectBlueprintText` und startet `gpi_synthesize.exe`.
- **Modi 6–10 – Integrierte Forschungsmodi (`gpi_integrated.exe`)**: A–D-Vergleich, Einzelkontext, isolierte oder kommunizierende Kompartimente und Erfahrung.

In allen generativen Modi (`0..5`) ist die Windows-AppContainer-Sandbox (`gpi_sandbox.exe`) verpflichtend; die GUI materialisiert die jeweilige Spezifikation bzw. den isolierten Fremdprojekt-Workspace in einem eigenen `gdi_run_*`-Arbeitsordner und zeigt Compiler-, Linker- und Sandbox-Ergebnisse live an. Der GUI-Selbsttest (`gpi_gui.exe --self-test`) verifiziert beim Build automatisch die Parser- und Analysekette aller sechs generativen Modi (Fremdprojekt-Analyse, Autonomer Planer, `.curriculum`, `.behavior` V2, `.goal` V2, `.blueprint` V2) sowie des Forschungsmodus.
