#include "Dialog.h"

using namespace rmlib;

void
DownloadDialog::State::init(AppContext& ctx, const BuildContext& buildCtx) {
  startTimer = ctx.addTimer(std::chrono::seconds(0), [&] {
    constexpr auto url =
      "http://web.archive.org/web/20240409191813id_/http://tiroms.weebly.com/"
      "uploads/1/1/0/5/110560031/ti84plus.rom";
    auto cmd = "wget -O '" + getWidget().romPath + "' " + url;

    std::cout << cmd << "\n";
    system(cmd.c_str());

    Navigator::of(buildCtx).pop();
  });
}

DownloadDialog::State
DownloadDialog::createState() {
  return State{};
}
