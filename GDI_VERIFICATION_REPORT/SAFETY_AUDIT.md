# Sicherheits- und Safety-Audit

Status: **PARTIAL**, keine bestätigte ausnutzbare Sicherheitslücke nach statischer Source-to-Sink-Gegenprüfung.

Implementierungsnachtrag: kryptografische SHA-256-Manifeste, Importgrößenlimits, Symlink/Junction-Abweisung und kanonische Original-/Workspace-Trennung sind nun produktiv integriert und negativ getestet. MDRAFTNE hatte vor und nach dem Abschlusslauf exakt SHA-256 `d9be5adf49a98ccc9d6c3b55aeb51f8e79f3f97726a3313edfaafd3c356870fa`.

- Fremdprojekte A/B/C und MDRAFTNE: interne Vor-/Nachhashes identisch, `allowWriteback=false`, `writeback_performed=false`.
- Manifest-/Blueprint-Pfade besitzen lexikalische und kanonische Root-Prüfungen; Sandbox-Pfade werden kanonisiert und auf Zugehörigkeit geprüft.
- Kandidaten werden über Windows AppContainer gestartet; die Suite prüfte den Sandboxvertrag.
- Generiertes `KR_BUILD` wird beim optionalen Writeback explizit übersprungen.
- Kein Original unter `D:\antigravity_GDI` oder `F:\MDRAFTNE` wurde geändert; die KR-Toolchain wurde nur ausgeführt/gelesen.

Coverage: Dateipfade/Traversal `COMPLETE`; Native Memory `PARTIAL`; Prozess-/Sandboxgrenze `COMPLETE` für statischen Pfad; AuthN/AuthZ/Web/Krypto `BLOCKED` bzw. nicht relevant (keine entsprechende Serveroberfläche); Concurrency `PARTIAL`; Dependencies `PARTIAL`.

Härtung: Fremdprojekt-Rekursion sollte Symlink-/Junction-Policy und Größenlimits explizit dokumentieren und testen. Dies wurde mangels belegtem erreichbaren Schreib-Sink nicht als bestätigte Schwachstelle gewertet.
