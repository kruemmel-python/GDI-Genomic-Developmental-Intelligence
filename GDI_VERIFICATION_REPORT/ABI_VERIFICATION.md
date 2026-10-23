# ABI- und MDRAFTNE-Verifikation

Status: **PARTIAL**.

Implementierungsnachtrag: Die beiden namensfesten ABI-Synthesezweige wurden durch eine typgeprüfte, namensunabhängige Pipeline für begrenzte skalare KR/Go/Rust-Ausdrücke ersetzt. Die unbekannten Funktionen `abi_limit_ok` und `abi_step_count` wurden aus Primärquellen abgeleitet, kompiliert, gelinkt und mit Randfällen getestet. Komplexe ABI-Verträge bleiben fail-closed; MDRAFTNE ist weiterhin kein vollständiger Programm-E2E-Nachweis.

`F:\MDRAFTNE` war vorhanden. GDI erkannte 10 `.kr`-Primärquellen, 50 generierte KR_BUILD-Dateien, 10 Module, 105 Symbole und 86 Hypothesen. Der Originalbaum blieb mit internem Hash `17537356594078581135` unverändert; 66 Dateien wurden nachher nochmals per SHA-256 inventarisiert (aggregierter Nachherwert `21C0A25D3F38BBA3C359DE8D37803D3D719901EE7876D9F26C86F2BD3DC49317`).

Die Semantik der beiden ABI-Funktionen stimmt textuell überein:

- Go/KR `md_workspace_action_valid`: wahr genau für 0 bis 4.
- Rust/KR `md_workspace_accumulate_nonempty`: erhöht nur bei `has_content`.

Abschlussnachtrag: `gdi_intelligence --kr-e2e` importierte ausschließlich die zehn `.kr`-Primärquellen in einen isolierten Arbeitsbereich. Die unveränderte KR-Compiler-Engine erzeugte acht C++-Translation-Units, `libkr_rust.a` (7.068.306 Bytes), `libkr_go.a` (3.858.404 Bytes), `SDL3.dll` und `programm.exe` (7.910.100 Bytes). Der namenunabhängig aus den erkannten Verträgen generierte ABI-Test meldete `KR_ABI_FUNCTIONAL_PASS`. Die Anwendung erreichte `SDL_Init`, Fenster- und Renderer-Erzeugung, Eventloop, ersten Frame und 300 Frames. Vorher-/Nachher-SHA-256 des Originals: `d9be5adf49a98ccc9d6c3b55aeb51f8e79f3f97726a3313edfaafd3c356870fa`.
