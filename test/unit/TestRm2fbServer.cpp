#include <catch2/catch_test_macros.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "ServerInternal.h"

namespace {

// Records suspend/resume calls instead of touching real hardware. Note:
// on a dev/CI box without a real /dev/fb0, Server::inQemu is always true,
// so Server never actually calls suspendForXochitl()/resumeForXochitl()
// regardless of what this records - see the comment on Server::resume()'s
// "focus tracks client-driven-ness unconditionally" for why that's still
// fine to assert on via `focus` itself.
struct FakeAddressInfo : AddressInfoBase {
  void initThreads() const override {}
  bool doUpdate(const UpdateParams&) const override { return true; }
  void shutdownThreads() const override {}
  bool installHooks(UpdateFn*) const override { return true; }

  mutable int suspendCount = 0;
  mutable int resumeCount = 0;
  void suspendForXochitl() const override { suspendCount++; }
  void resumeForXochitl() const override { resumeCount++; }
};

// Records what pause()/resume() would have sent instead of sending a real
// SIGSTOP/SIGCONT/SIGUSR1 - see ProcessControl's own comment. Without
// this, testing pause()/resume() at all would need a real, safely-
// signalable process to point them at (this test used to fork one; that
// turned out to need its own process-group isolation and inherited-fd
// cleanup to avoid colliding with whatever other tests in this same
// binary happen to be doing - not worth it just to satisfy a kill() call
// nothing here actually needs to be real).
struct FakeProcessControl : ProcessControl {
  mutable std::vector<std::pair<pid_t, int>> signals;

  void pause(pid_t pid, bool dontPause) const override {
    signals.emplace_back(pid, dontPause ? SIGUSR1 : SIGSTOP);
  }
  void resume(pid_t pid) const override { signals.emplace_back(pid, SIGCONT); }
};

// A bare-bones stand-in for Client.cpp/ClientSwtcon.cpp's own doInit() -
// speaks the same raw UnixClientMsg protocol without going through either
// client library.
struct FakeClient {
  unistdpp::FD sock;

  explicit FakeClient(const char* serverPath) {
    sock = unistdpp::fatalOnError(
      unistdpp::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0), "socket");
    unistdpp::fatalOnError(
      unistdpp::bind(sock, unistdpp::Address::fromUnixPath(nullptr)), "bind");
    unistdpp::fatalOnError(
      unistdpp::connect(sock, unistdpp::Address::fromUnixPath(serverPath)),
      "connect");
  }

  void sendInit(bool ownSwtcon) {
    auto res =
      sendMessage(sock, UnixClientMsg{ Init{ .ownSwtcon = ownSwtcon } });
    REQUIRE(res.has_value());
  }

  void sendIdleUpdate(bool idle) {
    auto res = sendMessage(sock, UnixClientMsg{ IdleUpdate{ idle } });
    REQUIRE(res.has_value());
  }

  // Reads back the granted format followed by the buffer fd the server
  // sends in reply to Init - see Buffer::recv(), which this mirrors
  // without needing a whole Buffer.
  unistdpp::FD recvBuffer() {
    auto format = sock.readAll<FbFormat>();
    REQUIRE(format.has_value());
    auto fd = unistdpp::recvFD(sock);
    REQUIRE(fd.has_value());
    return unistdpp::FD(*fd);
  }
};

// Server::poll()'s blocking ::poll() can be interrupted by a signal
// (EINTR) - e.g. a SIGCHLD from some other test's own subprocess
// elsewhere in this test binary - in which case it just logs and returns
// without processing anything. The real daemon's own outer loop
// (serverMain()) just calls poll() again on the next iteration when that
// happens, which is harmless there since nothing else runs in between;
// do the same here instead of asserting after a single call, which was
// flaky for exactly this reason.
template<typename Pred>
void
pollUntil(Server& server, Pred ready) {
  for (int i = 0; i < 50 && !ready(); i++) {
    server.poll();
  }
}

} // namespace

// Regression coverage for two related bugs in how the server used to
// decide a client owns its own independent swtcon (and so should get
// suspendForXochitl()/its own ClientDrivenFront panel ownership):
//  1. Detection used to be by process name ("xochitl") at accept() time -
//     which also (wrongly) matched the older by-address-hooking xochitl
//     client (Client.cpp), which does NOT run its own swtcon and relies
//     on this server driving updates for it like any other client.
//  2. Even for a client that genuinely does own its own swtcon, resume()
//     ran synchronously inside acceptUnixClient() - before the client's
//     Init (which is what actually carries this information) could
//     possibly have arrived - so the decision was made on data that
//     wasn't there yet.
// Both are now fixed by deriving it purely from the client's own Init
// message, and only ever switching a client to front once that Init has
// been processed (see readUnixSock()/requestSwitch() in ServerInternal.h).
TEST_CASE("Server client-driven front is derived from Init not accept",
          "[rm2fb][rm2fb-server]") {
  const char* lockPath = "/tmp/rm2fb-test-server.lock";
  const char* sockPath = "/tmp/rm2fb-test-server.sock";
  unlink(sockPath);

  FakeAddressInfo fakeAddrs;
  FakeProcessControl fakeProcessControl;
  Server server(&fakeAddrs, lockPath, sockPath, fakeProcessControl);

  // Nothing connected yet.
  REQUIRE(std::holds_alternative<NoFront>(server.focus));

  SECTION("old hooking-based client (Init.ownSwtcon=false) never becomes "
          "client-driven") {
    FakeClient client(sockPath);

    // Wait for the server to accept() before sending Init, to also
    // exercise the accept-then-Init ordering explicitly.
    pollUntil(server, [&] { return !server.unixClients.empty(); });
    REQUIRE(server.unixClients.size() == 1);
    REQUIRE(std::holds_alternative<NoFront>(server.focus));

    client.sendInit(/* ownSwtcon= */ false);
    pollUntil(server,
              [&] { return !std::holds_alternative<NoFront>(server.focus); });

    REQUIRE(std::holds_alternative<ServerDrivenFront>(server.focus));
    REQUIRE(focusPid(server.focus) == getpid());
    REQUIRE(server.unixClients[0].ownSwtcon == false);

    // Never asked to suspend this server's own swtcon for a client that
    // doesn't own one.
    REQUIRE(fakeAddrs.suspendCount == 0);
  }

  SECTION("swtcon-owning client only becomes client-driven once its Init "
          "actually arrives") {
    FakeClient client(sockPath);

    // accept() alone must not decide anything yet - this is exactly bug 2
    // above: resume() used to run here, before Init, with no way to know
    // ownSwtcon yet.
    pollUntil(server, [&] { return !server.unixClients.empty(); });
    REQUIRE(server.unixClients.size() == 1);
    REQUIRE(std::holds_alternative<NoFront>(server.focus));
    REQUIRE(!server.unixClients[0].ownSwtcon.has_value());

    client.sendInit(/* ownSwtcon= */ true);
    pollUntil(server,
              [&] { return !std::holds_alternative<NoFront>(server.focus); });

    REQUIRE(std::holds_alternative<ClientDrivenFront>(server.focus));
    REQUIRE(focusPid(server.focus) == getpid());
    REQUIRE(server.unixClients[0].ownSwtcon == true);
  }

  // Whichever section ran, resume() should have sent exactly one SIGCONT
  // to this (fake) client and nothing else - no pause() was ever due,
  // since this is the only client to ever connect.
  REQUIRE(fakeProcessControl.signals ==
          std::vector<std::pair<pid_t, int>>{ { getpid(), SIGCONT } });
}

// Regression: resume() used to hardcode a fresh ClientDrivenFront's idle to
// false, even though pause() only ever runs once idle was confirmed true -
// so a client that did nothing new after being resumed had no way to ever
// report idle again, and a later switch away from it deferred forever. Idle
// now lives on UnixClient (survives pause()/resume() on its own) instead of
// being reset on every fresh ClientDrivenFront - see requestSwitch().
TEST_CASE("Client-driven front keeps its confirmed idle state across "
          "pause()/resume()",
          "[rm2fb][rm2fb-server]") {
  const char* lockPath = "/tmp/rm2fb-test-server-idle.lock";
  const char* sockPath = "/tmp/rm2fb-test-server-idle.sock";
  unlink(sockPath);

  FakeAddressInfo fakeAddrs;
  FakeProcessControl fakeProcessControl;
  Server server(&fakeAddrs, lockPath, sockPath, fakeProcessControl);

  FakeClient client(sockPath);
  pollUntil(server, [&] { return !server.unixClients.empty(); });
  client.sendInit(/* ownSwtcon= */ true);
  pollUntil(server, [&] {
    return std::holds_alternative<ClientDrivenFront>(server.focus);
  });

  // Confirm idle, exactly like xochitl reaching its real 3s blank.
  client.sendIdleUpdate(true);
  pollUntil(server, [&] { return server.unixClients[0].idle; });

  // pause()+resume() on the same client, exactly what requestSwitch() does
  // when switching away and back - with no fresh IdleUpdate in between,
  // matching xochitl doing nothing new after being resumed.
  server.pause(getpid());
  server.resume(server.unixClients[0]);
  REQUIRE(server.unixClients[0].idle);

  // The actual observable bug: a switch request right after resuming, with
  // no update in between, must not get deferred.
  server.requestSwitch(getpid());
  auto* front = std::get_if<ClientDrivenFront>(&server.focus);
  REQUIRE(front != nullptr);
  REQUIRE(!front->pendingSwitchTarget.has_value());
}

// Regression: the server used to withhold a client's buffer-fd reply until
// resume() actually promoted it to front - a client whose switch is
// deferred behind a still-busy ClientDrivenFront (see
// ClientDrivenFront::pendingSwitchTarget) would then never get a reply at
// all until that front went idle. It now replies as soon as Init is
// processed (readUnixSock()), independently of when - or whether -
// requestSwitch() actually promotes this client.
TEST_CASE("Server replies to Init immediately, even when the switch itself "
          "is deferred",
          "[rm2fb][rm2fb-server]") {
  const char* lockPath = "/tmp/rm2fb-test-server-init-reply.lock";
  const char* sockPath = "/tmp/rm2fb-test-server-init-reply.sock";
  unlink(sockPath);

  FakeAddressInfo fakeAddrs;
  FakeProcessControl fakeProcessControl;
  Server server(&fakeAddrs, lockPath, sockPath, fakeProcessControl);

  // Client A takes front via the real accept()/Init path and never reports
  // idle, so it stays "busy" from the server's point of view.
  FakeClient clientA(sockPath);
  pollUntil(server, [&] { return !server.unixClients.empty(); });
  clientA.sendInit(/* ownSwtcon= */ true);
  pollUntil(server, [&] {
    return std::holds_alternative<ClientDrivenFront>(server.focus);
  });
  clientA.recvBuffer();

  // Client B is injected directly into unixClients with a distinct fake
  // pid instead of going through a second real connect() - acceptUnixClient
  // derives pid from SO_PEERCRED, and a second socket from this same test
  // process would get the identical real pid as A, tripping its accept-time
  // duplicate-pid handling instead of giving us two independent clients.
  // readUnixSock() itself only ever cares about the UnixClient reference
  // it's given, not how it ended up in unixClients.
  int fds[2];
  REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) == 0);
  unistdpp::FD clientBTestSide(fds[1]);
  UnixClient clientBEntry;
  clientBEntry.sock = unistdpp::FD(fds[0]);
  clientBEntry.pid = getpid() + 1;
  clientBEntry.dontPause = false;
  server.unixClients.push_back(std::move(clientBEntry));
  UnixClient& clientB = server.unixClients.back();

  REQUIRE(
    sendMessage(clientBTestSide, UnixClientMsg{ Init{ .ownSwtcon = false } })
      .has_value());
  server.readUnixSock(clientB);

  // B's switch must have been deferred - A is still front and still busy.
  auto* front = std::get_if<ClientDrivenFront>(&server.focus);
  REQUIRE(front != nullptr);
  REQUIRE(front->pid == getpid());
  REQUIRE(front->pendingSwitchTarget == clientB.pid);

  // Yet B must already have received its granted format and own buffer
  // fd, sent right at Init instead of waiting for a resume() that never
  // happened.
  auto receivedFormat = clientBTestSide.readAll<FbFormat>();
  REQUIRE(receivedFormat.has_value());
  auto receivedFd = unistdpp::recvFD(clientBTestSide);
  REQUIRE(receivedFd.has_value());
  unistdpp::FD received(*receivedFd);
  REQUIRE(received.isValid());
  struct stat st{};
  REQUIRE(fstat(received.fd, &st) == 0);
  REQUIRE(st.st_size == total_size);
}
