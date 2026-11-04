// tsio.h - TensorStore-I/O-Schicht fuer tsq.
//
// Diese Header ist bewusst frei von TensorStore: tsq.cpp benutzt ausschliesslich
// die hier deklarierten Funktionen, alle TensorStore-Aufrufe stehen in tsio.cpp
// (ohne TensorStore gebaut wird tsio_stub.cpp mit derselben API verwendet).
//
// Datensatz-Durchlauf: ein Dataset wird genau einmal sequenziell durchlaufen,
// jedes Feld wird genau einmal gelesen.  Der Aufrufer iteriert aeusserlich
// ueber die Schritte (tsGetNextStep) und innerlich ueber die Felder eines
// Schritts (tsGetField/tsReadField).  Die Reihenfolge ist: Schritt aufsteigend,
// darin die Arrays in der Reihenfolge von tsMetadata::arrayList, darin die
// Layer aufsteigend.  Die Schrittzahl und die Zahl der Felder pro Schritt ist
// fuer alle Arrays gleich.  Die Identitaet des gelieferten Feldes steht in
// tsField; tsReadField/tsWriteField arbeiten immer auf dem zuletzt durch
// tsGetField/tsSetField ausgewaehlten Feld.

#ifndef TSIO_H
#define TSIO_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// 1-basierter, inklusiver Bereich einer Achse; ein nicht gesetztes `end`
// bedeutet "bis zur Ausdehnung des Arrays".
struct tsRange
{
  std::int64_t start = 1;
  std::optional<std::int64_t> end;
};

// Konfiguration eines Datensatzes inklusive Array- und Hypercube-Auswahl.
struct tsConfig
{
  int format = 0;       // 0 = auto-detect, 2 or 3 = force
  std::string kvstore;  // "" = auto-detect, "file", "s3" or "http"
  std::string path;
  std::vector<std::string> arrays;
  std::optional<std::string> endpoint;
  bool consolidated = false;                 // Metadaten aus `.zmetadata` (zarr2)
  std::optional<std::string> group;          // zarr3 Gruppe unterhalb `path`
  bool debug = false;                        // Diagnose auf stderr (in tsio)
  bool info = false;                         // nur TensorStore-Version ausgeben
  std::optional<int> data_copy_concurrency;  // parallele Chunk-(De)Kompression
  bool double_buffer = false;                // Double-Buffer-Prefetch
  bool overwrite = false;                    // --out-path vorher loeschen
  std::optional<std::string> out_path;       // Ausgabe-Datensatz (Schreibmodus)
  std::string out_format = "zarr3";          // Ausgabeformat: "zarr2", "zarr3" oder "n5"
  std::string out_dtype = "float32";         // Datenarrays (zarr3): "float64", "float32", "float16" oder "bfloat16"
  std::optional<std::string>
      compressor;  // --compressor: Ausgabe-Kompressor (Schreibmodus); ungesetzt = Quell-Kompression uebernehmen
  std::optional<std::int64_t> step_chunk;  // --step-chunk: Laenge der step-Ausgabe-Chunks (Schreibmodus)
  std::optional<std::int64_t> cell_chunk;  // --cell-chunk: Laenge der cell-Ausgabe-Chunks (letzte Dim, Schreibmodus)
  tsRange step;                            // Zeitachse
  tsRange layer;                           // Layer-Achse (rank >= 3)
  tsRange cell;                            // letzte Achse (Gitterpunkte)
};

// Ein Array des Datensatzes mit Auswahl und Metadaten.  Nur einfache
// Datentypen, damit tsq.cpp weder TensorStore noch JSON braucht.  `attributes`
// enthaelt Skalare als Text (Zahlen/Booleans als JSON-Skalar, Strings ohne
// Anfuehrungszeichen, Objekte/Arrays als kompaktes JSON).
struct tsArray
{
  std::string name;
  int format = 0;  // 2 oder 3
  std::string kvstore;
  std::string dataType;
  std::string compressor;                // Quell-Kompressor, "-" wenn keiner/unbekannt
  std::vector<std::int64_t> shape;       // volle Quellform
  std::vector<std::int64_t> chunks;      // aeussere Quell-Chunkform
  std::vector<std::int64_t> bstart;      // Auswahl, 0-basiert
  std::vector<std::int64_t> bcount;      // Auswahl, auf die Form geklemmt
  std::vector<std::int64_t> out_chunks;  // Ausgabe-Chunkform (Schreibmodus)
  std::vector<std::string> dim_names;    // dimension_names / _ARRAY_DIMENSIONS
  std::map<std::string, std::string> attributes;
  std::int64_t num_steps = 0;  // ausgewaehlte Zeitschritte dieses Arrays
  std::int64_t num_layers = 1;
  std::int64_t num_cells = 0;  // Werte pro Layer
};

// Metadaten eines geoeffneten Datensatzes: die ausgewaehlten Daten-Arrays
// (alphabetisch) und die von ihnen benutzten Koordinaten-Arrays (alphabetisch).
struct tsMetadata
{
  std::int64_t num_steps = 0;  // Maximum der num_steps aller Daten-Arrays
  std::vector<tsArray> arrayList;
  std::vector<tsArray> coordList;
};

// Eine Zeile der Dataset-Uebersicht.  Die Listen sind bereits formatiert
// (z. B. "[3653,12]"), weil ihre Formatierung zarr-spezifisch ist.
struct tsInfo
{
  std::string name;
  std::string units;      // "-" wenn nicht gesetzt
  std::string data_type;  // z. B. "float32", "-"
  std::string shape;
  std::string chunks;
  std::string compressor;              // "-" wenn unbekannt
  std::vector<std::string> dim_names;  // dimension_names / _ARRAY_DIMENSIONS
  std::string grid_mapping;            // `grid_mapping`-Attribut, "" wenn nicht gesetzt
};

// Uebersicht eines Datensatzes: Gruppenliste (nur Zarr-3-Wurzel) oder eine
// Zeile pro Array.
struct tsOverview
{
  std::vector<std::string> groups;
  std::vector<tsInfo> arrays;
  bool ok = false;
};

// Identitaet eines Feldes: Index in tsMetadata::arrayList und 0-basierter
// (absoluter) Layer.  Der Zeitschritt ist NICHT Teil des Feldes: er wird von
// tsGetNextStep zurueckgegeben.
struct tsField
{
  std::size_t array = 0;
  std::int64_t layer = 0;
};

// Eine Koordinatenausgabe: der von tsReadCoordinates gelesene Quell-Teil
// (unabhaengig vom Output-Config) und der von tsApplyCoordinateConfig gesetzte
// Ausgabe-Teil.  `source` ist ein opaker Zeiger auf den Quell-Store (nur
// waehrend das Input-Handle offen ist gueltig), `data` ein opaker
// TensorStore-Puffer (SharedArray im Quell-Datentyp).
struct tsCoordinate
{
  // Quelle (von tsReadCoordinates gefuellt)
  std::string name;
  int format = 0;               // Quell-Zarr-Format (2 oder 3)
  std::string source_metadata;  // Roh-.zarray/zarr.json der Quelle (Kompression)
  std::string attributes;       // Quell-Attribute als JSON-Text (v2 .zattrs)
  std::string dim_names;        // Quell-dimension_names als JSON-Text (v2 .zattrs)
  std::vector<std::int64_t> shape;
  const void *source = nullptr;  // opaker Quell-Store

  // Ausgabe (von tsApplyCoordinateConfig gesetzt)
  std::string directory;  // Zielpfad <out_path>/<name>/
  std::string driver;     // "zarr3", "n5" oder "zarr"
  std::string metadata;   // Store-Metadaten als JSON-Text
  std::shared_ptr<void> data;
};

// Ausgabe-Metadaten eines Datenarrays.  Der Quell-Teil wird von tsGetMetadata
// gefuellt (unabhaengig vom Output-Config), der Ausgabe-Teil von
// tsApplyMetadataConfig; tsSetMetadata legt daraus den Store an.
struct tsArrayMeta
{
  // Quelle
  std::string name;
  int format = 0;               // Quell-Zarr-Format (2 oder 3)
  std::string source_metadata;  // Roh-.zarray/zarr.json der Quelle (Kompression)
  std::string attributes;       // Quell-Attribute als JSON-Text (v2 .zattrs)
  std::string dim_names;        // Quell-dimension_names als JSON-Text (v2 .zattrs)
  std::vector<std::int64_t> bstart;
  std::vector<std::int64_t> bcount;
  std::vector<std::int64_t> out_chunks;

  // Ausgabe (von tsApplyMetadataConfig gesetzt)
  std::string driver;    // "zarr3", "n5" oder "zarr"
  std::string metadata;  // Store-Metadaten als JSON-Text
};

// Ausgabe-Metadaten des Schreibmodus: Zeitgrenzen und je Datenarray die
// Metadaten.  Wird von tsGetMetadata gefuellt und an tsSetMetadata uebergeben.
struct tsOutputMetadata
{
  std::int64_t start_t = 0;
  std::int64_t num_steps = 0;
  std::vector<tsArrayMeta> arrays;
};

// TensorStore-freier Teil des Dateizugriffs.  Der TensorStore-spezifische Teil
// (Context, Stores, Reader/Writer) wird in tsio.cpp als `tsHandleImpl` ergaenzt.
struct tsHandle
{
  tsConfig cfg;
  tsMetadata metadata;
  std::vector<std::string> out_dirs;

  std::int64_t start_t = 0;                                  // erster absoluter Schritt (kleinstes bstart[0])
  std::int64_t cur_t = 0;                                    // aktueller absoluter Schritt (Eingabe)
  std::size_t next_field = 0;                                // naechstes Feld des aktuellen Schritts
  std::vector<std::pair<std::size_t, std::int64_t>> fields;  // (Array, absoluter Layer) je Schritt
  tsField last_field;                                        // Feld, das tsGetField zuletzt geliefert hat
  bool have_field = false;

  // Ausgabe-Schrittzaehler (nur Schreibmodus).
  std::int64_t out_start = 0;  // erster absoluter Schritt der Ausgabe
  std::int64_t out_t = 0;      // aktueller absoluter Schreib-Schritt
  std::int64_t out_end = 0;    // ein hinter dem letzten Schreib-Schritt
  std::size_t sel_field = 0;   // zuletzt mit tsSetField gewaehltes Array
  std::int64_t sel_layer = 0;  // absoluter Layer des Schreibziels
  bool have_sel = false;

  bool fields_open = false;
  bool done = false;
};

// TensorStore-Version dieses Builds.
const char *tsVersion();

// Oeffnet alle in `config.arrays` gewaehlten Arrays und die benutzten
// Koordinaten-Arrays und liefert deren Metadaten.  `nullptr` bei Fehlern
// (Diagnose auf stderr).
tsHandle *tsOpenFileRead(const std::string &path, const tsConfig &config);
tsHandle *tsApplyConfig(tsHandle *h);

// Legt je gewaehltem Array einen Ausgabestore unter <path>/<name>/ an.
// `nullptr` bei Fehlern (Diagnose auf stderr).
tsHandle *tsOpenFileWrite(const std::string &path, const tsConfig &config);

// Metadaten des geoeffneten Datensatzes; gueltig bis tsCloseFile.  Nach
// tsResolveArray stehen darin auch die aufgeloesten Auswahlgrenzen.
tsMetadata &tsGetReadMetadata(tsHandle *handle);

// Liest Attribute, dimension_names und die Roh-Kompressionsmetadaten aller
// Datenarrays in `meta` ein (Reihenfolge wie tsMetadata::arrayList);
// unabhaengig vom Output-Config.  Nach tsOpenFields(`in`) aufrufen.  false bei
// Fehlern (Diagnose auf stderr).
bool tsGetMetadata(tsHandle *in, tsOutputMetadata &meta);

// Wendet das Ausgabe-Config (Format, dtype, Kompression) auf die von
// tsGetMetadata gelesenen Metadaten an und baut driver + Store-Metadaten.
void tsApplyMetadataConfig(const tsConfig &cfg, tsOutputMetadata &meta);

// Legt je Datenarray den Ausgabestore an und setzt die Schreibgrenzen.  Benoetigt
// nur das Output-Handle (Ergebnis von tsApplyMetadataConfig).  false bei Fehlern
// (Diagnose auf stderr).
bool tsSetMetadata(tsHandle *out, const tsOutputMetadata &meta);

// Loest die Auswahl (step/layer/cell) eines Arrays aus tsConfig gegen dessen
// Ausdehnung auf und fuellt bstart, bcount, chunks und out_chunks.  false, wenn
// der Startindex ausserhalb der Ausdehnung liegt (Diagnose auf stderr).
bool tsResolveArray(tsHandle *handle, std::size_t index);

// Oeffnet die Daten-Arrays als float32 und richtet die Lesepuffer ein; wird
// nach der Koordinaten-Pruefung aufgerufen, bevor Felder gelesen werden.
bool tsOpenFields(tsHandle *handle);

// Anzahl der Felder pro Zeitschritt (konstant: Summe der `num_layers` aller
// ausgewaehlten Daten-Arrays).  Erst nach tsOpenFields abfragen.
std::size_t tsGetNumFields(tsHandle *handle);

// Rueckt auf den naechsten Zeitschritt (absolute Nummer) und gibt ihn zurueck
// (0 = erster Schritt); -1, wenn keine weiteren Schritte mehr vorhanden sind.
// Setzt den Feldzeiger von tsGetField auf den Anfang des Schritts zurueck.
// Der zurueckgegebene Schritt gilt fuer alle Felder des folgenden
// Felddurchlaufs (tsField enthaelt keinen Schritt).
std::int64_t tsGetNextStep(tsHandle *handle);

// Liefert das naechste Feld des aktuellen Schritts (Array und absoluter Layer)
// bzw. false, wenn der Schritt keine weiteren Felder hat.
bool tsGetField(tsHandle *handle, tsField &field);

// Liest das soeben von tsGetField gelieferte Feld in `data` (Laenge
// arrayList[field.array].num_cells).  false ohne ausgewaehltes Feld.
bool tsReadField(tsHandle *handle, float *data, std::size_t n);

// Zeitschritt-Zaehler der Ausgabe: rueckt auf den naechsten absoluten
// Schreib-Schritt vor und gibt ihn zurueck (-1 am Ende); vor dem ersten Schritt
// aufgerufen.
std::int64_t tsSetNextStep(tsHandle *handle);

// Merkt sich das Ausgabeziel (Array und absoluter Layer) fuer tsWriteField.
bool tsSetField(tsHandle *handle, const tsField &field);

// Schreibt das zuvor mit tsSetField ausgewaehlte Feld in die Ausgabe.
bool tsWriteField(tsHandle *handle, const float *data, std::size_t n);

// Liest alle benutzten Koordinaten-Arrays in `coords` ein (Reihenfolge wie
// tsMetadata::coordList); unabhaengig vom Output-Config.  false bei Fehlern
// (Diagnose auf stderr).
bool tsReadCoordinates(tsHandle *in, std::vector<tsCoordinate> &coords);

// Wendet das Ausgabe-Config (Auswahl, Zielpfad, Format, Chunk-Overrides und
// Kompression) auf die von tsReadCoordinates gelesenen Koordinaten an und
// materialisiert die ausgewaehlten Werte.  false bei Fehlern (Diagnose auf
// stderr).
bool tsApplyCoordinateConfig(const tsConfig &cfg, std::vector<tsCoordinate> &coords);

// Schreibt die mit tsReadCoordinates gelesenen Koordinaten unter <out_path>/
// <name>/ mit Quell-Datentyp, Auswahl, Attributen und Kompression.
bool tsWriteCoordinates(tsHandle *out, const std::vector<tsCoordinate> &coords);

// Gibt den Zugriff frei.
void tsCloseFile(tsHandle *handle);

// Direkte Array-Namen unterhalb von `path`.
std::vector<std::string> tsListArrays(const tsConfig &config);

// Uebersicht des Datensatzes: Gruppen der Wurzel, sonst eine Zeile pro Array.
bool tsDatasetOverview(const tsConfig &config, tsOverview &overview);

bool ts_read_array(tsHandle *handle, const char *arrayName, double *buffer);

#endif  // TSIO_H
