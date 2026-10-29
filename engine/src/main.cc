// Точка входа (sokol_app). По умолчанию — «Острог: 99 ночей тайги» (client/surv_game). Флаги для автотестов (Linux):
//   --screenshot out.png --frames N   отрисовать N кадров, сохранить кадр и выйти
//   --bot                              персонажем управляет помощник;  --ff N  перемотать N секунд;  --yaw A  поворот камеры
//   --build                            режим стройки;  --touch  экранные кнопки и джойстик
//   --scene village|mission|raid|night   старая стратегия;  --modal shop|army|map|night|codex|menu
//   --deploy                             высадить всю армию (бой);  --dialogue  оставить вступительный диалог
#include <cstdlib>
#include <cstring>
#include <string>

#include "client/app.h"
#include "client/surv_game.h"
#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "engine/input.h"
#include "engine/r3d.h"
#include "platform/platform.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_time.h"

namespace {
client::App app;
sclient::Game survival;
bool survMode = true;
uint64_t lastTick = 0;
int frameNo = 0;
struct Opts {
    const char* shot = nullptr;
    int frames = 60;
    std::string scene = "survival", modal, select;
    bool deploy = false, keepDialogue = false, bot = false, build = false, touch = false;
    float ff = 0, yaw = -100;
    std::string spawn;
    int w = 1280, h = 720;
    float view = 0;
} opt;

void init() {
    stm_setup();
    gfx::init();
    r3d::init();
    std::string assets = platform::assetDir();
    atlas::load(assets + "/atlas");
    font::load(assets + "/fonts/Rubik-Bold.ttf", 56);
    if (!opt.shot) audio::init(assets + "/sfx");
    survMode = opt.scene == "survival";
    if (survMode) {
        survival.init();
#if defined(__APPLE__)
        survival.touch = true;
#endif
        survival.touch |= opt.touch;
        survival.bot = opt.bot;
        if (opt.ff > 0) survival.fastForward(opt.ff);
        if (opt.yaw > -99) survival.setCameraYaw(opt.yaw);
        survival.setBuildMode(opt.build);
        if (opt.spawn == "likho") survival.debugSpawn(surv::EnemyKind::Likho, 9.f);
        if (opt.spawn == "wolf") survival.debugSpawn(surv::EnemyKind::Wolf, 6.f);
        return;
    }
    app.init();
    if (!opt.keepDialogue && opt.shot) app.skipDialogue();
    if (opt.shot && opt.scene != "title") app.title = false;
    if (opt.view > 0) app.viewRadius(opt.view);
    if (opt.scene == "mission") app.startMission(0);
    else if (opt.scene == "raid") app.startRaid();
    else if (opt.scene == "night") app.startNight();
    if (!opt.select.empty()) app.selectType(opt.select);
    using M = client::Modal;
    if (opt.modal == "shop") app.modal = M::Shop;
    else if (opt.modal == "army") app.modal = M::Army;
    else if (opt.modal == "map") app.modal = M::Map;
    else if (opt.modal == "night") app.modal = M::Night;
    else if (opt.modal == "codex") app.modal = M::Codex;
    else if (opt.modal == "profile") app.modal = M::Profile;
    else if (opt.modal == "menu") app.modal = M::Menu;
    else if (opt.modal == "info") app.modal = M::Info;
    else if (opt.modal == "tasks") app.modal = M::Tasks;
    else if (opt.modal == "workshop") app.modal = M::Workshop;
    else if (opt.modal == "chat") app.modal = M::Chat;
    else if (opt.modal == "clan") app.modal = M::Clan;
}

void frame() {
    double dt = lastTick ? stm_sec(stm_laptime(&lastTick)) : 1.0 / 60;
    if (!lastTick) stm_laptime(&lastTick);
    if (opt.shot) dt = 1.0 / 30;  // детерминированное время в тестах
    if (dt > 0.1) dt = 0.1;
    static bool deployed = false;
    if (opt.deploy && !deployed && frameNo >= 2 && app.scene == client::Scene::Battle) { app.autoDeploy(); deployed = true; }
    if (survMode) survival.frame((float)dt, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale());
    else app.frame((float)dt, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale());
    frameNo++;
    if (opt.shot && frameNo >= opt.frames) {
        gfx::screenshot(opt.shot);
        sapp_request_quit();
    }
}

void event(const sapp_event* e) {
    input::handle(e);
    if (!survMode && (e->type == SAPP_EVENTTYPE_SUSPENDED || e->type == SAPP_EVENTTYPE_QUIT_REQUESTED)) app.save();
}

void cleanup() {
    if (!survMode) app.save();
    audio::shutdown();
    gfx::shutdown();
}
}  // namespace

sapp_desc sokol_main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--screenshot") opt.shot = next();
        else if (a == "--frames") opt.frames = std::atoi(next());
        else if (a == "--scene") opt.scene = next();
        else if (a == "--modal") opt.modal = next();
        else if (a == "--deploy") opt.deploy = true;
        else if (a == "--select") opt.select = next();
        else if (a == "--dialogue") opt.keepDialogue = true;
        else if (a == "--view") opt.view = (float)std::atof(next());
        else if (a == "--bot") opt.bot = true;
        else if (a == "--spawn") opt.spawn = next();
        else if (a == "--build") opt.build = true;
        else if (a == "--touch") opt.touch = true;
        else if (a == "--ff") opt.ff = (float)std::atof(next());
        else if (a == "--yaw") opt.yaw = (float)std::atof(next());
        else if (a == "--size") { std::string s = next(); opt.w = std::atoi(s.c_str()); opt.h = std::atoi(s.substr(s.find('x') + 1).c_str()); }
    }
    sapp_desc d = {};
    d.init_cb = init;
    d.frame_cb = frame;
    d.event_cb = event;
    d.cleanup_cb = cleanup;
    d.width = opt.w;
    d.height = opt.h;
    d.window_title = "Острог: 99 ночей тайги";
    d.high_dpi = true;
    d.sample_count = 1;
    d.ios.keyboard_resizes_canvas = false;
    d.logger.func = slog_func;
    return d;
}
