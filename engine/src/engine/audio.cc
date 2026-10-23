#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "sokol_audio.h"
#include "sokol_log.h"
#include "../platform/platform.h"

extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels, int* sample_rate, short** output);

namespace audio {
namespace {

struct Clip { std::vector<float> samples; int rate = 44100; };
struct Voice { const Clip* clip; double pos; float step, volume; };

std::unordered_map<std::string, Clip> clips;
std::vector<Voice> voices;
std::vector<float> mixBuf;
bool ready = false, isMuted = false;
int outRate = 44100;
std::string dir;

// Музыка: одна дорожка играет по кругу, предыдущая плавно затухает.
struct Track { const Clip* clip = nullptr; double pos = 0; float vol = 0, target = 0; };
Track cur, prev;
std::string curName;

const Clip* loadPath(const std::string& key, const std::string& path) {
    auto it = clips.find(key);
    if (it != clips.end()) return &it->second;
    std::vector<unsigned char> data;
    if (!platform::readFile(path, data)) return nullptr;
    int ch = 0, rate = 0;
    short* pcm = nullptr;
    int n = stb_vorbis_decode_memory(data.data(), (int)data.size(), &ch, &rate, &pcm);
    if (n <= 0 || !pcm) return nullptr;
    Clip c;
    c.rate = rate;
    c.samples.resize(n);
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int k = 0; k < ch; k++) s += pcm[i * ch + k] / 32768.f;
        c.samples[i] = s / ch;  // в моно
    }
    free(pcm);
    return &(clips[key] = std::move(c));
}

const Clip* load(const std::string& name) { return loadPath(name, dir + "/" + name + ".ogg"); }

void mixTrack(Track& tr, int frames) {
    if (!tr.clip || tr.clip->samples.size() < 2) return;
    const auto& s = tr.clip->samples;
    double step = (double)tr.clip->rate / outRate;
    float fade = 1.f / (1.5f * outRate);  // полторы секунды на смену громкости
    for (int i = 0; i < frames; i++) {
        tr.vol += std::clamp(tr.target - tr.vol, -fade, fade);
        size_t p = (size_t)tr.pos;
        size_t q = (p + 1) % s.size();
        float f = (float)(tr.pos - p);
        mixBuf[i] += (s[p] * (1 - f) + s[q] * f) * tr.vol;
        tr.pos += step;
        if (tr.pos >= s.size()) tr.pos -= s.size();
    }
}
}  // namespace

void music(const std::string& name, float volume) {
    if (!ready) return;
    if (name == curName && cur.clip) { cur.target = volume; return; }
    prev = cur;
    prev.target = 0;
    cur = Track{loadPath("music/" + name, dir + "/../music/" + name + ".ogg"), 0, 0, volume};
    curName = name;
}

bool init(const std::string& sfxDir) {
    dir = sfxDir;
    saudio_desc d = {};
    d.num_channels = 1;
    d.sample_rate = 44100;
    d.buffer_frames = 2048;
    d.logger.func = slog_func;
    saudio_setup(&d);
    ready = saudio_isvalid();
    if (ready) outRate = saudio_sample_rate();
    return ready;
}

void shutdown() {
    if (ready) saudio_shutdown();
    ready = false;
}

void play(const std::string& name, float volume, float pitch) {
    if (!ready || isMuted) return;
    // не даём одному звуку забить микс: не чаще раза в 60 мс
    static std::unordered_map<std::string, double> last;
    static double clock = 0;
    clock = (double)platform::nowMs() / 1000.0;
    auto it = last.find(name);
    if (it != last.end() && clock - it->second < 0.06) return;
    last[name] = clock;
    const Clip* c = load(name);
    if (!c) return;
    if (voices.size() >= 24) voices.erase(voices.begin());
    voices.push_back({c, 0.0, pitch * (float)c->rate / outRate, volume});
}

void update() {
    if (!ready) return;
    int frames = saudio_expect();
    if (frames <= 0) return;
    mixBuf.assign(frames, 0.f);
    if (!isMuted) {
        mixTrack(cur, frames);
        mixTrack(prev, frames);
    }
    for (auto& v : voices) {
        const auto& s = v.clip->samples;
        for (int i = 0; i < frames; i++) {
            size_t p = (size_t)v.pos;
            if (p + 1 >= s.size()) { v.pos = (double)s.size(); break; }
            float f = (float)(v.pos - p);
            mixBuf[i] += (s[p] * (1 - f) + s[p + 1] * f) * v.volume;
            v.pos += v.step;
        }
    }
    voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& v) { return v.pos + 1 >= v.clip->samples.size(); }), voices.end());
    for (float& x : mixBuf) x = std::tanh(x * 0.9f);  // мягкий лимитер
    saudio_push(mixBuf.data(), frames);
}

void setMuted(bool m) { isMuted = m; if (m) voices.clear(); }
bool muted() { return isMuted; }

}  // namespace audio
