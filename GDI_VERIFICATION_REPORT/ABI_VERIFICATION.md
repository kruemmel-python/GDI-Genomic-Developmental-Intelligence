# ABI- und MDRAFTNE-Verifikation

Status: **PARTIAL**.

Implementierungsnachtrag: Die beiden namensfesten ABI-Synthesezweige wurden durch eine typgeprüfte, namensunabhängige Pipeline für begrenzte skalare KR/Go/Rust-Ausdrücke ersetzt. Die unbekannten Funktionen `abi_limit_ok` und `abi_step_count` wurden aus Primärquellen abgeleitet, kompiliert, gelinkt und mit Randfällen getestet. Komplexe ABI-Verträge bleiben fail-closed; MDRAFTNE ist weiterhin kein vollständiger Programm-E2E-Nachweis.

`F:\MDRAFTNE` war vorhanden. GDI erkannte 10 `.kr`-Primärquellen, 50 generierte KR_BUILD-Dateien, 10 Module, 105 Symbole und 86 Hypothesen. Der Originalbaum blieb mit internem Hash `17537356594078581135` unverändert; 66 Dateien wurden nachher nochmals per SHA-256 inventarisiert (aggregierter Nachherwert `21C0A25D3F38BBA3C359DE8D37803D3D719901EE7876D9F26C86F2BD3DC49317`).

Die Semantik der beiden ABI-Funktionen stimmt textuell überein:

- Go/KR `md_workspace_action_valid`: wahr genau für 0 bis 4.
- Rust/KR `md_workspace_accumulate_nonempty`: erhöht nur bei `has_content`.

GDI enthält für beide Namen ausdrücklich fest codierte Synthesezweige. Das ist keine allgemeine autonome KR-/Go-/Rust-Übersetzung. Der MDRAFTNE-Status `PASS` belegt nur das erkannte `core`-Compile; es gab kein vollständiges Linken der originalen Fremdsprachenbibliotheken, keine gestartete MDRAFTNE-Executable und keinen Funktionstest. Gesamtstatus daher `PARTIAL`.
