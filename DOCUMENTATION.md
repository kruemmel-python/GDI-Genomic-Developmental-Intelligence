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

### 3.1 Interpretierte Sprachebene

Lexer, Präprozessor, Parser, Typen, Klassen, Templates, Concepts, `constexpr`, Laufzeitmerkmale und Speicherregeln bilden einen geprüften Teil von C++20 ab. Regeln werden soweit vorgesehen aus Genomregionen gelesen und nicht ausschließlich als Host-Fallunterscheidungen behandelt.

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

## 7. Verifikation

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

## 8. Autonome Entwicklungszyklen

`runAutonomousProjectRepair` beginnt immer mit dem unveränderten Projekt. Aus der ersten fehlgeschlagenen Evidenz wählt es ausschließlich genomisch freigegebene Operatoren. Implementiert und end-to-end geprüft sind:

- Standard-Header aus einer echten Fehlendeclarations-Diagnose ergänzen,
- einen zuvor nicht eingebundenen Provider anhand von Linkersymbolen verbinden,
- einen numerischen Stringliteral in einen typisierten Zahlenliteral umwandeln,
- Zahlenliterale anhand beobachteter Ausgabedifferenzen kalibrieren,
- bei fehlender numerischer Ausgabe eine begrenzte lokale Ganzzahlnachbarschaft durchsuchen.

Jeder Kandidat wird erneut vollständig kompiliert und verhaltensgeprüft. Der Entwicklungszyklus besitzt ein festes Budget von höchstens 16 Versuchen. Nicht erkannte Fehler werden nicht willkürlich umgeschrieben.

## 9. Erfahrung und Transfer

Erfolgreiche Reparaturen werden als `VerifiedProjectTransformation` gespeichert. Ein Datensatz bindet:

- vollständigen Genom-Hash,
- Ursprungsgraph-Hash,
- verifizierten Graph-Hash,
- Transformationstyp,
- Zielknoten und Hilfsdaten,
- Vorher-/Nachher-Token,
- Hash des ursprünglichen Evidenzberichts.

Der Speicher ist append-only und hashverkettet. Beim Laden werden Format, Grenzen, Kette, Provenienz und transformationsspezifische Pflichtfelder geprüft. Eine Transformation eines anderen Genoms wird abgelehnt.

Transfer gilt erst als erfolgreich, wenn die gespeicherte Transformation auf einen verwandten neuen Graphen anwendbar ist und das Ergebnis den neuen Zielvertrag unabhängig besteht. Die frühere Evidenz ersetzt niemals die neue Prüfung.

## 10. Multi-Kompartiment-Regulation

Analyse, Synthese und Forschung arbeiten logisch getrennt mit demselben Genom. Jedes Kompartiment besitzt lokale Moleküle, zugängliche Regionen und Erfahrungserlaubnisse. Typisierte Signale verlangen zulässige Sender-/Empfängerrollen und einen nichtleeren Zielvertrag-Hash.

Der Molekülpool verwendet einen strukturierten Schlüssel aus vollständiger Kompartiment-ID und vollständiger 64-Bit-Signatur; Signaturen mit gleichen unteren 48 Bit bleiben verschieden.

Der A–D-Vergleich misst mit demselben Ziel und Genom:

- einen Einzelkontext,
- isolierte Kompartimente,
- kommunizierende Kompartimente,
- kommunizierende Kompartimente mit verifizierter Erfahrung.

## 11. Forschungsentscheidungen

Eine fehlende gespeicherte Komponente ist nicht automatisch eine fehlende Fähigkeit. GDI misst zunächst einen Basiskandidaten. Die Diagnose unterscheidet vorhandene wiederverwendbare Fähigkeit, ohne Forschung lösbare Aufgabe, messbare Optimierungsmöglichkeit und fehlende ausführbare Fähigkeit.

Hypothesen werden kompiliert, durch unabhängige Fixtures geprüft und anhand aufgezeichneter Evidenz verglichen. Forschungsziele dürfen keine unbegrenzte Selbstmodifikation auslösen; Genom, Zyklusbudget, Dateigrenzen und Ausführungsgrenzen bleiben invariant.

## 12. Sandbox

Unter Windows startet `gpi_sandbox` den Kandidaten in einem AppContainer:

- kein deklarierter Netzwerkzugriff,
- eigener Container-Speicher,
- nur Kandidat und erforderliche Laufzeit-DLLs werden bereitgestellt,
- Job-Objekt beendet den gesamten Prozessbaum,
- festes Timeout,
- keine unisolierte Rückfallausführung, wenn Sandboxpflicht gesetzt ist.

Die Sandbox reduziert das Risiko unbekannten Programmcodes, ist aber kein Beweis absoluter Betriebssystemsicherheit. Betriebssystem- und Compileraktualisierungen bleiben Teil des Vertrauensankers.

## 13. Programme und Bibliotheksmodule

Die öffentliche Bibliothek liegt unter `include/`, die Implementierung unter `src/`. Wichtige Module:

| Modul | Zweck |
| --- | --- |
| `genome/` | Strang, Regionen, Decoder, Integrität und Bootstrap |
| `regulation/` | Moleküle und regulatorische Auflösung |
| `cpp20/` | interpretierte Sprachebene und exakte Quellebene |
| `gpi/generative_project` | allgemeiner typisierter Projektgraph |
| `gpi/project_manifest` | sicherer Dateiimport und Zielvertrag |
| `gpi/project_verifier` | Compiler-, Linker-, Sandbox- und Verhaltensprüfung |
| `gpi/autonomous_project_developer` | evidenzgeleitete Entwicklungszyklen |
| `gpi/project_experience` | persistente Transformationen und Transfer |
| `gpi/integrated_development` | Zielvertrag, Kompartimente und Vergleich |
| `runtime/` | begrenzte IR-/VM-Ausführung |

## 14. Tests

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

## 15. Reproduzierbarkeit und Veröffentlichung

Das Developer-Paket enthält ausschließlich:

- `apps/`, `include/`, `src/`, `tests/`, `examples/`,
- `CMakeLists.txt`,
- `README.md`,
- `DOCUMENTATION.md`,
- `LICENSE`.

Build-Verzeichnisse, erzeugte Programme, Laufprotokolle, Gedächtnisdateien, lokale Compiler und historische Entwicklungsnotizen gehören nicht in das Paket.
