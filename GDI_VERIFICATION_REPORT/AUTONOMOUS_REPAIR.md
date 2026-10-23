# Autonome Reparatur

Status: **PARTIAL**.

Implementierungsnachtrag: Kampagnen schreiben nun strukturierte Diagnosezahlen und einen expliziten `termination_reason`; bereits versuchte Graphhashes werden unterdrückt. Der begrenzte Legacy-Fallback bleibt für bestehende Reparaturklassen erhalten und die vollständige Regression bestand.

MDRAFTNE zeigte einen echten kumulativen Verlauf: Iteration 1 und 2 scheiterten mit GCC-`-Werror=deprecated-declarations` an `u8path`; in Iteration 3 dokumentiert `campaign.json` Seed (nicht verifiziert) und Cycle 2 `genomic-deprecated-api-1` (verifiziert). Der Graphhash änderte sich von `1063466948934294754` auf `7347038244951325089`.

Projekt A zeigte dagegen vor der Codekorrektur zwei identische Seed-Zyklen ohne gewählte Hypothese oder Transformation. Das Ergebnis blieb korrekt `success:false`; es wurde kein falscher Erfolg behauptet.
