#pragma once
// Звук: OGG декодируется при загрузке (stb_vorbis), голоса смешиваются на CPU и подаются в sokol_audio.
#include <string>

namespace audio {

bool init(const std::string& sfxDir);
void shutdown();
// Проиграть звук по имени файла без расширения. pitch 1 = исходная высота.
void play(const std::string& name, float volume = 1.f, float pitch = 1.f);
// Вызывать каждый кадр.
void update();
void setMuted(bool m);
bool muted();

}  // namespace audio
