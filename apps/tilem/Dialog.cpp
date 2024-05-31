#include "Dialog.h"

using namespace rmlib;

void
DownloadDialog::State::init(AppContext& ctx, const BuildContext& buildCtx) {
  startTimer = ctx.addTimer(std::chrono::seconds(0), [&] {
    constexpr auto url =
      "http://ipfs.io/ipfs/QmSNmqjQ1Ao4jff9pXbzv98ebuCzKf2ZuyEpSGL31D8z44";
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
