// Точка входа Lanternfall (sokol_app). Флаги для автотестов (Linux):
//   --screenshot out.png --frames N   отрисовать N кадров, сохранить кадр и выйти
//   --scene village|mission|raid|night   стартовая сцена;  --modal shop|army|map|night|codex|menu
//   --deploy                             высадить всю армию (бой);  --dialogue  оставить вступительный диалог
#include <cstdlib>
#include <cstring>
#include <string>

#include "client/app.h"
#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "engine/input.h"
#include "platform/platform.h"
#include "sokol_app.h"
#include "sokol_log.h"
#include "sokol_time.h"

namespace {
client::App app;
uint64_t lastTick = 0;
int frameNo = 0;
struct Opts {
    const char* shot = nullptr;
    int frames = 60;
    std::string scene = "village", modal, select;
    bool deploy = false, keepDialogue = false;
    int w = 1280, h = 720;
} opt;

void init() {
    stm_setup();
    gfx::init();
    std::string assets = platform::assetDir();
    atlas::load(assets + "/atlas");
    font::load(assets + "/fonts/Philosopher-Bold.ttf", 56);
    if (!opt.shot) audio::init(assets + "/sfx");
    app.init();
    if (!opt.keepDialogue && opt.shot) app.skipDialogue();
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
    else if (opt.modal == "menu") app.modal = M::Menu;
    else if (opt.modal == "info") app.modal = M::Info;
    else if (opt.modal == "tasks") app.modal = M::Tasks;
    else if (opt.modal == "workshop") app.modal = M::Workshop;
}

void frame() {
    double dt = lastTick ? stm_sec(stm_laptime(&lastTick)) : 1.0 / 60;
    if (!lastTick) stm_laptime(&lastTick);
    if (opt.shot) dt = 1.0 / 30;  // детерминированное время в тестах
    if (dt > 0.1) dt = 0.1;
    if (opt.deploy && frameNo == 2 && app.scene == client::Scene::Battle) app.autoDeploy();
    app.frame((float)dt, sapp_widthf() / sapp_dpi_scale(), sapp_heightf() / sapp_dpi_scale());
    frameNo++;
    if (opt.shot && frameNo >= opt.frames) {
        gfx::screenshot(opt.shot);
        sapp_request_quit();
    }
}

void event(const sapp_event* e) {
    input::handle(e);
    if (e->type == SAPP_EVENTTYPE_SUSPENDED || e->type == SAPP_EVENTTYPE_QUIT_REQUESTED) app.save();
}

void cleanup() {
    app.save();
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
        else if (a == "--size") { std::string s = next(); opt.w = std::atoi(s.c_str()); opt.h = std::atoi(s.substr(s.find('x') + 1).c_str()); }
    }
    sapp_desc d = {};
    d.init_cb = init;
    d.frame_cb = frame;
    d.event_cb = event;
    d.cleanup_cb = cleanup;
    d.width = opt.w;
    d.height = opt.h;
    d.window_title = "Lanternfall";
    d.high_dpi = true;
    d.sample_count = 1;
    d.ios.keyboard_resizes_canvas = false;
    d.logger.func = slog_func;
    return d;
}
