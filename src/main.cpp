#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GameObject.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
struct GPTEvent {
    double timeSeconds = 0.0;
    int button = 1;       // 1 = primary, 2 = secondary
    bool pressed = true;
    float x = 0.f;
    float y = 0.f;
};
struct TrailPoint { float x = 0.f; float y = 0.f; double t = 0.0; };

std::vector<GPTEvent> g_events;
std::vector<TrailPoint> g_trail;
bool g_recording = false;
bool g_enabled = true;
bool g_playing = false;
bool g_trajectory = true;
bool g_pathfinder = false;
double g_recordTime = 0.0;
double g_playTime = 0.0;
size_t g_playIndex = 0;
float g_sampleTimer = 0.f;
constexpr double kPlaybackSpeed = 15.0;

std::string safeName(std::string name) {
    for (auto& c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '_';
    }
    if (name.empty()) name = "showcase";
    return name;
}
std::filesystem::path macroDir() {
    auto path = Mod::get()->getSaveDir() / "macros";
    std::filesystem::create_directories(path);
    return path;
}
std::string currentLevelName() {
    auto pl = PlayLayer::get();
    if (pl && pl->m_level && !pl->m_level->m_levelName.empty()) return pl->m_level->m_levelName;
    return "showcase";
}
bool saveMacro(std::string name) {
    std::ofstream out(macroDir() / (safeName(std::move(name)) + ".gpt"), std::ios::binary);
    if (!out) return false;
    out << "GPT 2\n# time_seconds button pressed(0|1) x y\n";
    out.precision(9);
    for (auto const& e : g_events)
        out << e.timeSeconds << ' ' << e.button << ' ' << (e.pressed ? 1 : 0) << ' ' << e.x << ' ' << e.y << '\n';
    out << "# trajectory x y time_seconds\n";
    for (auto const& p : g_trail) out << "#P " << p.x << ' ' << p.y << ' ' << p.t << '\n';
    return static_cast<bool>(out);
}
bool loadMacro(std::filesystem::path const& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string header; std::getline(in, header);
    if (header != "GPT 1" && header != "GPT 2") return false;
    std::vector<GPTEvent> parsed;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line); GPTEvent e; int pressed = 0;
        if (!(row >> e.timeSeconds >> e.button >> pressed)) return false;
        if (e.timeSeconds < 0 || e.button < 0 || e.button > 16 || (pressed != 0 && pressed != 1)) return false;
        row >> e.x >> e.y;
        e.pressed = pressed == 1; parsed.push_back(e);
    }
    g_events = std::move(parsed); g_playIndex = 0; g_playTime = 0; return !g_events.empty();
}
void stopRecordingAndSave(bool autoSave) {
    if (!g_recording) return;
    g_recording = false;
    if (autoSave && !g_events.empty()) {
        if (saveMacro(currentLevelName())) {
            Notification::create("Macro saved automatically", NotificationIcon::Success, 2.5f)->show();
        } else {
            Notification::create("Macro auto-save failed", NotificationIcon::Error, 3.0f)->show();
        }
    }
}
void startRecording() {
    g_events.clear(); g_trail.clear(); g_recordTime = 0.0; g_sampleTimer = 0.f;
    g_playing = false; g_recording = true;
}
void addEvent(PlayerObject* player, int button, bool pressed) {
    if (!g_recording || !g_enabled || !player) return;
    auto pl = PlayLayer::get();
    if (!pl || (player != pl->m_player1 && player != pl->m_player2)) return;
    g_events.push_back({g_recordTime, button, pressed, player->getPositionX(), player->getPositionY()});
}

class GPTMacroPanel : public CCLayerColor {
    CCLabelBMFont* m_status = nullptr;
    CCLayerColor* m_panel = nullptr;

    void setStatus(std::string const& text) {
        if (m_status) m_status->setString(text.c_str());
    }
    void addButton(CCMenu* menu, char const* title, CCPoint pos, SEL_MenuHandler callback, char const* sprite = "GJ_button_01.png") {
        auto spr = ButtonSprite::create(title, "bigFont.fnt", sprite, 0.65f);
        if (!spr) return;
        spr->setScale(0.72f);
        auto item = CCMenuItemSpriteExtra::create(spr, this, callback);
        if (!item) return;
        item->setPosition(pos);
        menu->addChild(item);
    }
    bool init() {
        auto win = CCDirector::get()->getWinSize();
        if (!CCLayerColor::initWithColor({0, 0, 0, 165})) return false;
        this->setContentSize(win);
        this->setTouchEnabled(true);

        m_panel = CCLayerColor::create({25, 30, 48, 248});
        if (!m_panel) return false;
        m_panel->setContentSize({350.f, 260.f});
        m_panel->setPosition({(win.width - 350.f) / 2.f, (win.height - 260.f) / 2.f});
        this->addChild(m_panel, 1);

        auto title = CCLabelBMFont::create("GPT SHOWCASE", "bigFont.fnt");
        if (title) { title->setScale(0.7f); title->setPosition({175.f, 232.f}); m_panel->addChild(title); }
        auto subtitle = CCLabelBMFont::create("Macro tools  |  15x event playback", "chatFont.fnt");
        if (subtitle) { subtitle->setScale(0.55f); subtitle->setPosition({175.f, 211.f}); m_panel->addChild(subtitle); }

        auto menu = CCMenu::create();
        if (!menu) return false;
        menu->setPosition({0.f, 0.f});
        addButton(menu, g_recording ? "Stop + Save" : "Record", {92.f, 172.f}, menu_selector(GPTMacroPanel::onRecord), g_recording ? "GJ_button_06.png" : "GJ_button_01.png");
        addButton(menu, "Play 15x", {258.f, 172.f}, menu_selector(GPTMacroPanel::onPlay), "GJ_button_02.png");
        addButton(menu, "Stop Play", {92.f, 126.f}, menu_selector(GPTMacroPanel::onStop), "GJ_button_06.png");
        addButton(menu, g_trajectory ? "Trail ON" : "Trail OFF", {258.f, 126.f}, menu_selector(GPTMacroPanel::onTrail), "GJ_button_04.png");
        addButton(menu, "Map Objects", {92.f, 80.f}, menu_selector(GPTMacroPanel::onMap), "GJ_button_05.png");
        addButton(menu, "Save Macro", {258.f, 80.f}, menu_selector(GPTMacroPanel::onSave), "GJ_button_01.png");
        addButton(menu, "Close", {175.f, 38.f}, menu_selector(GPTMacroPanel::onClose), "GJ_button_06.png");
        m_panel->addChild(menu);

        m_status = CCLabelBMFont::create("Ready", "chatFont.fnt");
        if (m_status) { m_status->setScale(0.55f); m_status->setPosition({175.f, 15.f}); m_panel->addChild(m_status); }
        return true;
    }
    void onRecord(CCObject*) {
        if (g_recording) { stopRecordingAndSave(true); setStatus("Stopped; auto-save attempted"); }
        else { startRecording(); setStatus("Recording inputs and trail"); }
    }
    void onPlay(CCObject*) {
        if (g_events.empty()) { setStatus("Record a macro first"); return; }
        g_recording = false; g_playing = true; g_playIndex = 0; g_playTime = 0.0;
        setStatus("Playing events at 15x timing");
    }
    void onStop(CCObject*) { g_playing = false; setStatus("Playback stopped"); }
    void onTrail(CCObject*) { g_trajectory = !g_trajectory; setStatus(g_trajectory ? "Trail sampling enabled" : "Trail sampling disabled"); }
    void onMap(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl) { setStatus("Enter a level to map objects"); return; }
        std::ofstream out(macroDir() / (safeName(currentLevelName()) + "-object-map.csv"));
        if (!out) { setStatus("Could not write object map"); return; }
        out << "index,x,y,width,height,rotation\n";
        size_t count = 0;
        for (auto obj : CCArrayExt<GameObject*>(pl->m_objects)) {
            if (!obj) continue;
            auto pos = obj->getPosition();
            auto size = obj->getContentSize();
            out << count++ << ',' << pos.x << ',' << pos.y << ',' << size.width << ',' << size.height << ',' << obj->getRotation() << '\n';
        }
        setStatus("Exported object map (approx. geometry)");
    }
    void onSave(CCObject*) { setStatus(saveMacro(currentLevelName()) ? "Saved to Geode save/macros" : "Save failed"); }
    void onClose(CCObject*) { this->removeFromParentAndCleanup(true); }
public:
    static GPTMacroPanel* create() {
        auto ret = new GPTMacroPanel();
        if (ret && ret->init()) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
};}

class $modify(GPTPlayerObject, PlayerObject) {
    bool pushButton(PlayerButton button) {
        auto result = PlayerObject::pushButton(button);
        addEvent(this, static_cast<int>(button), true);
        return result;
    }
    bool releaseButton(PlayerButton button) {
        auto result = PlayerObject::releaseButton(button);
        addEvent(this, static_cast<int>(button), false);
        return result;
    }
};

class $modify(GPTPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_recording = false; g_playing = false; g_recordTime = 0; g_sampleTimer = 0;
        return true;
    }
    void update(float dt) {
        PlayLayer::update(dt);
        if (g_recording && g_enabled) {
            g_recordTime += dt;
            g_sampleTimer += dt;
            if (g_trajectory && g_sampleTimer >= 0.025f) {
                g_sampleTimer = 0.f;
                if (m_player1) g_trail.push_back({m_player1->getPositionX(), m_player1->getPositionY(), g_recordTime});
            }
        }
        if (g_playing && g_enabled && m_player1) {
            g_playTime += static_cast<double>(dt) * kPlaybackSpeed;
            while (g_playIndex < g_events.size() && g_events[g_playIndex].timeSeconds <= g_playTime) {
                auto const& e = g_events[g_playIndex++];
                auto btn = static_cast<PlayerButton>(e.button);
                if (e.pressed) m_player1->pushButton(btn); else m_player1->releaseButton(btn);
            }
            if (g_playIndex >= g_events.size()) g_playing = false;
        }
    }
    void levelComplete() {
        // Stop first so no further inputs are appended, then auto-save the completed run.
        g_playing = false;
        stopRecordingAndSave(true);
        PlayLayer::levelComplete();
    }
    void onQuit() {
        g_playing = false;
        stopRecordingAndSave(true);
        PlayLayer::onQuit();
    }
};

class $modify(GPTPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto win = CCDirector::get()->getWinSize();
        auto item = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png"), this,
            menu_selector(GPTPauseLayer::onOpenGPT));
        if (!item) return;
        item->setID("gpt-showcase-open-button"_spr);
        item->setScale(0.55f); item->setPosition({win.width - 28.f, win.height * 0.50f});
        if (auto menu = this->getChildByID("right-button-menu")) { menu->addChild(item); menu->updateLayout(); }
        else this->addChild(item);
    }
    void onOpenGPT(CCObject*) { if (auto panel = GPTMacroPanel::create()) this->addChild(panel, 9999); }
};
