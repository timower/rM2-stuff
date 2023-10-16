#include "Calculator.h"

#include "Dialog.h"

#include <UI/Navigator.h>

using namespace rmlib;

namespace {

constexpr auto calc_save_extension = ".sav";

const auto FPS = 100;
const auto TPS = std::chrono::milliseconds(1000) / FPS;

} // namespace

Calculator::Calculator(std::string romPath)
  : romPath(romPath), savePath(romPath + calc_save_extension) {}

CalcState
Calculator::createState() const {
  return CalcState{};
}

void
CalcState::init(rmlib::AppContext& context,
                const rmlib::BuildContext& buildCtx) {
  mCalc = loadCalc();

  if (mCalc == nullptr) {
    popupTimer = context.addTimer(std::chrono::seconds(0), [&] {
      Navigator::of(buildCtx)
        .push([&romPath = getWidget().romPath] { return ErrorDialog(romPath); })
        .then([&buildCtx, &romPath = getWidget().romPath] {
          return Navigator::of(buildCtx).push(
            [&romPath] { return DownloadDialog(romPath); });
        })
        .then([this, &context, &buildCtx] {
          setState([&context, &buildCtx](auto& self) {
            self.init(context, buildCtx);
          });
        });
    });
    return;
  }

  std::cout << "loaded rom, entering mainloop\n";
  lastUpdateTime = std::chrono::steady_clock::now();
  updateTimer = context.addTimer(
    TPS, [this] { updateCalcState(); }, TPS);
}

CalcState::~CalcState() {
  if (mCalc == nullptr) {
    return;
  }

  std::cout << "Saving state to: " + getWidget().savePath + "\n";
  auto* save = fopen(getWidget().savePath.c_str(), "w");
  if (save == nullptr) {
    perror("Error opening save file");
  } else {
    tilem_calc_save_state(mCalc, nullptr, save);
    fclose(save);
  }

  tilem_calc_free(mCalc);
}

TilemCalc*
CalcState::loadCalc() const {
  FILE* rom = fopen(getWidget().romPath.c_str(), "r");
  if (rom == nullptr) {
    perror("Error opening rom file");
    return nullptr;
  }

  FILE* save = fopen(getWidget().savePath.c_str(), "r");
  if (save == nullptr) {
    perror("No save");
  }

  auto* result = tilem_calc_new(TILEM_CALC_TI84P);
  if (result == nullptr) {
    std::cerr << "Error init Calc\n";
    return nullptr;
  }

  if (tilem_calc_load_state(result, rom, save) != 0) {
    perror("Error loading rom or save");

    fclose(rom);
    if (save != nullptr) {
      fclose(save);
    }
    return nullptr;
  }

  fclose(rom);
  if (save != nullptr) {
    fclose(save);
  }

  return result;
}

void
CalcState::updateCalcState() {
  const auto time = std::chrono::steady_clock::now();
  auto diff = time - lastUpdateTime;

  // Skip frames if we were paused.
  if (diff > std::chrono::seconds(1)) {
    std::cout << "Skipping frames...\n";
    diff = TPS;
  }

  tilem_z80_run_time(
    mCalc,
    std::chrono::duration_cast<std::chrono::microseconds>(diff).count(),
    nullptr);

  lastUpdateTime = time;
}
