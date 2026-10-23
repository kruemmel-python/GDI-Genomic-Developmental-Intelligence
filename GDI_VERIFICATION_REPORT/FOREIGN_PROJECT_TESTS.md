# Fremdprojekt-Tests

## Projekt A – fehlende Implementierung

Seed und erster Wiederholungslauf: `BLOCKED`, Exit 4, echte Linkerdiagnose `undefined reference to calc::combine(long long, long long)`. Ursache: `!=`-Testguards wurden nicht als Beispiele extrahiert. Nach Korrektur: `PASS`, Exit 0; Originalhash vor/nach `8032522382443503742`.

## Projekt B – veraltete API

`std::filesystem::u8path(std::string)` einschließlich leerem, ASCII- und UTF-8-Pfad. `PASS`, Exit 0; Originalhash vor/nach `11039044267231331814`. Die Reparatur erfolgte nur in der isolierten Kopie.

## Projekt C – fehlende Link-Abhängigkeit

Die Implementierung existierte bereits, aber der Testtarget-Graph enthielt keine Linkkante. `PASS`, Exit 0; Originalhash vor/nach `589453511364399324`. Keine zweite Implementierung wurde erzeugt.
