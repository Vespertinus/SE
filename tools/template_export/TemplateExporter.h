
#ifndef TEMPLATE_EXPORTER_H
#define TEMPLATE_EXPORTER_H 1

// .sesc subtree → .setm template exporter (offline tool).
// Extraction logic lives here (no engine includes) so tests can link it directly.

#include <cstdint>
#include <string>
#include <vector>

namespace se_template_export {

struct SOptions {
        std::string              sScenePath;    ///< input .sesc
        std::string              sNodeName;     ///< subtree root (BFS, first match)
        std::string              sTemplateId;   ///< .setm id field
        std::string              sOutPath;      ///< default: <template_id>.setm
        std::string              sBaseId;       ///< optional base_id field (chained merge)
        std::vector<std::string> vTags;         ///< optional authoring tags
        std::string              sFlattenBase;  ///< optional base .setm: merge offline, drop base_id
};

/// Export the named subtree of sOptions.sScenePath as a .setm template.
/// With sFlattenBase set: loads the base template, applies the export as a
/// same-name-node overlay (transform/enabled/structure), and writes a standalone
/// template without base_id. Returns false and prints an error on failure.
bool ExportSubtree(const SOptions & oOptions);

} // namespace se_template_export

#endif
