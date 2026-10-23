# R19 language-neutral requirement concepts

## Change

R19 no longer selects its existing semantic capabilities from English-only phrases. The requirement frontend now maps lexical realizations to a stable concept vector before source discovery:

| Canonical concept | Supported realizations |
|---|---|
| record | CSV, record(s), Datensatz/Datensaetze, Zeile(n) |
| invalid | invalid, malformed, ungueltig/ungültig, fehlerhaft |
| report | report, error, log, Protokoll, Fehlerbericht, Fehlermeldung |
| resume | resume, restore, recover, wiederherstell-, fortsetz-, laden |
| state | state, status, Zustand, Spielstand, Speicherstand |
| corrupt | corrupt, inconsistent, damaged, beschaedigt/beschädigt, inkonsistent, defekt, ungueltig/ungültig, invalid |

The canonical vector selects `record_validation` or `state_resume_validation`; the C++ source model, candidate selection, transformation and runtime proof remain unchanged. Thus language aliases do not encode source filenames, symbols, data structures or integration positions.

## Verification

`GDIR19SemanticIntegrationTests` passed after the change (480.96 seconds) with all prior hook-free end-to-end cases and new German semantic inputs:

* `Bei CSV-Datensaetzen ungueltige Zeilen ueberspringen und einen Fehlerbericht schreiben.`
* `Einen gespeicherten Zustand wiederherstellen und beschaedigte Zustandsdaten ablehnen.`

The post-development CIVITAS reanalysis recognizes the same German city-state requirement as `state_resume_validation`. It still returns `BLOCKED` because CIVITAS does not present R19's narrowly proved state-loader-to-process pattern. No original file was changed.

## Boundary

This is a semantic intermediate representation with German and English language packs, not a claim of universal natural-language understanding. Unknown language variants or requirements outside the two authorized capability classes remain fail-closed.
