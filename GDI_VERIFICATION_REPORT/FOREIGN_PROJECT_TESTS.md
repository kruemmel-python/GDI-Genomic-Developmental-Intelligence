# Fremdprojekt-Tests

## Projekt A – fehlende Implementierung

Seed und erster Wiederholungslauf: `BLOCKED`, Exit 4, echte Linkerdiagnose `undefined reference to calc::combine(long long, long long)`. Ursache: `!=`-Testguards wurden nicht als Beispiele extrahiert. Nach Korrektur: `PASS`, Exit 0; Originalhash vor/nach `8032522382443503742`.

## Projekt B – veraltete API

`std::filesystem::u8path(std::string)` einschließlich leerem, ASCII- und UTF-8-Pfad. `PASS`, Exit 0; Originalhash vor/nach `11039044267231331814`. Die Reparatur erfolgte nur in der isolierten Kopie.

## Projekt C – fehlende Link-Abhängigkeit

Die Implementierung existierte bereits, aber der Testtarget-Graph enthielt keine Linkkante. `PASS`, Exit 0; Originalhash vor/nach `589453511364399324`. Keine zweite Implementierung wurde erzeugt.

## Projekt D – MDRAFTNE KR/Go/Rust/SDL End-to-End

GDI ignorierte vorhandene generierte `KR_BUILD`-Dateien, importierte die `.kr`-Primärquellen read-only und ließ die unveränderte KR-Compiler-Engine alle acht C++-Module sowie Rust- und Go-Archive neu erzeugen. Das Windows-x64-Programm wurde mit SDL3 gelinkt, gestartet und bis zum ersten gerenderten Frame sowie 300 Frames beobachtet. Der automatisch aus Primärsemantik erzeugte ABI-Grenzfalltest bestand. Original-SHA-256 vor/nach: identisch (`d9be5adf49a98ccc9d6c3b55aeb51f8e79f3f97726a3313edfaafd3c356870fa`).
