#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <Kotonoha/core/Engine.hpp>
#include <Kotonoha/mcp/MCPServer.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalidSocket = INVALID_SOCKET;
void closeSocket(SocketHandle socket) { closesocket(socket); }
#else
using SocketHandle = int;
constexpr SocketHandle invalidSocket = -1;
void closeSocket(SocketHandle socket) { close(socket); }
#endif

void require(bool ok, const char *reason) {
  if (!ok)
    throw std::runtime_error(reason);
}

struct SocketStartup {
  SocketStartup() {
#ifdef _WIN32
    WSADATA data{};
    require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock startup");
#endif
  }
  ~SocketStartup() {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};

struct Socket {
  SocketHandle handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  Socket() { require(handle != invalidSocket, "Create TCP socket"); }
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;
  ~Socket() {
    if (handle != invalidSocket)
      closeSocket(handle);
  }
  void shutdown() {
    if (handle != invalidSocket) {
      closeSocket(handle);
      handle = invalidSocket;
    }
  }
};

sockaddr_in loopback(unsigned short port) {
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  local.sin_port = htons(port);
  return local;
}

unsigned short freeLoopbackPort() {
  Socket probe;
  auto local = loopback(0);
  require(bind(probe.handle, reinterpret_cast<const sockaddr *>(&local),
               sizeof(local)) == 0,
          "Bind local test port");
#ifdef _WIN32
  int length = sizeof(local);
#else
  socklen_t length = sizeof(local);
#endif
  require(getsockname(probe.handle, reinterpret_cast<sockaddr *>(&local),
                      &length) == 0,
          "Discover local test port");
  return ntohs(local.sin_port);
}

void connectTo(Socket &client, unsigned short port) {
  const auto local = loopback(port);
  require(connect(client.handle, reinterpret_cast<const sockaddr *>(&local),
                  sizeof(local)) == 0,
          "Connect local MCP client");
#ifdef _WIN32
  DWORD timeout = 2000;
#else
  timeval timeout{2, 0};
#endif
  require(setsockopt(client.handle, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char *>(&timeout),
                     sizeof(timeout)) == 0,
          "Set test socket timeout");
}

void sendLine(Socket &client, const std::string &line) {
  const auto message = line + "\n";
  std::size_t offset = 0;
  while (offset < message.size()) {
    const auto sent = ::send(client.handle, message.data() + offset,
                             static_cast<int>(message.size() - offset), 0);
    require(sent > 0, "Send MCP request");
    offset += static_cast<std::size_t>(sent);
  }
}

std::string receiveLine(Socket &client) {
  std::string response;
  char bytes[1024];
  while (response.find('\n') == std::string::npos) {
    const auto read = recv(client.handle, bytes, sizeof(bytes), 0);
    require(read > 0 && response.size() + static_cast<std::size_t>(read) <= 65536,
            "Receive bounded MCP response");
    response.append(bytes, static_cast<std::size_t>(read));
  }
  return response.substr(0, response.find('\n'));
}

struct EmptyRuntime : Kotonoha::Runtime {
  EmptyRuntime() : Runtime("MCP Transport Test", "1", "mcp.transport.test") {}
  void init(Kotonoha::Engine &) override {}
};

void testSocket() {
  SocketStartup network;
  const auto port = freeLoopbackPort();
  EmptyRuntime runtime;
  Kotonoha::EngineOptions options;
  options.rendererOverride = "software";
  Kotonoha::Engine engine(runtime, options);
  Kotonoha::MCPServer server(engine, std::filesystem::current_path().string(),
                             port);

  Socket first;
  connectTo(first, port);
  sendLine(first, R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})");
  require(server.pump(), "TCP MCP must not exit the engine");
  require(receiveLine(first).find("kotonoha-engine") != std::string::npos,
          "TCP initializes using the native engine dispatcher");

  sendLine(first, R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
  sendLine(first, R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  require(server.pump(), "TCP MCP handles multiple requests per tick");
  require(receiveLine(first).find("engine_state") != std::string::npos,
          "TCP exposes native engine MCP tools");

  first.shutdown();
  Socket second;
  connectTo(second, port);
  sendLine(second, R"({"jsonrpc":"2.0","id":3,"method":"tools/list"})");
  // Reconnect before pumping the closed first socket. Accept-before-read
  // used to reset this connection with WSAECONNRESET on Windows.
  // Pump several frames without waiting: TCP FIN delivery can lag the new
  // connection, which must stay queued instead of being reset in that case.
  for (int frame = 0; frame < 32; ++frame)
    require(server.pump(), "TCP MCP accepts immediate reconnect without exiting");
  require(receiveLine(second).find("-32002") != std::string::npos,
          "Immediate reconnect must remain open with fresh MCP handshake");
  sendLine(second, R"({"jsonrpc":"2.0","id":4,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"reconnect-test","version":"1"}}})");
  require(server.pump(), "Reconnected client can initialize without sleep");
  require(receiveLine(second).find("kotonoha-engine") != std::string::npos,
          "Reconnected client shares the native MCP dispatcher");
}
} // namespace

int main() {
  try {
    testSocket();
    return 0;
  } catch (const std::exception &) {
    return 1;
  }
}
