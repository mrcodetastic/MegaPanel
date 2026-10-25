#include "OutputRegistry.hpp"

#include <cstring>
#include "IOutput.hpp"
#include "outputs/ambient/AmbientCustomPca.hpp"
#include "outputs/ambient/AmbientPatterns.hpp"
#include "outputs/clocks/AnimatedClocks.hpp"
#include "outputs/clocks/ClockMorphDigits.hpp"
#include "outputs/clocks/ClockStandard.hpp"
#include "outputs/clocks/StaticClocks.hpp"
#include "outputs/media/AssetOutput.hpp"
#include "outputs/media/CanvasOutput.hpp"
#include "outputs/media/ImageOutput.hpp"
#include "outputs/media/NewsOutput.hpp"
#include "outputs/media/PcaOutput.hpp"
#include "outputs/media/TickerOutput.hpp"
#include "outputs/system/NotifyOverlay.hpp"
#include "outputs/system/StatusCorners.hpp"

namespace {

ClockStandard gClockStandard;
ClockMorphDigits gClockMorphDigits;
ClockLarge gClockLarge;
ClockWords gClockWords;
ClockWorldMapCw gClockWorldMapCw;
ClockMarioCw gClockMarioCw;
ClockPacmanCw gClockPacmanCw;
ClockMario gClockMario;
ClockSpace gClockSpace;
ClockPong gClockPong;
ClockPacman gClockPacman;
ClockSnake gClockSnake;
ClockTetris gClockTetris;
ClockAsteroids gClockAsteroids;
ClockDino gClockDino;
ClockMatrix gClockMatrix;
ClockWeather gClockWeather;
ClockWorld gClockWorld;
ClockBomberman gClockBomberman;
ClockTron gClockTron;
AmbientStarfield gStarfield;
AmbientPlasma gPlasma;
AmbientRain gRain;
AmbientMatrix gMatrix;
AmbientFire gFire;
AmbientWave gWave;
AmbientSpiro gSpiro;
AmbientMandala gMandala;
AmbientFireworks gFireworks;
AmbientStars gAmbientStars;
AmbientInvaders gAmbientInvaders;
AmbientPacmanChase gAmbientPacmanChase;
AmbientCustomPca gAmbientCustomPca;
TickerOutput gTicker;
PcaOutput gPca;
ImageOutput gImage;
AssetOutput gAsset;
CanvasOutput gCanvas;
NewsOutput gNews;
NotifyOverlay gNotify;
StatusCorners gStatusCorners;

const OutputDescriptor kOutputs[] = {
    {&gClockStandard, &ClockStandard::metaStatic, &ClockStandard::schemaStatic},
    {&gClockMorphDigits, &ClockMorphDigits::metaStatic, &ClockMorphDigits::schemaStatic},
    {&gClockLarge, &ClockLarge::metaStatic, &ClockLarge::schemaStatic},
    {&gClockWords, &ClockWords::metaStatic, &ClockWords::schemaStatic},
    {&gClockWorldMapCw, &ClockWorldMapCw::metaStatic, &ClockWorldMapCw::schemaStatic},
    {&gClockMarioCw, &ClockMarioCw::metaStatic, &ClockMarioCw::schemaStatic},
    {&gClockPacmanCw, &ClockPacmanCw::metaStatic, &ClockPacmanCw::schemaStatic},
    {&gClockMario, &ClockMario::metaStatic, &ClockMario::schemaStatic},
    {&gClockSpace, &ClockSpace::metaStatic, &ClockSpace::schemaStatic},
    {&gClockPong, &ClockPong::metaStatic, &ClockPong::schemaStatic},
    {&gClockPacman, &ClockPacman::metaStatic, &ClockPacman::schemaStatic},
    {&gClockSnake, &ClockSnake::metaStatic, &ClockSnake::schemaStatic},
    {&gClockTetris, &ClockTetris::metaStatic, &ClockTetris::schemaStatic},
    {&gClockAsteroids, &ClockAsteroids::metaStatic, &ClockAsteroids::schemaStatic},
    {&gClockDino, &ClockDino::metaStatic, &ClockDino::schemaStatic},
    {&gClockMatrix, &ClockMatrix::metaStatic, &ClockMatrix::schemaStatic},
    {&gClockWeather, &ClockWeather::metaStatic, &ClockWeather::schemaStatic},
    {&gClockWorld, &ClockWorld::metaStatic, &ClockWorld::schemaStatic},
    {&gClockBomberman, &ClockBomberman::metaStatic, &ClockBomberman::schemaStatic},
    {&gClockTron, &ClockTron::metaStatic, &ClockTron::schemaStatic},
    {&gStarfield, &AmbientStarfield::metaStatic, &AmbientStarfield::schemaStatic},
    {&gPlasma, &AmbientPlasma::metaStatic, &AmbientPlasma::schemaStatic},
    {&gRain, &AmbientRain::metaStatic, &AmbientRain::schemaStatic},
    {&gMatrix, &AmbientMatrix::metaStatic, &AmbientMatrix::schemaStatic},
    {&gFire, &AmbientFire::metaStatic, &AmbientFire::schemaStatic},
    {&gWave, &AmbientWave::metaStatic, &AmbientWave::schemaStatic},
    {&gSpiro, &AmbientSpiro::metaStatic, &AmbientSpiro::schemaStatic},
    {&gMandala, &AmbientMandala::metaStatic, &AmbientMandala::schemaStatic},
    {&gFireworks, &AmbientFireworks::metaStatic, &AmbientFireworks::schemaStatic},
    {&gAmbientStars, &AmbientStars::metaStatic, &AmbientStars::schemaStatic},
    {&gAmbientInvaders, &AmbientInvaders::metaStatic, &AmbientInvaders::schemaStatic},
    {&gAmbientPacmanChase, &AmbientPacmanChase::metaStatic, &AmbientPacmanChase::schemaStatic},
    {&gAmbientCustomPca, &AmbientCustomPca::metaStatic, &AmbientCustomPca::schemaStatic},
    {&gTicker, &TickerOutput::metaStatic, &TickerOutput::schemaStatic},
    {&gPca, &PcaOutput::metaStatic, &PcaOutput::schemaStatic},
    {&gImage, &ImageOutput::metaStatic, &ImageOutput::schemaStatic},
    {&gAsset, &AssetOutput::metaStatic, &AssetOutput::schemaStatic},
    {&gCanvas, &CanvasOutput::metaStatic, &CanvasOutput::schemaStatic},
    {&gNews, &NewsOutput::metaStatic, &NewsOutput::schemaStatic},
    {&gNotify, &NotifyOverlay::metaStatic, &NotifyOverlay::schemaStatic},
    {&gStatusCorners, &StatusCorners::metaStatic, &StatusCorners::schemaStatic},
};

constexpr size_t kOutputCount = sizeof(kOutputs) / sizeof(kOutputs[0]);

}  // namespace

const OutputRegistry& OutputRegistry::instance() {
  static const OutputRegistry registry;
  return registry;
}

IOutput* OutputRegistry::find(const char* id) const {
  if (!id || !*id) return nullptr;
  for (size_t i = 0; i < kOutputCount; ++i) {
    if (strcmp(kOutputs[i].instance->meta().id, id) == 0) {
      return kOutputs[i].instance;
    }
  }
  return nullptr;
}

size_t OutputRegistry::count() const { return kOutputCount; }

const OutputDescriptor& OutputRegistry::at(size_t index) const {
  return kOutputs[index < kOutputCount ? index : 0];
}
