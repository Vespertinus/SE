
// template_export — extract a .sesc subtree into a .setm template.
//
// Usage:
//   template_export <scene.sesc> --node <name> --id <template_id>
//                   [--out <file>] [--base <base_id>] [--tags a,b]
//                   [--flatten <base.setm>]
//
// The exported subtree keeps its authored hierarchy, transforms, components and
// enabled flags. --flatten merges the export over a base .setm offline (nodes
// matched by name) and drops base_id, so the runtime never pays merge cost.

#include <iostream>
#include <string>
#include <vector>

#include "TemplateExporter.h"

int main(int argc, char ** argv) {

        if (argc < 2) {
                std::cerr << "usage: template_export <scene.sesc> --node <name> --id <template_id>"
                          << " [--out <file>] [--base <base_id>] [--tags a,b] [--flatten <base.setm>]\n";
                return 1;
        }

        se_template_export::SOptions oOptions;
        oOptions.sScenePath = argv[1];

        for (int i = 2; i < argc; ++i) {
                const std::string sArg = argv[i];
                auto need_value = [&](const char * pFlag) -> std::string {
                        if (i + 1 >= argc) {
                                std::cerr << "template_export: " << pFlag << " requires a value\n";
                                std::exit(1);
                        }
                        return argv[++i];
                };

                if (sArg == "--node")        { oOptions.sNodeName    = need_value("--node"); }
                else if (sArg == "--id")     { oOptions.sTemplateId  = need_value("--id"); }
                else if (sArg == "--out")    { oOptions.sOutPath     = need_value("--out"); }
                else if (sArg == "--base")   { oOptions.sBaseId      = need_value("--base"); }
                else if (sArg == "--flatten"){ oOptions.sFlattenBase = need_value("--flatten"); }
                else if (sArg == "--tags") {
                        const std::string sTags = need_value("--tags");
                        std::string sCur;
                        for (const char c : sTags) {
                                if (c == ',') {
                                        if (!sCur.empty()) oOptions.vTags.push_back(sCur);
                                        sCur.clear();
                                } else {
                                        sCur.push_back(c);
                                }
                        }
                        if (!sCur.empty()) oOptions.vTags.push_back(sCur);
                }
                else {
                        std::cerr << "template_export: unknown argument '" << sArg << "'\n";
                        return 1;
                }
        }

        return se_template_export::ExportSubtree(oOptions) ? 0 : 1;
}
