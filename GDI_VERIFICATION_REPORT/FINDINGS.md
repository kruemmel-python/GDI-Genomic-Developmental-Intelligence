# Findings

## Behoben – `!=`-Guards wurden nicht als Verhaltensvertrag erkannt

Komponente: `src/gpi/foreign_project_intelligence.cpp`, Funktion `synthesizeMissingModuleFunction`.

Source-to-Sink: unabhängiger C++-Testguard → `testAndAppSource` → Beispielparser → leere `extractedSamples` → keine Synthese → unveränderte Linker-Sink mit `undefined reference`. Die Gegenprüfung bestätigte, dass Namespace, Symbolerkennung, Compiler und Linker arbeiteten; nur der Operatorfilter verwarf das Beispiel. Der Rückwärtspfad vom Linkerfehler führte zur fehlenden Transformation und anschließend zur Operatorprüfung.

Fix: `==` und der konventionelle `!= expected`-Guard werden als beobachteter Sollwert akzeptiert. Regression in `tests/test_behavioral_induction.cpp`. Betroffener Test sowie vollständige Suite bestanden danach.

## Offen – GUI-Alias fehlt

Status: **PARTIAL**. `gpi_gui.exe` ist vorhanden und getestet; `gdi_gui.exe` fehlt im Prüfbuild. Da die Kopie ausdrücklich optional/tolerant implementiert ist, ist dies kein Compilerfehler, aber die geforderte Binärnamensynchronität ist nicht erfüllt.

## Bewertungsgrenzen

- Kein Beleg für vollständige ISO-C++20-Konformität.
- MDRAFTNE besitzt nun einen vollständigen Windows-x64-Build-, Link- und Startbeleg mit echten Go-/Rust-Archiven, SDL3, erstem Frame und ABI-Funktionstest. Nicht abgedeckt sind alle interaktiven GUI-/Picker-Abläufe und der Android-Gerätelauf.
- Namensspezifische ABI-Synthese ist keine allgemeine Sprachübersetzung.
