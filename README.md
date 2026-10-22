# GDI – Genomic Developmental Intelligence

GDI ist ein in C++20 implementiertes Entwicklungs- und Forschungssystem für Programme. Ein unveränderliches lineares Genom beschreibt grammatische, regulatorische und generative Möglichkeiten. Lokale molekulare Zustände, Entwicklungszustände und verifizierte Erfahrung bestimmen, welche Fähigkeiten in einem Lauf exprimiert werden. Das Genom selbst wird dabei nicht verändert.

Das Repository enthält den reinen Projektquellcode. Der verwendete C++20-Compiler ist eine externe Abhängigkeit und gehört nicht zum Projekt.

## Was das System ausführt

- Es erzeugt aus unterstützten deutsch- und englischsprachigen Aufgaben einen expliziten Zielvertrag.
- Es zerlegt Ziele in typisierte Operationen und Abhängigkeiten.
- Es entwickelt und verifiziert C++20-Kandidaten in begrenzten Entwicklungszyklen.
- Es modelliert Analyse, Synthese und Forschung als logisch getrennte Kompartimente über demselben eingefrorenen Genom.
- Es repräsentiert beliebige C++20-Quelldateien bytegenau als genomisch hergeleitete Artefakte.
- Es verarbeitet allgemeine mehrdateiige Projektgraphen mit Headern, Quellen, Modulen, Tests, Ressourcen, Build-Manifesten, Abhängigkeiten und Targets.
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

## Desktop-Oberfläche

Unter Windows erzeugt der Build `gpi_gui.exe`. Die Oberfläche bietet:

- Eingabe und Vorschau des Zielvertrags,
- Auswahl der Entwicklungsarchitektur,
- Compiler- und Arbeitsordnerauswahl,
- Live-Protokoll,
- kontrollierten Abbruch des gesamten Prozessbaums.

Die Oberfläche gibt nur den sicheren typisierten Aufgabenmodus frei. Freier eingebetteter Prompt-Code wird dort nicht ausgeführt.

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

## Zentrale Programme

| Programm | Aufgabe |
| --- | --- |
| `gpi_gui` | native Windows-Oberfläche für sichere typisierte Entwicklung |
| `gpi_integrated` | Zielvertrag, Kompartimente, Forschung und A–D-Architekturvergleich |
| `gpi_develop` | allgemeine manifestbasierte Mehrdatei-Projekte, Reparatur und Erfahrungstransfer |
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
