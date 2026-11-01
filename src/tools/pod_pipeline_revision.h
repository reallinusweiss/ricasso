#pragma once
// pod_pipeline_revision.h — the one number that says "a .POD written by an older
// converter is not the same asset as one written today".
//
// A .POD file carries no provenance: the format's only version field is the
// PowerVR "AB.POD.2.0" string the game engine parses, so there is nowhere in
// the bytes to record which importer produced them. That gap is not academic.
// smario/.../soldier-v1 shipped a base model baked by a pre-rebase converter
// whose mesh sat at 56% of the size the glTF spec binds it at, 0.39 m low, and
// it passed every self-consistency check we had, because every one of those
// checks compared the POD to itself. See pod_fbx_gltf_interconversion_report.md
// §8b.4.
//
// So the converter writes a sidecar stamp (<file>.POD.meta) holding this
// revision, and pod_loader warns when it loads a POD stamped with a different
// one. Nothing is added to the POD bytes, so the game engine sees exactly the
// file it saw before.
//
// BUMP THIS whenever a change to the POD import/convert path makes previously
// written .POD files wrong — a change to vertex space, bone selection,
// animation sampling, unit handling, or the bind pose. Not for changes that
// only affect textures or diagnostics. Bumping it is what turns "the model
// looks broken" into a one-line warning naming the file and telling you to
// re-convert.
//   4 -> 5: node transforms moved off tag 5010, which the runtime reader
//           discards (`case 0x1392u: goto LABEL_202;`), onto 5011. Every POD
//           written before this had matrix-only nodes — i.e. every Sketchfab
//           GLB export, including the stock mh-60l_dap_usa — with no transform
//           at all, which the game rendered as a collapsed "spirit sheet"
//           while ruby_gg (whose loader does read 5010) looked correct. The
//           mesh section also gained the stock 6005/6008/6009/6011/6020 tags.
//   5 -> 6: the block grammar now matches the stock writer exactly. Every
//           leaf carries its `<tag|0x80000000> <0>` close pair (previously only
//           containers did), empty CPODData streams no longer carry a
//           meaningless 4-byte 9003 payload, the top-level 1002/1003 exporter
//           blocks are present, scene children are ordered
//           materials/meshes/nodes/textures, and materials carry the full
//           stock tag set — in particular the nine auxiliary texture slots,
//           which previously fell through to calloc'd 0 (a valid texture
//           index) instead of stock's -1 sentinel. Verified by structurally
//           diffing our output against resources/rock1.POD.
#define SWORDIGO_POD_PIPELINE_REVISION 6
