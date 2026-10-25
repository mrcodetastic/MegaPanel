#pragma once

#include "outputs/media/PcaOutput.hpp"

// Background-layer PCA playback. Same decoder and scaling as media.pca; the
// only differences are the id, the background category and the fact that the
// SceneManager never gives it the foreground.
class AmbientCustomPca final : public PcaOutput {
 public:
  const OutputMeta& meta() const override { return metaStatic(); }

  static const OutputMeta& metaStatic() {
    static const OutputMeta m = {
        "ambient.custom_pca",
        "PCA background",
        "1.0",
        "Ported from animatedpixelclock anim_store.cpp",
        OutputCategory::BackgroundPattern,
        LAYER_BG,
        LayerId::Background,
        ScreenSpec{128, 64},
        ScreenSpec{16, 16},
        0,
        0,
        30,
        20,
        true,
        false,
        false,
        false,
        true,
        false};
    return m;
  }

  void buildConfigSchema(JsonObject& out) const override { PcaOutput::schemaStatic(out); }
};
