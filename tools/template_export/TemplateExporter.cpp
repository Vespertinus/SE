
#include "TemplateExporter.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include <flatbuffers/flatbuffers.h>
#include <SceneTree_generated.h>
#include <EntityTemplate_generated.h>

namespace se_template_export {

namespace {

bool ReadFileBytes(const std::string & sPath, std::vector<uint8_t> & vOut) {

        std::ifstream oInput(sPath, std::ios::binary | std::ios::ate);
        if (!oInput.is_open()) {
                std::cerr << "template_export: cannot open '" << sPath << "'\n";
                return false;
        }
        const auto size = static_cast<size_t>(oInput.tellg());
        oInput.seekg(0);
        vOut.resize(size);
        oInput.read(reinterpret_cast<char *>(vOut.data()), static_cast<std::streamsize>(size));
        return oInput.good() || oInput.eof();
}

bool WriteFileBytes(const std::string & sPath, const std::vector<uint8_t> & vData) {

        std::ofstream oOutput(sPath, std::ios::binary | std::ios::trunc);
        if (!oOutput.is_open()) {
                std::cerr << "template_export: cannot write '" << sPath << "'\n";
                return false;
        }
        oOutput.write(reinterpret_cast<const char *>(vData.data()),
                        static_cast<std::streamsize>(vData.size()));
        return oOutput.good();
}

// SceneTree.fbs Node and EntityTemplate.fbs NodeTemplate share the same field set
// ("mirrors Node exactly") — copy is field-by-field through the object API.
std::unique_ptr<SE::FlatBuffers::NodeTemplateT> ConvertNode(const SE::FlatBuffers::NodeT & oSrc) {

        auto pDst = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        pDst->name        = oSrc.name;
        pDst->enabled     = oSrc.enabled;
        pDst->info        = oSrc.info;
        pDst->translation = oSrc.translation ? std::make_unique<SE::FlatBuffers::Vec3>(*oSrc.translation) : nullptr;
        pDst->rotation    = oSrc.rotation    ? std::make_unique<SE::FlatBuffers::Vec3>(*oSrc.rotation)    : nullptr;
        pDst->scale       = oSrc.scale       ? std::make_unique<SE::FlatBuffers::Vec3>(*oSrc.scale)       : nullptr;

        pDst->components.reserve(oSrc.components.size());
        for (const auto & pComp : oSrc.components) {
                pDst->components.push_back(pComp ? std::make_unique<SE::FlatBuffers::ComponentT>(*pComp) : nullptr);
        }
        pDst->children.reserve(oSrc.children.size());
        for (const auto & pChild : oSrc.children) {
                pDst->children.push_back(pChild ? ConvertNode(*pChild) : nullptr);
        }
        return pDst;
}

const SE::FlatBuffers::NodeT * FindNodeByName(const SE::FlatBuffers::NodeT & oRoot,
                const std::string & sName) {

        std::vector<const SE::FlatBuffers::NodeT *> vQueue { &oRoot };
        for (size_t i = 0; i < vQueue.size(); ++i) {
                const auto * pNode = vQueue[i];
                if (pNode->name == sName) {
                        return pNode;
                }
                for (const auto & pChild : pNode->children) {
                        if (pChild) vQueue.push_back(pChild.get());
                }
        }
        return nullptr;
}

// Offline flatten (variant semantics, mirroring the runtime merge): the export
// ROOT overlays the base root unconditionally (name + transform + structure —
// the variant's identity wins); deeper export nodes match base nodes by name
// (BFS, first match) and replace them; unmatched export children are appended.
// Plugin-dependent field overrides are a runtime-merge feature and are not
// applied here.
void OverlayNode(SE::FlatBuffers::NodeTemplateT & oBase, const SE::FlatBuffers::NodeT & oExport) {

        oBase.name        = oExport.name;
        oBase.enabled     = oExport.enabled;
        oBase.info        = oExport.info;
        oBase.translation = oExport.translation ? std::make_unique<SE::FlatBuffers::Vec3>(*oExport.translation) : nullptr;
        oBase.rotation    = oExport.rotation    ? std::make_unique<SE::FlatBuffers::Vec3>(*oExport.rotation)    : nullptr;
        oBase.scale       = oExport.scale       ? std::make_unique<SE::FlatBuffers::Vec3>(*oExport.scale)       : nullptr;
        oBase.components.clear();
        for (const auto & pComp : oExport.components) {
                oBase.components.push_back(pComp ? std::make_unique<SE::FlatBuffers::ComponentT>(*pComp) : nullptr);
        }

        // children: match by name anywhere under the (already overlaid) base root
        for (const auto & pChild : oExport.children) {
                if (!pChild) { continue; }

                SE::FlatBuffers::NodeTemplateT * pMatch = nullptr;
                std::vector<SE::FlatBuffers::NodeTemplateT *> vQueue{ &oBase };
                for (size_t i = 0; i < vQueue.size() && !pMatch; ++i) {
                        for (auto & pBaseChild : vQueue[i]->children) {
                                if (pBaseChild && pBaseChild->name == pChild->name) {
                                        pMatch = pBaseChild.get();
                                        break;
                                }
                                if (pBaseChild) { vQueue.push_back(pBaseChild.get()); }
                        }
                }

                if (pMatch) {
                        OverlayNode(*pMatch, *pChild);
                } else {
                        oBase.children.push_back(ConvertNode(*pChild));
                }
        }
}

} // namespace

bool ExportSubtree(const SOptions & oOptions) {

        if (oOptions.sNodeName.empty() || oOptions.sTemplateId.empty()) {
                std::cerr << "template_export: --node and --id are required\n";
                return false;
        }

        std::vector<uint8_t> vScene;
        if (!ReadFileBytes(oOptions.sScenePath, vScene)) {
                return false;
        }

        flatbuffers::Verifier oVerifier(vScene.data(), vScene.size());
        if (!SE::FlatBuffers::VerifySceneTreeBuffer(oVerifier)) {
                std::cerr << "template_export: scene verification failed for '"
                          << oOptions.sScenePath << "'\n";
                return false;
        }

        const auto * pSceneFB = SE::FlatBuffers::GetSceneTree(vScene.data());
        if (!pSceneFB || !pSceneFB->root()) {
                std::cerr << "template_export: scene has no root\n";
                return false;
        }

        // Unpack to the object API for the conversion walk.
        std::unique_ptr<SE::FlatBuffers::NodeT> pRootT(pSceneFB->root()->UnPack());
        if (!pRootT) {
                std::cerr << "template_export: failed to unpack scene root\n";
                return false;
        }

        const auto * pSubtree = FindNodeByName(*pRootT, oOptions.sNodeName);
        if (!pSubtree) {
                std::cerr << "template_export: node '" << oOptions.sNodeName
                          << "' not found in '" << oOptions.sScenePath << "'\n";
                return false;
        }

        auto pTemplateRoot = ConvertNode(*pSubtree);

        // Optional offline flatten against a base .setm.
        std::unique_ptr<SE::FlatBuffers::EntityTemplateT> pFlattened;
        if (!oOptions.sFlattenBase.empty()) {

                std::vector<uint8_t> vBase;
                if (!ReadFileBytes(oOptions.sFlattenBase, vBase)) {
                        return false;
                }
                flatbuffers::Verifier oBaseVerifier(vBase.data(), vBase.size());
                if (!SE::FlatBuffers::VerifyEntityTemplateBuffer(oBaseVerifier)) {
                        std::cerr << "template_export: base verification failed for '"
                                  << oOptions.sFlattenBase << "'\n";
                        return false;
                }
                const auto * pBaseFB = SE::FlatBuffers::GetEntityTemplate(vBase.data());
                if (!pBaseFB || !pBaseFB->root()) {
                        std::cerr << "template_export: base template has no root\n";
                        return false;
                }
                std::unique_ptr<SE::FlatBuffers::NodeTemplateT> pBaseRoot(pBaseFB->root()->UnPack());
                if (!pBaseRoot) {
                        std::cerr << "template_export: failed to unpack base root\n";
                        return false;
                }
                OverlayNode(*pBaseRoot, *pSubtree);
                pTemplateRoot = std::move(pBaseRoot);
        }

        // Build the .setm
        flatbuffers::FlatBufferBuilder fbb;
        auto oRootOffset = SE::FlatBuffers::NodeTemplate::Pack(fbb, pTemplateRoot.get());
        auto oIdOffset   = fbb.CreateString(oOptions.sTemplateId);

        std::vector<flatbuffers::Offset<flatbuffers::String>> vTagOffsets;
        for (const auto & sTag : oOptions.vTags) {
                vTagOffsets.push_back(fbb.CreateString(sTag));
        }
        auto oTagsOffset = fbb.CreateVector(vTagOffsets);

        SE::FlatBuffers::EntityTemplateBuilder oBuilder(fbb);
        oBuilder.add_id(oIdOffset);
        oBuilder.add_root(oRootOffset);
        oBuilder.add_tags(oTagsOffset);
        if (!oOptions.sBaseId.empty() && oOptions.sFlattenBase.empty()) {
                oBuilder.add_base_id(fbb.CreateString(oOptions.sBaseId));
        }
        auto oTemplateOffset = oBuilder.Finish();
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oTemplateOffset);

        std::vector<uint8_t> vOut(fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize());

        // Determinism guard: re-parse what we wrote and confirm it verifies.
        flatbuffers::Verifier oOutVerifier(vOut.data(), vOut.size());
        if (!SE::FlatBuffers::VerifyEntityTemplateBuffer(oOutVerifier)) {
                std::cerr << "template_export: internal error — output failed verification\n";
                return false;
        }

        const std::string sOut = oOptions.sOutPath.empty()
                        ? oOptions.sTemplateId + ".setm" : oOptions.sOutPath;
        if (!WriteFileBytes(sOut, vOut)) {
                return false;
        }

        std::cout << "template_export: wrote '" << sOut << "' (id='" << oOptions.sTemplateId
                  << "', " << vOut.size() << " bytes"
                  << (oOptions.sFlattenBase.empty()
                        ? (oOptions.sBaseId.empty() ? "" : ", base_id='" + oOptions.sBaseId + "'")
                        : ", flattened")
                  << ")\n";
        return true;
}

} // namespace se_template_export
