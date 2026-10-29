#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Popup.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

// This prototype defines a small, documented .gpt event format and pause-menu
// entry point. Input capture/playback and TPS timing hooks are intentionally
// isolated for the next implementation pass; see README.md.

struct GPTEvent {
    double timeSeconds = 0.0;
    int button = 1;      // 1 = primary, 2 = secondary
    bool pressed = true;
};

class GPTMacroStore {
public:
    static std::filesystem::path directory() {
        auto path = Mod::get()->getSaveDir() / "macros";
        std::filesystem::create_directories(path);
        return path;
    }

    static bool save(std::string const& name, std::vector<GPTEvent> const& events) {
        auto safeName = name;
        for (auto& c : safeName) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '_';
        }
        if (safeName.empty()) safeName = "showcase";
        std::ofstream out(directory() / (safeName + ".gpt"), std::ios::binary);
        if (!out) return false;

        // Plain-text GPT v1: header followed by "time button pressed" rows.
        out << "GPT 1\n";
        out << "# time_seconds button pressed(0|1)\n";
        out.precision(9);
        for (auto const& e : events) {
            out << e.timeSeconds << ' ' << e.button << ' ' << (e.pressed ? 1 : 0) << '\n';
        }
        return static_cast<bool>(out);
    }

    static bool load(std::filesystem::path const& file, std::vector<GPTEvent>& events) {
        std::ifstream in(file, std::ios::binary);
        if (!in) return false;
        std::string header;
        std::getline(in, header);
        if (header != "GPT 1") return false;

        std::vector<GPTEvent> parsed;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream row(line);
            GPTEvent e;
            int pressed = 0;
            if (!(row >> e.timeSeconds >> e.button >> pressed)) return false;
            if (e.timeSeconds < 0 || (e.button != 1 && e.button != 2) ||
                (pressed != 0 && pressed != 1)) return false;
            e.pressed = pressed == 1;
            parsed.push_back(e);
        }
        events = std::move(parsed);
        return true;
    }
};

class GPTMacroPopup : public Popup<> {
protected:
    CCLabelBMFont* m_status = nullptr;

    bool setup() override {
        this->setTitle("GPT Showcase Macro");
        auto winSize = CCDirector::get()->getWinSize();

        auto addButton = [&](char const* label, CCPoint pos, SEL_MenuHandler callback) {
            auto button = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(label, 100, true, "goldFont.fnt", "GJ_button_01.png", 30, 0.7f),
                this, callback
            );
            button->setPosition(pos);
            return button;
        };

        auto menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        menu->addChild(addButton("Record", {winSize.width * 0.30f, winSize.height * 0.56f},
                                 menu_selector(GPTMacroPopup::onRecord)));
        menu->addChild(addButton("Stop", {winSize.width * 0.50f, winSize.height * 0.56f},
                                 menu_selector(GPTMacroPopup::onStop)));
        menu->addChild(addButton("Continue", {winSize.width * 0.70f, winSize.height * 0.56f},
                                 menu_selector(GPTMacroPopup::onContinue)));
        menu->addChild(addButton("Save .gpt", {winSize.width * 0.38f, winSize.height * 0.40f},
                                 menu_selector(GPTMacroPopup::onSave)));
        menu->addChild(addButton("Enable / Disable", {winSize.width * 0.63f, winSize.height * 0.40f},
                                 menu_selector(GPTMacroPopup::onToggle)));
        this->addChild(menu);

        m_status = CCLabelBMFont::create(
            "Prototype UI: recorder hooks not connected yet", "bigFont.fnt");
        m_status->setScale(0.38f);
        m_status->setPosition({winSize.width / 2.f, winSize.height * 0.28f});
        this->addChild(m_status);
        return true;
    }

    void onRecord(CCObject*) { setStatus("Record requested (input hook TODO)"); }
    void onStop(CCObject*) { setStatus("Stop requested; macro data is preserved"); }
    void onContinue(CCObject*) { setStatus("Continue requested (append hook TODO)"); }
    void onSave(CCObject*) {
        std::vector<GPTEvent> emptyPrototype;
        bool ok = GPTMacroStore::save("showcase", emptyPrototype);
        setStatus(ok ? "Saved macros/showcase.gpt (empty prototype)" : "Save failed");
    }
    void onToggle(CCObject*) { setStatus("Enable/disable UI toggle requested (state hook TODO)"); }

    void setStatus(char const* text) {
        if (m_status) m_status->setString(text);
    }

public:
    static GPTMacroPopup* create() {
        auto ret = new GPTMacroPopup();
        if (ret->initAnchored(360.f, 220.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

$modify(GPTPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::get()->getWinSize();
        auto button = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png"),
            this,
            menu_selector(GPTPauseLayer::onOpenGPT)
        );
        if (!button) return;

        button->setID("gpt-showcase-open-button"_spr);
        button->setScale(0.65f);
        button->setPosition({winSize.width - 28.f, winSize.height * 0.52f});

        if (auto menu = this->getChildByID("right-button-menu")) {
            menu->addChild(button);
            menu->updateLayout();
        } else {
            // Fallback for layouts where the standard menu ID differs.
            this->addChild(button);
        }
    }

    void onOpenGPT(CCObject*) {
        if (auto popup = GPTMacroPopup::create()) popup->show();
    }
};

