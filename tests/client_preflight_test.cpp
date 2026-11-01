#include "platform/client_preflight.h"
#include <array>
#include <iostream>
#include <stdexcept>

static void require(bool ok) { if (!ok) throw std::runtime_error("preflight regression"); }
int main() {
    namespace fs = std::filesystem;
    // Isolated in the test working directory; no user saves/game files touched.
    const auto root = fs::current_path() / fs::u8path(u8"client fixtures ä");
    fs::create_directories(root / "assets" / "resources");
    auto game = root / "game.so";
    auto sre = root / "sre.so";
    const auto assets = (root / "assets").u8string();
    require(client::check_data(game.u8string(), assets, sre.u8string()).size() == 3);
    std::array<unsigned char,64> h{};
    h[0]=0x7f; h[1]='E'; h[2]='L'; h[3]='F'; h[4]=2; h[5]=1; h[6]=1; h[16]=3; h[18]=183;
    auto write = [&](const fs::path& p) {
        std::ofstream f(p, std::ios::binary); f.write(reinterpret_cast<char*>(h.data()), h.size());
    };
    write(game); write(sre);
    std::ofstream(root / "assets" / "resources" / "gamedata.gdata").put('x');
    require(client::check_data(game.u8string(), assets, sre.u8string()).empty());
    h[18]=62; write(game); // Reject x86-64 host ELF despite matching ELF64 class.
    require(!client::arm64_elf(game));
    h[18]=183; h[4]=1; write(game); require(!client::arm64_elf(game));
    std::ofstream(game, std::ios::binary).put('x'); require(!client::arm64_elf(game));
    for (const auto& p : {game, sre, root / "assets" / "resources" / "gamedata.gdata"}) fs::remove(p);
    fs::remove(root / "assets" / "resources"); fs::remove(root / "assets"); fs::remove(root);
    std::cout << "preflight: missing, valid, wrong-machine, ELF32, truncated, Unicode passed\n";
}
