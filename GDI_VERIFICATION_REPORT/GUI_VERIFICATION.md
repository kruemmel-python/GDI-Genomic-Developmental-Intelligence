# GUI-Verifikation

Status: **PARTIAL**.

Implementierungsnachtrag: `gpi_gui.exe` und `gdi_gui.exe` werden nun beide erzeugt und sind bytegleich. Abschluss-SHA-256: `FBED6DCE6BCE2D13B2172681CC490E10476C11BE14A807E009191B5054115D7A`. Beide Self-Tests endeten mit Exit 0. Der Beleg meldet Engine 1.4.0 und GCC 16.1.0. Beim Start des Alias über PowerShell wurde weiterhin der kanonische Prozesspfad `gpi_gui.exe` protokolliert; deshalb bleibt die Alias-Pfadidentität PARTIAL.

- `gpi_gui.exe --self-test`: PASS über CTest.
- Prüfbuild-Hash von `gpi_gui.exe`: beginnt mit `5D7628AF10F59628E07E275546073F2721FFAB7B3CDBB818E3A1CBCE9` (vollständiger Hash über `Get-FileHash` reproduzierbar).
- `gdi_intelligence.exe`: Hash beginnt `ECABD234EC75B8113FEFC8D33E6B1BCE450EB2371788325C2EE4CD45A`.
- `gpi_sandbox.exe`: Hash beginnt `CC5E1F0CCA996E6A0C7B5E943039F5901FE52F35E1F8567BCC965C058`.
- `gdi_gui.exe` wurde trotz optionaler Post-Build-Kopie nicht erzeugt; es ist kein eigenes Target.
- Ein vollständiger interaktiver Fremdprojektlauf über die sichtbare GUI und eine belastbare DLL-Importauflistung wurden nicht durchgeführt.
- Keine Binärdateien wurden nach `KR_BUILD` synchronisiert, weil die verlangte vollständige GUI-Verifikation nicht erreicht wurde.
