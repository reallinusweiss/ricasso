// pod_ipc.cpp — shared-memory frame ring + control stream (see pod_ipc.h)
#include "platform/pod_ipc.h"

#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace pod {

void shm_unlink_name(const std::string& name) {
#if !defined(_WIN32)
    shm_unlink(shm_sanitize(name).c_str());
#else
    (void)name;
#endif
}

std::string shm_sanitize(const std::string& raw) {
    std::string out = "swfpod_";
    for (char c : raw) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.';
        out.push_back(ok ? c : '_');
    }
    if (out.size() > 90) out.resize(90);
    return out;
}

// ---------------------------------------------------------------------------
// Shared memory layout
//   [0 .. 127]   ShmHeader (fixed; zero padded)
//   [128 .. )    3 × frame slots, each bytes = stride*height, 64-byte aligned
// ---------------------------------------------------------------------------
namespace {
constexpr size_t kSlotBase = 128;
constexpr size_t kMaxFrame = (size_t)4096 * 4096 * 4; // safety cap

struct ShmHeader {
    uint64_t magic;      // 0
    uint32_t version;    // 8
    uint32_t width;      // 12
    uint32_t height;     // 16
    uint32_t stride;     // 20
    uint32_t bytes;      // 24
    uint32_t state;      // 28
    uint32_t fps;        // 32
    uint64_t seq_total;  // 40
    uint64_t slot_gen[3];// 48  per-slot write counter (monotonic per slot)
    uint64_t slot_ack[3];// 72  per-slot consumer ack (== gen when consumed)
    uint64_t slot_pub[3];// 96  GLOBAL publish seq written into the slot on each
                         //     publish. Consumer picks max slot_pub => true newest;
                         //     producer drops min slot_pub => true oldest.
                         //     (v1 picked "newest" via gen-ack lag, which broke
                         //     ordering when several slots had equal lag.)
    // 120 .. 127 spare
};

inline uint32_t load32(const uint32_t* p) {
#if defined(_MSC_VER)
    return (uint32_t)InterlockedCompareExchange((volatile long*)p, 0, 0);
#else
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}
inline void store32(uint32_t* p, uint32_t v) {
#if defined(_MSC_VER)
    InterlockedExchange((volatile long*)p, (long)v);
#else
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}
inline uint64_t load64(const uint64_t* p) {
#if defined(_MSC_VER)
    return (uint64_t)InterlockedCompareExchange64((volatile long long*)p, 0, 0);
#else
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}
inline void store64(uint64_t* p, uint64_t v) {
#if defined(_MSC_VER)
    InterlockedExchange64((volatile long long*)p, (long long)v);
#else
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}
} // namespace

struct FrameRing::Impl { ShmHeader* hdr = nullptr; };

FrameRing::~FrameRing() { unmap(); }

void FrameRing::unmap() {
    if (m_map) {
#if defined(_WIN32)
        UnmapViewOfFile(m_map);
#else
        munmap(m_map, m_size);
#endif
        m_map = nullptr;
    }
    delete m;
    m = nullptr;
#if !defined(_WIN32)
    if (m_creator) shm_unlink(m_name.c_str());
#endif
}

FrameRing* FrameRing::create(const std::string& name, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0 || (size_t)w * h * 4 > kMaxFrame) return nullptr;
    FrameRing* r = new FrameRing();
    if (!r->map_common(shm_sanitize(name), w, h, true)) { delete r; return nullptr; }
    return r;
}

FrameRing* FrameRing::open(const std::string& name) {
    FrameRing* r = new FrameRing();
    if (!r->map_common(shm_sanitize(name), 0, 0, false)) { delete r; return nullptr; }
    return r;
}

bool FrameRing::map_common(const std::string& name, uint32_t w, uint32_t h, bool create) {
    m_name = name;
    m_creator = create;
    m = new Impl();

#if defined(_WIN32)
    (void)create; (void)w; (void)h; // phase-1: Windows consumer only (pod runs on Linux)
    return false;
#else
    int oflags = O_RDWR | (create ? O_CREAT : 0);
    int fd = shm_open(name.c_str(), oflags, 0644);
    if (fd < 0) { delete m; m = nullptr; return false; }

    size_t total;
    if (create) {
        total = kSlotBase + 3 * (size_t)w * h * 4 + 4096;
        if (ftruncate(fd, (off_t)total) != 0) { close(fd); delete m; m = nullptr; return false; }
    } else {
        struct stat st;
        if (fstat(fd, &st) != 0) { close(fd); delete m; m = nullptr; return false; }
        total = (size_t)st.st_size;
    }
    void* base = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED) { delete m; m = nullptr; return false; }
    m_map = base;
    m_size = total;
#endif

    m->hdr = (ShmHeader*)m_map;

    if (create) {
        std::memset(m_map, 0, m_size);
        ShmHeader* hh = m->hdr;
        hh->magic    = kShmMagic;
        hh->version  = kShmVersion;
        hh->width    = w;
        hh->height   = h;
        hh->stride   = w * 4;
        hh->bytes    = hh->stride * h;
        hh->state    = kStateBooting;
        hh->fps      = 0;
        hh->seq_total = 0;
    } else {
        ShmHeader* hh = m->hdr;
        if (hh->magic != kShmMagic || hh->version != kShmVersion ||
            hh->width == 0 || hh->height == 0) {
            unmap();
            return false;
        }
    }
    return true;
}

uint8_t* FrameRing::slot_base(int slot) const {
    const ShmHeader* h = m->hdr;
    return (uint8_t*)h + kSlotBase + (size_t)slot * h->bytes;
}

bool FrameRing::publish(const uint8_t* rgba, uint32_t w, uint32_t h) {
    if (!m || !m->hdr || !rgba) return false;
    ShmHeader* hh = m->hdr;
    if (hh->width != w || hh->height != h || hh->bytes == 0) return false;

    // Reuse a fully-consumed slot; otherwise recycle the OLDEST published one
    // (true drop-oldest ring, ordered by slot_pub).
    int best = -1;
    uint64_t best_pub = (uint64_t)-1;
    for (int i = 0; i < (int)kFrameSlots; ++i) {
        uint64_t g = load64(&hh->slot_gen[i]);
        uint64_t a = load64(&hh->slot_ack[i]);
        if (g == a) { best = i; break; }           // fully consumed — free
        uint64_t pub = load64(&hh->slot_pub[i]);
        if (pub < best_pub) { best_pub = pub; best = i; }
    }
    if (best < 0) return false;                    // (unreachable with 3 slots)

    std::memcpy(slot_base(best), rgba, hh->bytes);
    // Release stores: the pixels must be visible before the markers that tell
    // the consumer this slot is freshly published.
    uint64_t total = load64(&hh->seq_total) + 1;
    store64(&hh->slot_pub[best], total);
    store64(&hh->slot_gen[best], load64(&hh->slot_gen[best]) + 1);
    store64(&hh->seq_total, total);
    return true;
}

uint64_t FrameRing::grab(uint8_t* dst) {
    if (!m || !m->hdr || !dst) return 0;
    ShmHeader* hh = m->hdr;
    if (hh->bytes == 0) return 0;

    uint64_t total = load64(&hh->seq_total);
    if (total == m_last_seq) return 0; // nothing new since last grab

    // The newest UNREAD frame is the unread slot (gen > ack) with the highest
    // slot_pub — an unambiguous global ordering (v1's gen-ack "lag" was
    // per-slot and picked an older frame whenever two slots had equal lag).
    // Skipping acked slots also guarantees we never start copying a slot the
    // producer could be recycling concurrently (torn frames).
    int best = -1;
    uint64_t best_pub = 0;
    for (int i = 0; i < (int)kFrameSlots; ++i) {
        uint64_t g = load64(&hh->slot_gen[i]);
        uint64_t a = load64(&hh->slot_ack[i]);
        if (g <= a) continue;             // consumed — nothing new in it
        uint64_t pub = load64(&hh->slot_pub[i]);
        if (pub > best_pub) { best_pub = pub; best = i; }
    }
    if (best < 0) return 0;               // newest write not visible yet — retry next poll

    std::memcpy(dst, slot_base(best), hh->bytes);
    store64(&hh->slot_ack[best], load64(&hh->slot_gen[best]));
    m_last_seq = total;
    return total;
}

void FrameRing::set_state(uint32_t s)   { if (m && m->hdr) store32(&m->hdr->state, s); }
uint32_t FrameRing::state() const       { return (m && m->hdr) ? load32(&m->hdr->state) : kStateCrashed; }
void FrameRing::set_fps(uint32_t f)     { if (m && m->hdr) store32(&m->hdr->fps, f); }
uint32_t FrameRing::fps() const         { return (m && m->hdr) ? load32(&m->hdr->fps) : 0; }
FrameLayout FrameRing::layout() const {
    FrameLayout fl;
    if (m && m->hdr) {
        fl.width = m->hdr->width; fl.height = m->hdr->height;
        fl.stride = m->hdr->stride; fl.bytes = m->hdr->bytes;
    }
    return fl;
}

// ---------------------------------------------------------------------------
// Control message stream (ruby → pod stdin)
// ---------------------------------------------------------------------------
void MsgStream::push(const PodMsg& m) {
    uint32_t len = (uint32_t)sizeof(PodMsg);
    m_out->append((const char*)&len, 4);
    m_out->append((const char*)&m, len);
}

// ---------------------------------------------------------------------------
// Stream parser (pod side) — keeps a carry buffer between stdin reads
// ---------------------------------------------------------------------------
size_t pod_dispatch_stream(const uint8_t* data, size_t len,
                           std::string* carry, MsgHandler cb, void* user) {
    carry->append((const char*)data, len);
    size_t consumed = 0;
    while (carry->size() - consumed >= 4) {
        uint32_t msg_len;
        std::memcpy(&msg_len, carry->data() + consumed, 4);
        if (msg_len == 0 || msg_len > sizeof(PodMsg)) {
            carry->clear();      // desync — drop everything
            return len;
        }
        if (carry->size() - consumed < (size_t)4 + msg_len) break;
        PodMsg msg;
        std::memcpy(&msg, carry->data() + consumed + 4, msg_len);
        if (cb) cb(msg, user);
        consumed += 4 + msg_len;
    }
    if (consumed > 0) carry->erase(0, consumed);
    return consumed;
}

} // namespace pod
