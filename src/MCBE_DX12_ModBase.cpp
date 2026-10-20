// =============================================================================
//  MCBE_DX12_ModBase  --  single translation unit build
//  This file is the automatic merge of every header/source under src/.
//  Third-party libraries (imgui, entt, nlohmann, libhat, kiero, MinHook, d3dx12)
//  are NOT merged; they are still included from external/.
// =============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define IMGUI_DEFINE_MATH_OPERATORS

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <shared_mutex>
#include <cstring>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <type_traits>
#include <entt/entt.hpp>
#include <cstddef>
#include <string_view>
#include <format>
#include <source_location>
#include <fstream>
#include <cstdlib>
#include <cctype>
#include <MinHook.h>
#include <libhat/scanner.hpp>
#include <chrono>
#include <utility>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <kiero.h>
#include <imgui_internal.h>
#include <cstdio>
#include <map>
#include <iostream>

// ==============================================================================
// HEADER: src/client.h
// ==============================================================================
class Client {
public:
    static Client& instance();

    void initialize(HMODULE module);
    void shutdown();

    HMODULE module_handle() const { return m_module; }

private:
    Client() = default;

    HMODULE m_module = nullptr;
    bool m_configLoaded = false;
};

// ==============================================================================
// HEADER: src/config/config_manager.h
// ==============================================================================
class ConfigManager {
public:
    static ConfigManager& instance();

    std::filesystem::path get_config_directory() const;

    bool save_config(const std::string& configName = "default");
    bool load_config(const std::string& configName = "default");
    bool delete_config(const std::string& configName);

    std::vector<std::string> get_available_configs() const;

private:
    ConfigManager() = default;
    void ensure_directory_exists() const;
};

// ==============================================================================
// HEADER: src/events/event.h
// ==============================================================================
class Event {
public:
    virtual ~Event() = default;

    bool is_cancelled() const { return m_cancelled; }
    void cancel() { m_cancelled = true; }

private:
    bool m_cancelled = false;
};

// ==============================================================================
// HEADER: src/events/client_instance_update_event.h
// ==============================================================================
class ClientInstance;

struct ClientInstanceUpdateEvent : public Event {
    ClientInstance* clientInstance = nullptr;
    bool isInitFinished = false;

    ClientInstanceUpdateEvent(ClientInstance* instance, bool initFinished)
        : clientInstance(instance), isInitFinished(initFinished) {
    }
};

// ==============================================================================
// HEADER: src/events/event_bus.h
// ==============================================================================
class EventBus {
public:
    using ListenerId = std::uint64_t;

    template <typename T>
    using EventCallback = std::function<void(T&)>;

    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    template <typename T>
    ListenerId subscribe(EventCallback<T> callback) {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        const std::type_index typeIndex = typeid(T);
        const ListenerId id = m_nextId.fetch_add(1, std::memory_order_relaxed);

        auto wrapper = [callback](Event& event) {
            callback(static_cast<T&>(event));
            };

        std::lock_guard<std::mutex> lock(m_mutex);
        m_listeners[typeIndex].push_back({ id, wrapper });
        return id;
    }

    void unsubscribe(ListenerId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [type, list] : m_listeners) {
            std::erase_if(list, [id](const ListenerEntry& entry) {
                return entry.id == id;
                });
        }
    }

    template <typename T>
    void publish(T& event) {
        static_assert(std::is_base_of_v<Event, T>, "T must derive from Event");

        const std::type_index typeIndex = typeid(T);
        std::vector<ListenerEntry> listenersCopy;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_listeners.find(typeIndex);
            if (it == m_listeners.end()) {
                return;
            }
            listenersCopy = it->second;
        }

        for (const auto& entry : listenersCopy) {
            entry.callback(event);
            if (event.is_cancelled()) {
                break;
            }
        }
    }

private:
    EventBus() = default;

    struct ListenerEntry {
        ListenerId id;
        std::function<void(Event&)> callback;
    };

    std::mutex m_mutex;
    std::atomic<ListenerId> m_nextId{ 1 };
    std::unordered_map<std::type_index, std::vector<ListenerEntry>> m_listeners;
};

// ==============================================================================
// HEADER: src/minecraft/hooks/hook_manager.h
// ==============================================================================
namespace minecraft::hooks {
    bool initialize();
    void shutdown();
}

// ==============================================================================
// HEADER: src/ui/widgets/widgets.h
// ==============================================================================
namespace ui {

    struct Palette {
        ImU32 bg_dark = IM_COL32(11, 13, 17, 252);
        ImU32 panel_bg = IM_COL32(17, 20, 26, 252);
        ImU32 card_bg = IM_COL32(23, 27, 36, 255);
        ImU32 card_hover = IM_COL32(31, 37, 50, 255);
        ImU32 card_enabled = IM_COL32(14, 42, 33, 255);
        ImU32 border = IM_COL32(42, 50, 66, 180);
        ImU32 border_focused = IM_COL32(16, 185, 129, 220);

        ImU32 text_primary = IM_COL32(245, 248, 252, 255);
        ImU32 text_secondary = IM_COL32(156, 163, 175, 255);
        ImU32 text_muted = IM_COL32(100, 116, 139, 255);

        ImU32 accent = IM_COL32(16, 185, 129, 255);        // Emerald Accent (#10B981)
        ImU32 accent_hover = IM_COL32(52, 211, 153, 255);  // Light Emerald (#34D399)
        ImU32 accent_active = IM_COL32(5, 150, 105, 255);  // Deep Emerald (#059669)
        ImU32 accent_muted = IM_COL32(16, 185, 129, 50);   // Emerald Glow

        ImU32 danger = IM_COL32(239, 68, 68, 255);
        ImU32 danger_hover = IM_COL32(248, 113, 113, 255);
    };

    const Palette& get_palette();
    void apply_theme();

    // UI Widget Controls
    bool ToggleSwitch(const char* label, bool* v);
    bool Checkbox(const char* label, bool* v);
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f");
    bool SliderInt(const char* label, int* v, int v_min, int v_max);
    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items);
    bool ColorPicker(const char* label, float col[4]);
    bool InputText(const char* label, char* buf, size_t buf_size);
    bool KeybindBadge(const char* label, int keybind, bool is_binding);
    bool SearchBar(const char* id, char* buf, size_t buf_size, const char* placeholder = "Search modules...");

    void SectionHeader(const char* title, const char* badge = nullptr);
    bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));
}

// ==============================================================================
// HEADER: src/config/setting.h
// ==============================================================================
namespace config {

    enum class SettingType {
        BOOL,
        FLOAT,
        INT,
        ENUM,
        COLOR,
        STRING
    };

    class Setting {
    public:
        Setting(std::string name, std::string description, std::function<bool()> visibility = nullptr)
            : m_name(std::move(name)), m_description(std::move(description)), m_visibility(std::move(visibility)) {
        }
        virtual ~Setting() = default;

        const std::string& name() const { return m_name; }
        const std::string& description() const { return m_description; }
        bool is_visible() const { return m_visibility ? m_visibility() : true; }
        virtual SettingType type() const = 0;

        virtual void render_ui() = 0;
        virtual nlohmann::json serialize() const = 0;
        virtual void deserialize(const nlohmann::json& j) = 0;

    protected:
        std::string m_name;
        std::string m_description;
        std::function<bool()> m_visibility;
    };

    class BoolSetting : public Setting {
    public:
        BoolSetting(std::string name, std::string description, bool defaultValue, std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)), m_value(defaultValue) {
        }

        SettingType type() const override { return SettingType::BOOL; }

        bool value() const { return m_value.load(std::memory_order_acquire); }
        void set_value(bool val) { m_value.store(val, std::memory_order_release); }
        operator bool() const { return value(); }

        void render_ui() override {
            bool val = m_value.load(std::memory_order_relaxed);
            if (ui::ToggleSwitch(m_name.c_str(), &val)) {
                m_value.store(val, std::memory_order_release);
            }
            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override { return value(); }
        void deserialize(const nlohmann::json& j) override {
            if (j.is_boolean()) set_value(j.get<bool>());
        }

    private:
        std::atomic<bool> m_value;
    };

    class FloatSetting : public Setting {
    public:
        FloatSetting(std::string name, std::string description, float defaultValue, float min, float max, const char* format = "%.2f", std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)),
            m_value(defaultValue), m_min(min), m_max(max), m_format(format) {
        }

        SettingType type() const override { return SettingType::FLOAT; }

        float value() const { return m_value.load(std::memory_order_acquire); }
        void set_value(float val) { m_value.store(std::clamp(val, m_min, m_max), std::memory_order_release); }
        operator float() const { return value(); }

        float min() const { return m_min; }
        float max() const { return m_max; }
        const char* format() const { return m_format; }

        void render_ui() override {
            float val = m_value.load(std::memory_order_relaxed);
            if (ui::SliderFloat(m_name.c_str(), &val, m_min, m_max, m_format)) {
                set_value(val);
            }
            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override { return value(); }
        void deserialize(const nlohmann::json& j) override {
            if (j.is_number()) set_value(j.get<float>());
        }

    private:
        std::atomic<float> m_value;
        float m_min;
        float m_max;
        const char* m_format;
    };

    class IntSetting : public Setting {
    public:
        IntSetting(std::string name, std::string description, int defaultValue, int min, int max, std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)),
            m_value(defaultValue), m_min(min), m_max(max) {
        }

        SettingType type() const override { return SettingType::INT; }

        int value() const { return m_value.load(std::memory_order_acquire); }
        void set_value(int val) { m_value.store(std::clamp(val, m_min, m_max), std::memory_order_release); }
        operator int() const { return value(); }

        int min() const { return m_min; }
        int max() const { return m_max; }

        void render_ui() override {
            int val = m_value.load(std::memory_order_relaxed);
            if (ui::SliderInt(m_name.c_str(), &val, m_min, m_max)) {
                set_value(val);
            }
            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override { return value(); }
        void deserialize(const nlohmann::json& j) override {
            if (j.is_number_integer()) set_value(j.get<int>());
        }

    private:
        std::atomic<int> m_value;
        int m_min;
        int m_max;
    };

    class EnumSetting : public Setting {
    public:
        EnumSetting(std::string name, std::string description, int defaultValue, std::vector<std::string> options, std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)),
            m_value(defaultValue), m_options(std::move(options)) {
        }

        SettingType type() const override { return SettingType::ENUM; }

        int value() const { return m_value.load(std::memory_order_acquire); }
        void set_value(int val) {
            if (val >= 0 && val < static_cast<int>(m_options.size())) {
                m_value.store(val, std::memory_order_release);
            }
        }
        operator int() const { return value(); }

        const std::vector<std::string>& options() const { return m_options; }

        std::string current_option() const {
            int val = value();
            if (val >= 0 && val < static_cast<int>(m_options.size())) {
                return m_options[val];
            }
            return "";
        }

        void render_ui() override {
            int val = m_value.load(std::memory_order_relaxed);
            if (ui::Combo(m_name.c_str(), &val, m_options)) {
                set_value(val);
            }
            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override { return value(); }
        void deserialize(const nlohmann::json& j) override {
            if (j.is_number_integer()) {
                set_value(j.get<int>());
            }
        }

    private:
        std::atomic<int> m_value;
        std::vector<std::string> m_options;
    };

    class ColorSetting : public Setting {
    public:
        ColorSetting(std::string name, std::string description, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f, std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)), m_color{ r, g, b, a } {
        }

        SettingType type() const override { return SettingType::COLOR; }

        void get_color(float outColor[4]) const {
            std::shared_lock lock(m_mutex);
            std::memcpy(outColor, m_color, sizeof(float) * 4);
        }

        void set_color(float r, float g, float b, float a) {
            std::unique_lock lock(m_mutex);
            m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
        }

        ImVec4 as_imvec4() const {
            std::shared_lock lock(m_mutex);
            return ImVec4(m_color[0], m_color[1], m_color[2], m_color[3]);
        }

        void render_ui() override {
            float tempColor[4];
            {
                std::shared_lock lock(m_mutex);
                std::memcpy(tempColor, m_color, sizeof(tempColor));
            }

            if (ui::ColorPicker(m_name.c_str(), tempColor)) {
                std::unique_lock lock(m_mutex);
                std::memcpy(m_color, tempColor, sizeof(m_color));
            }

            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override {
            std::shared_lock lock(m_mutex);
            return nlohmann::json::array({ m_color[0], m_color[1], m_color[2], m_color[3] });
        }

        void deserialize(const nlohmann::json& j) override {
            if (j.is_array() && j.size() == 4) {
                std::unique_lock lock(m_mutex);
                m_color[0] = j[0].get<float>();
                m_color[1] = j[1].get<float>();
                m_color[2] = j[2].get<float>();
                m_color[3] = j[3].get<float>();
            }
        }

    private:
        mutable std::shared_mutex m_mutex;
        float m_color[4];
    };

    class StringSetting : public Setting {
    public:
        StringSetting(std::string name, std::string description, std::string defaultValue, std::function<bool()> visibility = nullptr)
            : Setting(std::move(name), std::move(description), std::move(visibility)), m_value(std::move(defaultValue)) {
        }

        SettingType type() const override { return SettingType::STRING; }

        std::string value() const {
            std::shared_lock lock(m_mutex);
            return m_value;
        }

        void set_value(std::string val) {
            std::unique_lock lock(m_mutex);
            m_value = std::move(val);
        }

        void render_ui() override {
            char buffer[256] = {};
            {
                std::shared_lock lock(m_mutex);
                strncpy_s(buffer, m_value.c_str(), sizeof(buffer) - 1);
            }

            if (ui::InputText(m_name.c_str(), buffer, sizeof(buffer))) {
                std::unique_lock lock(m_mutex);
                m_value = buffer;
            }

            if (!m_description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_description.c_str());
            }
        }

        nlohmann::json serialize() const override {
            std::shared_lock lock(m_mutex);
            return m_value;
        }

        void deserialize(const nlohmann::json& j) override {
            if (j.is_string()) {
                set_value(j.get<std::string>());
            }
        }

    private:
        mutable std::shared_mutex m_mutex;
        std::string m_value;
    };

}

// ==============================================================================
// HEADER: src/modules/module.h
// ==============================================================================
class Module {
public:
    Module(std::string category, std::string name, std::string tooltip);
    virtual ~Module() = default;

    const std::string& category() const;
    const std::string& name() const;
    const std::string& tooltip() const;

    bool enabled() const;
    int keybind() const;
    bool hold_to_enable() const;
    void set_enabled(bool enabled);
    void set_keybind(int keybind);
    void set_hold_to_enable(bool holdToEnable);
    void toggle();

    virtual void on_enable();
    virtual void on_disable();
    virtual void on_imgui_render();
    virtual void on_settings_render();
    virtual bool has_settings() const;

    template <typename T, typename... Args>
    T* add_setting(Args&&... args) {
        auto setting = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = setting.get();
        m_settings.push_back(std::move(setting));
        return ptr;
    }

    const std::vector<std::unique_ptr<config::Setting>>& settings() const { return m_settings; }

private:
    std::string m_category;
    std::string m_name;
    std::string m_tooltip;
    std::atomic<bool> m_enabled{ false };
    std::atomic<int> m_keybind{ 0 };
    std::atomic<bool> m_holdToEnable{ false };

    std::vector<std::unique_ptr<config::Setting>> m_settings;
};

// ==============================================================================
// HEADER: src/events/actor_tick_event.h
// ==============================================================================
class ClientInstance;
class LocalPlayer;

struct ActorTickEvent : public Event {
    ClientInstance* clientInstance = nullptr;
    LocalPlayer* localPlayer = nullptr;

    ActorTickEvent(ClientInstance* instance, LocalPlayer* player)
        : clientInstance(instance), localPlayer(player) {
    }
};

// ==============================================================================
// HEADER: src/utils/math.h
// ==============================================================================
namespace math {
    inline constexpr float kPi = 3.14159265358979323846f;

    struct Vec2 {
        float x = 0.0f, y = 0.0f;

        Vec2 operator*(float s) const { return { x * s, y * s }; }
        Vec2 operator/(float s) const { return s != 0.0f ? Vec2{ x / s, y / s } : Vec2{}; }
        Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
        Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
        Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
        Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

        float length() const { return std::sqrt(x * x + y * y); }
        float length_sq() const { return x * x + y * y; }
        float magnitude() const { return length(); }

        Vec2 normalized() const {
            const float len = length();
            return len > 0.0001f ? *this / len : Vec2{};
        }

        float distance(const Vec2& o) const { return (*this - o).length(); }
        float distance_sq(const Vec2& o) const { return (*this - o).length_sq(); }
    };

    struct Vec3 {
        float x = 0.0f, y = 0.0f, z = 0.0f;

        Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
        Vec3 operator/(float s) const { return s != 0.0f ? Vec3{ x / s, y / s, z / s } : Vec3{}; }
        Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
        Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
        Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

        float length() const { return std::sqrt(x * x + y * y + z * z); }
        float length_sq() const { return x * x + y * y + z * z; }
        float magnitude() const { return length(); }

        Vec3 normalized() const {
            const float len = length();
            return len > 0.0001f ? *this / len : Vec3{};
        }

        float distance(const Vec3& o) const { return (*this - o).length(); }
        float distance_sq(const Vec3& o) const { return (*this - o).length_sq(); }
    };

    struct ViewAngles {
        float pitch = 0.0f;
        float yaw = 0.0f;
    };

    inline Vec3 get_direction(ViewAngles angles) {
        const float pitchRad = angles.pitch * (kPi / 180.0f);
        const float yawRad = angles.yaw * (kPi / 180.0f);
        const float cosPitch = std::cos(pitchRad);

        return {
            -std::sin(yawRad) * cosPitch,
            -std::sin(pitchRad),
             std::cos(yawRad) * cosPitch
        };
    }

    inline void get_horizontal_vectors(float yaw, Vec3& forward, Vec3& right) {
        const float yawRad = yaw * (kPi / 180.0f);
        forward = { -std::sin(yawRad), 0.0f, std::cos(yawRad) };
        right = { -forward.z, 0.0f, forward.x };
    }
}

using Vec2 = math::Vec2;
using Vec3 = math::Vec3;
using ViewAngles = math::ViewAngles;

// ==============================================================================
// HEADER: src/minecraft/sdk/entity/i_entity_component.h
// ==============================================================================
struct IEntityComponent {};

// ==============================================================================
// HEADER: src/minecraft/sdk/components/AABBShapeComponent.h
// ==============================================================================
struct AABBShapeComponent : public IEntityComponent {
    math::Vec3 mMin{ 0.0f, 0.0f, 0.0f };
    math::Vec3 mMax{ 0.0f, 0.0f, 0.0f };
    float mWidth = 0.0f;
    float mHeight = 0.0f;

    [[nodiscard]] math::Vec3 center() const {
        return {
            (mMin.x + mMax.x) * 0.5f,
            (mMin.y + mMax.y) * 0.5f,
            (mMin.z + mMax.z) * 0.5f
        };
    }

    [[nodiscard]] math::Vec3 extents() const {
        return {
            (mMax.x - mMin.x) * 0.5f,
            (mMax.y - mMin.y) * 0.5f,
            (mMax.z - mMin.z) * 0.5f
        };
    }
};

static_assert(sizeof(AABBShapeComponent) == 0x20, "AABBShapeComponent size mismatch!");
static_assert(std::is_standard_layout_v<AABBShapeComponent>, "AABBShapeComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/RenderPositionComponent.h
// ==============================================================================
struct RenderPositionComponent : public IEntityComponent {
    math::Vec3 mPosition{ 0.0f, 0.0f, 0.0f };
};

static_assert(sizeof(RenderPositionComponent) == 0x0C, "RenderPositionComponent size mismatch!");
static_assert(std::is_standard_layout_v<RenderPositionComponent>, "RenderPositionComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorHeadRotationComponent.h
// ==============================================================================
struct ActorHeadRotationComponent : public IEntityComponent {
    math::Vec2 mHeadYaw{ 0.0f, 0.0f };
};

static_assert(sizeof(ActorHeadRotationComponent) == 0x8, "ActorHeadRotationComponent size mismatch!");
static_assert(std::is_standard_layout_v<ActorHeadRotationComponent>, "ActorHeadRotationComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/RuntimeIDComponent.h
// ==============================================================================
struct RuntimeIDComponent : public IEntityComponent {
    std::uint64_t mRuntimeId = 0;
};

static_assert(sizeof(RuntimeIDComponent) == 0x8, "RuntimeIDComponent size mismatch!");
static_assert(std::is_standard_layout_v<RuntimeIDComponent>, "RuntimeIDComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/entity/entity_id.h
// ==============================================================================
enum class EntityId : std::uint32_t {};

template <>
struct entt::internal::entt_traits<EntityId> {
    using value_type = EntityId;
    using entity_type = std::uint32_t;
    using version_type = std::uint16_t;

    static constexpr entity_type entity_mask = 0x3FFFF;
    static constexpr entity_type version_mask = 0x7FF;
};

// ==============================================================================
// HEADER: src/minecraft/sdk/entity/entity_context.h
// ==============================================================================
class EntityRegistry;

class EntityContext {
public:
    EntityRegistry* mRegistry = nullptr;
    entt::basic_registry<EntityId>& mEnttRegistry;
    EntityId mEntity;

    [[nodiscard]] inline entt::basic_registry<EntityId>& getRegistry() {
        return mEnttRegistry;
    }

    [[nodiscard]] inline const entt::basic_registry<EntityId>& getRegistry() const {
        return mEnttRegistry;
    }
};

// ==============================================================================
// HEADER: src/minecraft/sdk/components/StateVectorComponent.h
// ==============================================================================
struct StateVectorComponent : public IEntityComponent {
    math::Vec3 mPos;
    math::Vec3 mPosPrev;
    math::Vec3 mPosDelta;
};

static_assert(sizeof(StateVectorComponent) == 0x24, "StateVectorComponent size mismatch!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorRotationComponent.h
// ==============================================================================
struct ActorRotationComponent : public IEntityComponent {
    math::ViewAngles mRotation{ 0.0f, 0.0f };
    math::ViewAngles mOldRotation{ 0.0f, 0.0f };
};

static_assert(sizeof(ActorRotationComponent) == 0x10, "ActorRotationComponent size mismatch!");
static_assert(std::is_standard_layout_v<ActorRotationComponent>, "ActorRotationComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorDataFlagComponent.h
// ==============================================================================
enum class ActorFlags : int {
    Onfire = 0,
    Sneaking = 1,
    Riding = 2,
    Sprinting = 3,
    Usingitem = 4,
    Invisible = 5,
    Tempted = 6,
    Inlove = 7,
    Saddled = 8,
    Powered = 9,
    Ignited = 10,
    Baby = 11,
    Converting = 12,
    Critical = 13,
    CanShowName = 14,
    AlwaysShowName = 15,
    Noai = 16,
    Silent = 17,
    Wallclimbing = 18,
    Canclimb = 19,
    Canswim = 20,
    Canfly = 21,
    Canwalk = 22,
    Resting = 23,
    Sitting = 24,
    Angry = 25,
    Interested = 26,
    Charged = 27,
    Tamed = 28,
    Orphaned = 29,
    Leashed = 30,
    Sheared = 31,
    Gliding = 32,
    Elder = 33,
    Moving = 34,
    Breathing = 35,
    Chested = 36,
    Stackable = 37,
    ShowBottom = 38,
    Standing = 39,
    Shaking = 40,
    Idling = 41,
    Casting = 42,
    Charging = 43,
    WasdControlled = 44,
    CanPowerJump = 45,
    CanDash = 46,
    Lingering = 47,
    HasCollision = 48,
    HasGravity = 49,
    FireImmune = 50,
    Dancing = 51,
    Enchanted = 52,
    Returntrident = 53,
    ContainerIsPrivate = 54,
    IsTransforming = 55,
    Damagenearbymobs = 56,
    Swimming = 57,
    Bribed = 58,
    IsPregnant = 59,
    LayingEgg = 60,
    PassengerCanPick = 61,
    TransitionSitting = 62,
    Eating = 63,
    LayingDown = 64,
    Sneezing = 65,
    Trusting = 66,
    Rolling = 67,
    Scared = 68,
    InScaffolding = 69,
    OverScaffolding = 70,
    DescendThroughBlock = 71,
    Blocking = 72,
    TransitionBlocking = 73,
    BlockedUsingShield = 74,
    BlockedUsingDamagedShield = 75,
    Sleeping = 76,
    WantsToWake = 77,
    TradeInterest = 78,
    DoorBreaker = 79,
    BreakingObstruction = 80,
    DoorOpener = 81,
    IsIllagerCaptain = 82,
    Stunned = 83,
    Roaring = 84,
    DelayedAttack = 85,
    IsAvoidingMobs = 86,
    IsAvoidingBlock = 87,
    FacingTargetToRangeAttack = 88,
    HiddenWhenInvisible = 89,
    IsInUi = 90,
    Stalking = 91,
    Emoting = 92,
    Celebrating = 93,
    Admiring = 94,
    CelebratingSpecial = 95,
    OutOfControl = 96,
    RamAttack = 97,
    PlayingDead = 98,
    InAscendableBlock = 99,
    OverDescendableBlock = 100,
    Croaking = 101,
    EatMob = 102,
    JumpGoalJump = 103,
    Emerging = 104,
    Sniffing = 105,
    Digging = 106,
    SonicBoom = 107,
    HasDashCooldown = 108,
    PushTowardsClosestSpace = 109,
    Deprecated1 = 110,
    Deprecated2 = 111,
    Deprecated3 = 112,
    Searching = 113,
    Crawling = 114,
    TimerFlag1 = 115,
    TimerFlag2 = 116,
    TimerFlag3 = 117,
    BodyRotationBlocked = 118,
    RendersWhenInvisible = 119,
    RotationAxisAligned = 120,
    Collidable = 121,
    WasdFreeCameraControlled = 122,
    DoesServerAuthOnlyDismount = 123,
    BodyRotationAlwaysFollowsHead = 124,
    CanUseVerticalMovementAction = 125,
    RotationLockedToVehicle = 126,
    UsesLegacyFriction = 127,
    UsesUniformAirDrag = 128,
    NameplateDepthTested = 129,
    Count = 130
};

namespace brstd {
    template <std::size_t N, typename Word = std::uint64_t>
    struct bitset {
        static constexpr std::size_t kBitsPerWord = sizeof(Word) * 8;
        static constexpr std::size_t kWords = (N + kBitsPerWord - 1) / kBitsPerWord; 

        Word mStorage[kWords]{};

        [[nodiscard]] bool test(std::size_t index) const {
            if (index >= N) return false;
            return (mStorage[index / kBitsPerWord] & (Word(1) << (index % kBitsPerWord))) != 0;
        }

        void set(std::size_t index, bool value = true) {
            if (index >= N) return;
            if (value) {
                mStorage[index / kBitsPerWord] |= (Word(1) << (index % kBitsPerWord));
            }
            else {
                mStorage[index / kBitsPerWord] &= ~(Word(1) << (index % kBitsPerWord));
            }
        }
    };
}

template <typename T>
struct ActorDataComponentBase {
    T mValue{};
};

struct ActorDataFlagComponent : public IEntityComponent, public ActorDataComponentBase<brstd::bitset<130, std::uint64_t>> {
    [[nodiscard]] bool getFlag(ActorFlags flag) const {
        return mValue.test(static_cast<std::size_t>(flag));
    }

    void setFlag(ActorFlags flag, bool value) {
        mValue.set(static_cast<std::size_t>(flag), value);
    }
};

static_assert(sizeof(brstd::bitset<130, std::uint64_t>) == 0x18, "brstd::bitset size mismatch!");
static_assert(sizeof(ActorDataFlagComponent) == 0x18, "ActorDataFlagComponent size mismatch!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorGameTypeComponent.h
// ==============================================================================
enum class GameType : std::int32_t {
    Undefined = -1,
    Survival = 0,
    Creative = 1,
    Adventure = 2,
    Default = 5,
    Spectator = 6,
    WorldDefault = 0
};

struct ActorGameTypeComponent : public IEntityComponent {
    GameType mGameType = GameType::Undefined;
};

static_assert(sizeof(ActorGameTypeComponent) == 0x4, "ActorGameTypeComponent size mismatch!");
static_assert(std::is_standard_layout_v<ActorGameTypeComponent>, "ActorGameTypeComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorTypeComponent.h
// ==============================================================================
enum class ActorType : int {
    Undefined = 1,
    TypeMask = 255,
    Mob = 256,
    PathfinderMob = 768,
    Monster = 2816,
    Animal = 4864,
    TamableAnimal = 21248,
    Ambient = 33024,
    Undead = 65792,
    UndeadMonster = 68352,
    ZombieMonster = 199424,
    Arthropod = 264960,
    Minecart = 524288,
    SkeletonMonster = 1116928,
    EquineAnimal = 2118400,
    Projectile = 4194304,
    AbstractArrow = 8388608,
    WaterAnimal = 8960,
    VillagerBase = 16777984,
    Chicken = 4874,
    Cow = 4875,
    Pig = 4876,
    Sheep = 4877,
    Wolf = 21262,
    Villager = 16777999,
    MushroomCow = 4880,
    Squid = 8977,
    Rabbit = 4882,
    Bat = 33043,
    IronGolem = 788,
    SnowGolem = 789,
    Ocelot = 21270,
    Horse = 2118423,
    PolarBear = 4892,
    Llama = 4893,
    Parrot = 21278,
    Dolphin = 8991,
    Donkey = 2118424,
    Mule = 2118425,
    SkeletonHorse = 2183962,
    ZombieHorse = 2183963,
    Zombie = 199456,
    Creeper = 2849,
    Skeleton = 1116962,
    Spider = 264995,
    PigZombie = 68388,
    Slime = 2853,
    EnderMan = 2854,
    Silverfish = 264999,
    CaveSpider = 265000,
    Ghast = 2857,
    LavaSlime = 2858,
    Blaze = 2859,
    ZombieVillager = 199468,
    Witch = 2861,
    Stray = 1116974,
    Husk = 199471,
    WitherSkeleton = 1116976,
    Guardian = 2865,
    ElderGuardian = 2866,
    Npc = 307,
    WitherBoss = 68404,
    Dragon = 2869,
    Shulker = 2870,
    Endermite = 265015,
    Agent = 312,
    Vindicator = 2873,
    Phantom = 68410,
    IllagerBeast = 2875,
    ArmorStand = 317,
    TripodCamera = 318,
    Player = 319,
    ItemEntity = 64,
    PrimedTnt = 65,
    FallingBlock = 66,
    MovingBlock = 67,
    ExperiencePotion = 4194372,
    Experience = 69,
    EyeOfEnder = 70,
    EnderCrystal = 71,
    FireworksRocket = 72,
    Trident = 12582985,
    Turtle = 4938,
    Cat = 21323,
    ShulkerBullet = 4194380,
    FishingHook = 77,
    Chalkboard = 78,
    DragonFireball = 4194383,
    Arrow = 12582992,
    Snowball = 4194385,
    ThrownEgg = 4194386,
    Painting = 83,
    LargeFireball = 4194389,
    ThrownPotion = 4194390,
    Enderpearl = 4194391,
    LeashKnot = 88,
    WitherSkull = 4194393,
    BoatRideable = 90,
    WitherSkullDangerous = 4194395,
    LightningBolt = 93,
    SmallFireball = 4194398,
    AreaEffectCloud = 95,
    LingeringPotion = 4194405,
    LlamaSpit = 4194406,
    EvocationFang = 4194407,
    EvocationIllager = 2920,
    Vex = 2921,
    MinecartRideable = 524372,
    MinecartHopper = 524384,
    MinecartTNT = 524385,
    MinecartChest = 524386,
    MinecartFurnace = 524387,
    MinecartCommandBlock = 524388,
    IceBomb = 4194410,
    Balloon = 107,
    Pufferfish = 9068,
    Salmon = 9069,
    Drowned = 199534,
    Tropicalfish = 9071,
    Fish = 9072,
    Panda = 4977,
    Pillager = 2930,
    VillagerV2 = 16778099,
    ZombieVillagerV2 = 199540,
    Shield = 117,
    WanderingTrader = 886,
    Lectern = 119,
    ElderGuardianGhost = 2936,
    Fox = 4985,
    Bee = 378,
    Piglin = 379,
    Hoglin = 4988,
    Strider = 4989,
    Zoglin = 68478,
    PiglinBrute = 383,
    Goat = 4992,
    GlowSquid = 9089,
    Axolotl = 4994,
    Warden = 2947,
    Frog = 4996,
    Tadpole = 9093,
    Allay = 390,
    ChestBoatRideable = 218,
    TraderLlama = 5021,
    Camel = 5002,
    Sniffer = 5003,
    Breeze = 2956,
    BreezeWindChargeProjectile = 4194445,
    Armadillo = 5006,
    WindChargeProjectile = 4194447,
    Bogged = 1117072,
    OminousItemSpawner = 145,
    Creaking = 2962,
    HappyGhast = 5011,
    CopperGolem = 916,
    Nautilus = 9109,
    ZombieNautilus = 74646,
    Parched = 1117079,
    CamelHusk = 70552,
    SulfurCube = 2969
};

struct ActorTypeComponent : public IEntityComponent {
    ActorType mType = ActorType::Undefined;
};

static_assert(sizeof(ActorTypeComponent) == 0x4, "ActorTypeComponent size mismatch!");
static_assert(std::is_standard_layout_v<ActorTypeComponent>, "ActorTypeComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorOwnerComponent.h
// ==============================================================================
class Actor;

struct ActorOwnerComponent : public IEntityComponent {
    Actor* mActor = nullptr;
};

// ==============================================================================
// HEADER: src/minecraft/sdk/components/ActorUniqueIDComponent.h
// ==============================================================================
struct ActorUniqueIDComponent : public IEntityComponent {
    std::uint64_t mUniqueID = 0;
};

static_assert(sizeof(ActorUniqueIDComponent) == 0x8, "ActorUniqueIDComponent size mismatch!");
static_assert(std::is_standard_layout_v<ActorUniqueIDComponent>, "ActorUniqueIDComponent must be standard layout!");

// ==============================================================================
// HEADER: src/minecraft/sdk/components/FlagComponents.h
// ==============================================================================
struct OnGroundFlagComponent {};
struct InWaterFlag {};
struct InLavaFlag {};
struct ActorIsImmobileFlagComponent {};
struct IsDeadFlagComponent {};
struct MobIsJumpingFlagComponent {};
struct PermissionFlyFlagComponent {};
struct HasTeleportedFlagComponent {};
struct MoveRequestComponent {};

// ==============================================================================
// HEADER: src/minecraft/sdk/actor/actor.h
// ==============================================================================
class Actor {
public:
    virtual ~Actor() = default;

    EntityContext* getEntityContext() const {
        if (!this) return nullptr;
        return reinterpret_cast<EntityContext*>(reinterpret_cast<std::uintptr_t>(this) + 0x8);
    }

    template <typename T>
    bool hasComponent() const {
        EntityContext* context = getEntityContext();
        if (!context || !context->mRegistry) return false;
        return context->getRegistry().all_of<T>(context->mEntity);
    }

    template <typename T>
    T* getComponent() const {
        EntityContext* context = getEntityContext();
        if (!context || !context->mRegistry) return nullptr;
        return context->getRegistry().try_get<T>(context->mEntity);
    }

    template <typename T>
    void addComponent() {
        EntityContext* context = getEntityContext();
        if (!context || !context->mRegistry) return;
        context->getRegistry().get_or_emplace<T>(context->mEntity);
    }

    template <typename T>
    void removeComponent() {
        EntityContext* context = getEntityContext();
        if (!context || !context->mRegistry) return;
        context->getRegistry().remove<T>(context->mEntity);
    }

    std::uint64_t getUniqueId() const {
        if (auto* unique = getComponent<ActorUniqueIDComponent>()) {
            return unique->mUniqueID;
        }
        return 0;
    }

    ActorType getActorType() const {
        auto* comp = getComponent<ActorTypeComponent>();
        return comp ? comp->mType : ActorType::Undefined;
    }

    GameType getGameType() const {
        auto* comp = getComponent<ActorGameTypeComponent>();
        return comp ? comp->mGameType : GameType::Undefined;
    }

    void setGameType(GameType gameType) {
        if (auto* comp = getComponent<ActorGameTypeComponent>()) {
            comp->mGameType = gameType;
        }
    }

    bool getFlag(ActorFlags flag) const {
        if (auto* flags = getComponent<ActorDataFlagComponent>()) {
            return flags->getFlag(flag);
        }
        return false;
    }

    void setFlag(ActorFlags flag, bool value) {
        if (auto* flags = getComponent<ActorDataFlagComponent>()) {
            flags->setFlag(flag, value);
        }
    }

    math::Vec3 getVelocity() const {
        auto* stateVector = getComponent<StateVectorComponent>();
        return stateVector ? stateVector->mPosDelta : math::Vec3{ 0.0f, 0.0f, 0.0f };
    }

    void setVelocity(const math::Vec3& velocity) {
        if (auto* stateVector = getComponent<StateVectorComponent>()) {
            stateVector->mPosDelta = velocity;
        }
    }

    math::ViewAngles getRotation() const {
        if (auto* rot = getComponent<ActorRotationComponent>()) {
            return rot->mRotation;
        }
        return { 0.0f, 0.0f };
    }

    void setRotation(const math::ViewAngles& angles) {
        if (auto* rot = getComponent<ActorRotationComponent>()) {
            rot->mRotation = angles;
            rot->mOldRotation = angles;
        }
    }

    float getHeadYaw() const {
        if (auto* headRot = getComponent<ActorHeadRotationComponent>()) {
            return headRot->mHeadYaw.x;
        }
        return getRotation().yaw;
    }

    void setHeadYaw(float headYaw) {
        if (auto* headRot = getComponent<ActorHeadRotationComponent>()) {
            headRot->mHeadYaw = { headYaw, headYaw };
        }
    }

    std::uint64_t getRuntimeId() const {
        if (auto* runtime = getComponent<RuntimeIDComponent>()) {
            return runtime->mRuntimeId;
        }
        return 0;
    }

    AABBShapeComponent* getAABB() const {
        return getComponent<AABBShapeComponent>();
    }

    math::Vec3 getPosition() const {
        if (auto* stateVector = getComponent<StateVectorComponent>()) {
            if (stateVector->mPos.x != 0.0f || stateVector->mPos.y != 0.0f || stateVector->mPos.z != 0.0f) {
                return stateVector->mPos;
            }
        }
        if (auto* aabb = getAABB()) {
            if (aabb->mMin.x != 0.0f || aabb->mMin.y != 0.0f || aabb->mMin.z != 0.0f) {
                return {
                    (aabb->mMin.x + aabb->mMax.x) * 0.5f,
                    aabb->mMin.y,
                    (aabb->mMin.z + aabb->mMax.z) * 0.5f
                };
            }
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    math::Vec3 getRenderPosition() const {
        if (auto* renderPos = getComponent<RenderPositionComponent>()) {
            if (renderPos->mPosition.x != 0.0f || renderPos->mPosition.y != 0.0f || renderPos->mPosition.z != 0.0f) {
                return renderPos->mPosition;
            }
        }
        return getPosition();
    }

    [[nodiscard]] bool isAlive() const {
        return !hasComponent<IsDeadFlagComponent>();
    }

    [[nodiscard]] bool isPlayer() const {
        return getActorType() == ActorType::Player;
    }

    [[nodiscard]] static std::vector<Actor*> get_all(Actor* referenceActor) {
        std::vector<Actor*> actors;
        if (!referenceActor) return actors;

        EntityContext* context = referenceActor->getEntityContext();
        if (!context || !context->mRegistry) return actors;

        auto& registry = context->getRegistry();
        auto view = registry.view<ActorOwnerComponent>();

        for (auto entity : view) {
            auto* owner = registry.try_get<ActorOwnerComponent>(entity);
            if (owner && owner->mActor) {
                actors.push_back(owner->mActor);
            }
        }

        return actors;
    }
};

// ==============================================================================
// HEADER: src/utils/hash.h
// ==============================================================================
namespace utils::hash {

    // Compile time 64 bit FNV-1a string hashing
    constexpr std::uint64_t fnv1a_64(const char* str) noexcept {
        std::uint64_t hash = 0xCBF29CE484222325ULL;
        while (*str) {
            hash ^= static_cast<std::uint8_t>(*str++);
            hash *= 0x100000001B3ULL;
        }
        return hash;
    }

    constexpr std::uint64_t fnv1a_64(std::string_view str) noexcept {
        std::uint64_t hash = 0xCBF29CE484222325ULL;
        for (char c : str) {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 0x100000001B3ULL;
        }
        return hash;
    }

    inline namespace literals {
        constexpr std::uint64_t operator""_hash(const char* str, std::size_t len) noexcept {
            return fnv1a_64(std::string_view{str, len});
        }
    }

} // namespace utils::hash

// ==============================================================================
// HEADER: src/minecraft/bedrock_memory.h
// ==============================================================================
// Forward declarations
class LocalPlayer;
class ClientInstance;
class LevelRenderer;
class GuiData;
class ScreenContext;

#define DEFINE_SIG(name, readableName, patternStr) \
    struct name##_sig_t { \
        static constexpr const char*     symbol       = #name; \
        static constexpr const char*     display_name = (readableName); \
        static constexpr std::string_view pattern     {patternStr}; \
        static constexpr std::uint64_t   hash         = ::utils::hash::fnv1a_64(patternStr); \
        \
        [[nodiscard]] constexpr operator const char*() const noexcept { return patternStr; } \
        [[nodiscard]] constexpr operator std::string_view() const noexcept { return pattern; } \
        [[nodiscard]] static constexpr const char* get() noexcept { return patternStr; } \
        [[nodiscard]] static constexpr const char* name() noexcept { return display_name; } \
        [[nodiscard]] static constexpr std::string_view view() noexcept { return pattern; } \
    }; \
    static inline constexpr name##_sig_t name{}

#define DEFINE_OFFSET(OwnerClass, FieldType, name, offsetVal) \
    struct name##_offset_t { \
        using owner_type = OwnerClass; \
        using field_type = FieldType; \
        static constexpr std::uintptr_t  offset = (offsetVal); \
        static constexpr const char*     symbol = #name; \
        \
        template <typename T> \
        [[nodiscard]] constexpr FieldType& get(T* inst) const noexcept { \
            return *reinterpret_cast<FieldType*>(reinterpret_cast<std::uintptr_t>(inst) + offset); \
        } \
        template <typename T> \
        [[nodiscard]] constexpr FieldType& operator()(T* inst) const noexcept { \
            return get(inst); \
        } \
        [[nodiscard]] constexpr operator std::uintptr_t() const noexcept { \
            return offset; \
        } \
    }; \
    static inline constexpr name##_offset_t name{}

#define DEFINE_VTABLE_SLOT(name, slotIndex) \
    struct name##_slot_t { \
        static constexpr int         index  = (slotIndex); \
        static constexpr const char* symbol = #name; \
        [[nodiscard]] constexpr operator int() const noexcept { return index; } \
    }; \
    static inline constexpr name##_slot_t name{}

namespace minecraft {

    struct BedrockVtableSlots {
        DEFINE_VTABLE_SLOT(gameMode_Attack, 15);
    };

    struct BedrockOffsets {
        DEFINE_OFFSET(ClientInstance, void*,            clientInstance_levelRenderer, 0x1C0);
        DEFINE_OFFSET(ClientInstance, float*,           clientInstance_glMatrix,      0x420);
        DEFINE_OFFSET(ClientInstance, GuiData*,         clientInstance_guiData,       0x650);

        DEFINE_OFFSET(LocalPlayer,    void*,            localPlayer_gameMode,         0xAA0);

        DEFINE_OFFSET(LevelRenderer,  void*,            levelRenderer_renderPlayer,   0x468);
        DEFINE_OFFSET(void,           float*,           renderPlayer_cameraPos,       0x660);
        DEFINE_OFFSET(void,           float,            renderPlayer_fovX,            0xF58);
        DEFINE_OFFSET(void,           float,            renderPlayer_fovY,            0xF6C);

        DEFINE_OFFSET(GuiData,        float*,           guiData_windowSize,           0x040);
        DEFINE_OFFSET(ScreenContext,  float*,           screenContext_shaderColor,    0x030);
        DEFINE_OFFSET(ScreenContext,  void*,            screenContext_tessellator,    0x0B8);
    };

    struct BedrockSignatures {
        DEFINE_SIG(clientInstance_Update, "ClientInstance::update",
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? "
            "48 C7 85 ? ? ? ? FE FF FF FF 89 D3 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? "
            "48 8D 95 ? ? ? ? FF 15 ? ? ? ?");

        DEFINE_SIG(clientInstance_grabMouse, "ClientInstance::grabMouse",
            "56 48 83 EC 20 48 89 CE 48 8B 01 48 8B 80 10 05 00 00 FF 15 ? ? ? ? "
            "84 C0 74 ? 48 8B 8E A8 01 00 00 48 8B 01 48 8B 80 38 04 00 00");

        DEFINE_SIG(clientInstance_releaseMouse, "ClientInstance::releaseMouse",
            "56 48 83 EC 20 48 89 CE 48 8B 01 48 8B 80 10 05 00 00 FF 15 ? ? ? ? "
            "84 C0 74 ? 48 8B 8E A8 01 00 00 48 8B 01 48 8B 80 40 04 00 00");

        DEFINE_SIG(clientInstance_getLocalPlayer, "ClientInstance::getLocalPlayer",
            "48 83 EC 48 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 40 48 8D 91 ? ? ? ? "
            "48 8D 4C 24 20 E8 ? ? ? ? 80 7C 24 38 01");

        DEFINE_SIG(clientInstance_getScreenName, "ClientInstance::getScreenName",
            "55 56 57 48 83 EC ? 48 8D 6C 24 ? 48 C7 45 ? FE FF FF FF 48 89 D6 48 8B 01 "
            "48 8B 80 80 07 00 00 48 8D 55 ? FF 15 ? ? ? ? 48 83 7D ? 00 75 ? 48 8D 05 ? ? ? ? "
            "48 89 44 24 ? 48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 4C 8D 0D ? ? ? ? 41 B8 2C 01 00 00 "
            "E8 ? ? ? ? 84 C0 74 ? C7 04 25 00 00 00 00 DE C0 AD DE 48 8B 45 ? 80 38 00 75 ? "
            "48 8D 05 ? ? ? ? 48 89 44 24 ? 48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 4C 8D 0D ? ? ? ? "
            "41 B8 30 01 00 00 E8 ? ? ? ? 84 C0 74 ? C7 04 25 00 00 00 00 DE C0 AD DE 48 8B 4D ? "
            "48 8B 01 48 8B 80 60 01 00 00");

        DEFINE_SIG(actor_normalTick, "Actor::normalTick",
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? "
            "0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? FE FF FF FF 48 89 CE "
            "80 B9 ? ? ? ? 01 75 ? 83 BE ? ? ? ? 00");

        DEFINE_SIG(actor_setRotation, "Actor::setRotation",
            "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC D8 00 00 00 44 0F 29 94 24 C0 00 00 00 "
            "44 0F 29 8C 24 B0 00 00 00 44 0F 29 84 24 A0 00 00 00 0F 29 BC 24 90 00 00 00 "
            "0F 29 B4 24 80 00 00 00 4C 89 C6 49 8B 40 10 41 8B 78 18");

        DEFINE_SIG(options_getPerspective, "Options::getPerspective",
            "48 83 EC ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 01 "
            "48 8B 40 ? 48 8D 54 24 ? 41 B8 03 00 00 00");

        DEFINE_SIG(render_levelRender, "LevelRenderer::renderLevel",
            "E8 ? ? ? ? 45 31 E4 48 83 BE");

        DEFINE_SIG(render_tessVertex, "Tessellator::vertex",
            "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? "
            "44 0F 29 ? ? 44 0F 29 ? ? 0F 29 ? ? 0F 29 ? ? 48 C7 ? ? FE FF FF FF "
            "0F 28 F3 0F 28 FA 44 0F 28 C1 48 89 CE 48 8B 0D ? ? ? ? 48 8B 01");

        DEFINE_SIG(render_tessBegin, "Tessellator::begin",
            "56 57 55 53 48 83 EC ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? "
            "80 B9 ? ? ? ? ? 0F 85 ? ? ? ? 80 B9");

        DEFINE_SIG(render_tessColor, "Tessellator::color",
            "80 B9 ? ? ? ? ? 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 0A");

        DEFINE_SIG(render_meshImmediate, "MeshHelpers::renderMeshImmediately",
            "55 41 57 41 56 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? "
            "48 C7 85 ? ? ? ? ? ? ? ? 80 BA ? ? ? ? ? 0F 85 ? ? ? ? 4C 89 CF");

        DEFINE_SIG(render_uiFillColor, "MaterialPtr::getUIColor",
            "C6 45 D0 00 4C 8D 05 ? ? ? ? 4C 8D 4D B0 48 89 F1 48 89 FA E8");
    };

} // namespace minecraft

// ==============================================================================
// HEADER: src/minecraft/sdk/actor/game_mode.h
// ==============================================================================
class Actor;

class GameMode {
public:
    virtual ~GameMode() = default;                                                                                   // 0  (0x00)
    virtual bool startDestroyBlock(const void* pos, std::uint8_t face, bool& isDestroyedOut) = 0;                    // 1  (0x08)
    virtual bool destroyBlock(const void* pos, std::uint8_t face) = 0;                                               // 2  (0x10)
    virtual void continueDestroyBlock(const void* pos, std::uint8_t face, const Vec3& playerPos, bool& out) = 0;    // 3  (0x18)
    virtual void stopDestroyBlock(const void* pos) = 0;                                                              // 4  (0x20)
    virtual void startBuildBlock(const void* pos, std::uint8_t face) = 0;                                           // 5  (0x28)
    virtual bool buildBlock(const void* pos, std::uint8_t face, bool isAuthed) = 0;                                 // 6  (0x30)
    virtual void continueBuildBlock(const void* pos, std::uint8_t face) = 0;                                        // 7  (0x38)
    virtual void stopBuildBlock() = 0;                                                                               // 8  (0x40)
    virtual void tick() = 0;                                                                                         // 9  (0x48)
    virtual float getPickRange(const void* inputMode, bool isVR) = 0;                                                // 10 (0x50)
    virtual bool useItem(void* itemStack) = 0;                                                                       // 11 (0x58)
    virtual bool useItemAsAttack(void* itemStack) = 0;                                                               // 12 (0x60)
    virtual bool useItemOn(void* itemStack, const void* pos, std::uint8_t face, const Vec3& clickPos, void* block) = 0; // 13 (0x68)
    virtual bool interact(Actor* target, const Vec3& location) = 0;                                                  // 14 (0x70)
    virtual void attack(Actor* target) = 0;                                                                          // 15 (0x78)
    virtual void releaseUsingItem() = 0;                                                                             // 16 (0x80)
};

// ==============================================================================
// HEADER: src/minecraft/sdk/actor/local_player.h
// ==============================================================================
class LocalPlayer : public Actor {
public:
    [[nodiscard]] GameMode* getGameMode() const {
        if (!this) return nullptr;
        __try {
            const auto ptr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::localPlayer_gameMode;
            if (ptr < 0x10000) return nullptr;
            return *reinterpret_cast<GameMode**>(ptr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }
};

// ==============================================================================
// HEADER: src/modules/movement/Jetpack.h
// ==============================================================================
class Jetpack final : public Module {
public:
    Jetpack();

    void on_enable() override;
    void on_disable() override;

private:
    EventBus::ListenerId m_tickListener = 0;

    config::FloatSetting* m_speed = nullptr;
};

// ==============================================================================
// HEADER: src/modules/movement/Fly.h
// ==============================================================================
class Fly final : public Module {
public:
    Fly();

    void on_enable() override;
    void on_disable() override;

private:
    EventBus::ListenerId m_tickListener = 0;

    config::FloatSetting* m_speed = nullptr;
    config::FloatSetting* m_verticalSpeed = nullptr;
    config::FloatSetting* m_glide = nullptr;
};

// ==============================================================================
// HEADER: src/modules/visual/Arraylist.h
// ==============================================================================
class ArrayList final : public Module {
public:
    ArrayList();

    void on_imgui_render() override;

private:
    struct Element {
        std::string text;
        float width = 0.0f;
    };

    enum Position {
        TopRight = 0,
        TopLeft,
        BottomRight,
        BottomLeft
    };

    config::EnumSetting* m_position = nullptr;
    config::BoolSetting* m_showSideBar = nullptr;
    config::FloatSetting* m_bgOpacity = nullptr;
    config::BoolSetting* m_hideSelf = nullptr;
};

// ==============================================================================
// HEADER: src/events/level_render_event.h
// ==============================================================================
class LevelRenderer;
class ScreenContext;

struct LevelRenderEvent : public Event {
    LevelRenderer* levelRenderer = nullptr;
    ScreenContext* screenContext = nullptr;

    LevelRenderEvent(LevelRenderer* renderer, ScreenContext* context)
        : levelRenderer(renderer), screenContext(context) {
    }
};

// ==============================================================================
// HEADER: src/modules/visual/ESP.h
// ==============================================================================
class ESP final : public Module {
public:
    ESP();

    void on_enable() override;
    void on_disable() override;

private:
    void render(LevelRenderEvent& event);

    EventBus::ListenerId m_renderListener = 0;

    config::BoolSetting* m_targetPlayers = nullptr;
    config::BoolSetting* m_targetMobs = nullptr;
    config::ColorSetting* m_color = nullptr;
    config::FloatSetting* m_thickness = nullptr;
    config::FloatSetting* m_fillOpacity = nullptr;
};

// ==============================================================================
// HEADER: src/modules/visual/Trail.h
// ==============================================================================
class Trail final : public Module {
public:
    Trail();

    void on_enable() override;
    void on_disable() override;
    void on_imgui_render() override;

private:
    struct TrailPoint {
        math::Vec3 position;
        float age = 0.0f;
    };

    EventBus::ListenerId m_tickListener = 0;
    std::mutex m_mutex;
    std::vector<TrailPoint> m_points;

    config::FloatSetting* m_maxAge = nullptr;
    config::FloatSetting* m_spacing = nullptr;
    config::ColorSetting* m_color = nullptr;
    config::FloatSetting* m_thickness = nullptr;
};

// ==============================================================================
// HEADER: src/modules/visual/Freelook.h
// ==============================================================================
class Freelook final : public Module {
public:
    Freelook();

    void on_enable() override;
    void on_disable() override;

private:
    EventBus::ListenerId m_rotListener = 0;
    math::ViewAngles m_targetRotation{ 0.0f, 0.0f };
    bool m_hasTarget = false;
};

// ==============================================================================
// HEADER: src/modules/combat/Killaura.h
// ==============================================================================
class Killaura final : public Module {
public:
    Killaura();

    void on_enable() override;
    void on_disable() override;

private:
    EventBus::ListenerId m_tickListener = 0;
    int m_tickCounter = 0;

    config::FloatSetting* m_range = nullptr;
    config::IntSetting* m_delay = nullptr;
    config::BoolSetting* m_targetMobs = nullptr;
    config::BoolSetting* m_targetPlayers = nullptr;
};

// ==============================================================================
// HEADER: src/modules/module_manager.h
// ==============================================================================
// include module headers here

class ModuleManager {
public:
    static ModuleManager& instance();

    void initialize();
    void shutdown();

    void on_imgui_render();

    std::vector<Module*> modules() const;
    std::vector<std::string> categories() const;
    std::vector<Module*> modules_in_category(const std::string& category) const;

private:
    ModuleManager() = default;

    void add_module(std::unique_ptr<Module> module);
    void rebuild_categories();

    mutable std::shared_mutex m_mutex;
    bool m_initialized = false;
    std::vector<std::unique_ptr<Module>> m_modules;
    std::vector<std::string> m_categories;
};

// ==============================================================================
// HEADER: src/render/dx12/dx12_hook.h
// ==============================================================================
namespace dx12_hook {
bool start(HMODULE module);
void stop();
bool is_menu_open();
void set_menu_open(bool open);
}

// ==============================================================================
// HEADER: src/utils/color.h
// ==============================================================================
enum class TextColor {
    Black,
    DarkBlue,
    DarkGreen,
    DarkAqua,
    DarkRed,
    DarkPurple,
    Gold,
    Gray,
    DarkGray,
    Blue,
    Green,
    Aqua,
    Red,
    LightPurple,
    Yellow,
    White,
    Obfuscated,
    Bold,
    Strikethrough,
    Underline,
    Italic,
    Reset
};

constexpr const char* color_code(TextColor color) {
    switch (color) {
    case TextColor::Black:         return "\u00A70";
    case TextColor::DarkBlue:      return "\u00A71";
    case TextColor::DarkGreen:     return "\u00A72";
    case TextColor::DarkAqua:      return "\u00A73";
    case TextColor::DarkRed:       return "\u00A74";
    case TextColor::DarkPurple:    return "\u00A75";
    case TextColor::Gold:          return "\u00A76";
    case TextColor::Gray:          return "\u00A77";
    case TextColor::DarkGray:      return "\u00A78";
    case TextColor::Blue:          return "\u00A79";
    case TextColor::Green:         return "\u00A7a";
    case TextColor::Aqua:          return "\u00A7b";
    case TextColor::Red:           return "\u00A7c";
    case TextColor::LightPurple:   return "\u00A7d";
    case TextColor::Yellow:        return "\u00A7e";
    case TextColor::White:         return "\u00A7f";
    case TextColor::Obfuscated:    return "\u00A7k";
    case TextColor::Bold:          return "\u00A7l";
    case TextColor::Strikethrough: return "\u00A7m";
    case TextColor::Underline:     return "\u00A7n";
    case TextColor::Italic:        return "\u00A7o";
    case TextColor::Reset:         return "\u00A7r";
    default:                       return "\u00A7r";
    }
}

inline std::string color(TextColor col, std::string_view text) {
    std::string result;
    result.reserve(text.size() + 6);
    result += color_code(col);
    result += text;
    result += "\u00A7r";
    return result;
}

namespace Color {
    inline constexpr const char* Black         = "\u00A70";
    inline constexpr const char* DarkBlue      = "\u00A71";
    inline constexpr const char* DarkGreen     = "\u00A72";
    inline constexpr const char* DarkAqua      = "\u00A73";
    inline constexpr const char* DarkRed       = "\u00A74";
    inline constexpr const char* DarkPurple    = "\u00A75";
    inline constexpr const char* Gold          = "\u00A76";
    inline constexpr const char* Gray          = "\u00A77";
    inline constexpr const char* DarkGray      = "\u00A78";
    inline constexpr const char* Blue          = "\u00A79";
    inline constexpr const char* Green         = "\u00A7a";
    inline constexpr const char* Aqua          = "\u00A7b";
    inline constexpr const char* Red           = "\u00A7c";
    inline constexpr const char* LightPurple   = "\u00A7d";
    inline constexpr const char* Yellow        = "\u00A7e";
    inline constexpr const char* White         = "\u00A7f";
    inline constexpr const char* Obfuscated    = "\u00A7k";
    inline constexpr const char* Bold          = "\u00A7l";
    inline constexpr const char* Strikethrough = "\u00A7m";
    inline constexpr const char* Underline     = "\u00A7n";
    inline constexpr const char* Italic        = "\u00A7o";
    inline constexpr const char* Reset         = "\u00A7r";
}

// ==============================================================================
// HEADER: src/utils/logger.h
// ==============================================================================
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger {
public:
    static Logger& instance();

    void initialize(const std::string& clientName = "MCBE_ModBase", bool enableConsole = true, bool enableFile = true);
    void shutdown();

    void set_level(LogLevel level) { m_minLevel = level; }
    LogLevel get_level() const { return m_minLevel; }

    template <typename... Args>
    void log(LogLevel level, const std::source_location loc, std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_minLevel || !m_initialized) return;

        std::string formatted_msg = std::vformat(fmt.get(), std::make_format_args(args...));
        write_log(level, loc, formatted_msg);
    }

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void write_log(LogLevel level, const std::source_location& loc, const std::string& message);
    std::filesystem::path get_log_directory() const;

    bool m_initialized = false;
    bool m_consoleAllocated = false;
    bool m_fileOutput = false;
    LogLevel m_minLevel = LogLevel::Info;

    std::ofstream m_logFile;
    std::mutex m_mutex;
};

#define LOG_TRACE(...) ::Logger::instance().log(LogLevel::Trace, std::source_location::current(), __VA_ARGS__)
#define LOG_DEBUG(...) ::Logger::instance().log(LogLevel::Debug, std::source_location::current(), __VA_ARGS__)
#define LOG_INFO(...)  ::Logger::instance().log(LogLevel::Info,  std::source_location::current(), __VA_ARGS__)
#define LOG_WARN(...)  ::Logger::instance().log(LogLevel::Warn,  std::source_location::current(), __VA_ARGS__)
#define LOG_ERROR(...) ::Logger::instance().log(LogLevel::Error, std::source_location::current(), __VA_ARGS__)
#define LOG_FATAL(...) ::Logger::instance().log(LogLevel::Fatal, std::source_location::current(), __VA_ARGS__)

// ==============================================================================
// HEADER: src/input/input_manager.h
// ==============================================================================
class Module;

class InputManager {
public:
    static InputManager& instance();

    bool handle_win32_message(UINT msg, WPARAM wparam, LPARAM lparam);

    bool is_key_down(int vkCode) const;
    void clear_key_states();

    void begin_binding(Module* module);
    void cancel_binding();
    bool is_binding() const;
    Module* binding_module() const;

private:
    InputManager() = default;

    int extract_mouse_key(UINT msg, WPARAM wparam, bool& isDown);

    std::array<bool, 256> m_keyStates{};
    Module* m_bindingModule = nullptr;
};

// ==============================================================================
// HEADER: src/events/key_event.h
// ==============================================================================
enum class KeyAction {
    Press,
    Release,
    Repeat
};

struct KeyEvent : public Event {
    int key = 0;
    KeyAction action = KeyAction::Press;
    bool isMouse = false;

    KeyEvent(int k, KeyAction act, bool mouse = false)
        : key(k), action(act), isMouse(mouse) {
    }
};

// ==============================================================================
// HEADER: src/input/key_names.h
// ==============================================================================
namespace input {
    const char* get_key_name(int vkCode);
}

// ==============================================================================
// HEADER: src/minecraft/hooks/actor_set_rot_hook.h
// ==============================================================================
namespace minecraft::hooks {
    bool install_actor_set_rot_hook(std::uintptr_t setRotAddr);
    void remove_actor_set_rot_hook();
}

// ==============================================================================
// HEADER: src/minecraft/sdk/client/gl_matrix.h
// ==============================================================================
struct GLMatrix {
    float matrix[16]{};

    [[nodiscard]] math::Vec3 transform(const math::Vec3& p) const {
        return {
            matrix[0] * p.x + matrix[1] * p.y + matrix[2] * p.z + matrix[3],
            matrix[4] * p.x + matrix[5] * p.y + matrix[6] * p.z + matrix[7],
            matrix[8] * p.x + matrix[9] * p.y + matrix[10] * p.z + matrix[11]
        };
    }
};

// ==============================================================================
// HEADER: src/minecraft/sdk/client/client_instance.h
// ==============================================================================
class LocalPlayer;

class GuiData {
public:
    [[nodiscard]] math::Vec2 getWindowSize() const {
        if (!this) return { 0.0f, 0.0f };
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::guiData_windowSize;
            if (addr < 0x10000) return { 0.0f, 0.0f };
            return *reinterpret_cast<const math::Vec2*>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return { 0.0f, 0.0f };
        }
    }
};

class LevelRendererPlayer {
public:
    [[nodiscard]] math::Vec3 getOrigin() const {
        if (!this) return { 0.0f, 0.0f, 0.0f };
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::renderPlayer_cameraPos;
            if (addr < 0x10000) return { 0.0f, 0.0f, 0.0f };
            return *reinterpret_cast<const math::Vec3*>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return { 0.0f, 0.0f, 0.0f };
        }
    }

    [[nodiscard]] float getFovX() const {
        if (!this) return 1.0f;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::renderPlayer_fovX;
            if (addr < 0x10000) return 1.0f;
            return *reinterpret_cast<const float*>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 1.0f;
        }
    }

    [[nodiscard]] float getFovY() const {
        if (!this) return 1.0f;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::renderPlayer_fovY;
            if (addr < 0x10000) return 1.0f;
            return *reinterpret_cast<const float*>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 1.0f;
        }
    }
};

class LevelRenderer {
public:
    [[nodiscard]] LevelRendererPlayer* getLevelRendererPlayer() const {
        if (!this) return nullptr;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::levelRenderer_renderPlayer;
            if (addr < 0x10000) return nullptr;
            return *reinterpret_cast<LevelRendererPlayer**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }
};

class ClientInstance {
public:
    static ClientInstance* get() {
        return s_instance.load(std::memory_order_acquire);
    }

    static void set(ClientInstance* instance) {
        s_instance.store(instance, std::memory_order_release);
    }

    static LocalPlayer* getLocalPlayer() {
        return s_localPlayer.load(std::memory_order_acquire);
    }

    static void setLocalPlayer(LocalPlayer* player) {
        s_localPlayer.store(player, std::memory_order_release);
    }

    [[nodiscard]] LevelRenderer* getLevelRenderer() const {
        if (!this) return nullptr;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::clientInstance_levelRenderer;
            if (addr < 0x10000) return nullptr;
            return *reinterpret_cast<LevelRenderer**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    [[nodiscard]] GuiData* getGuiData() const {
        if (!this) return nullptr;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::clientInstance_guiData;
            if (addr < 0x10000) return nullptr;
            return *reinterpret_cast<GuiData**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    [[nodiscard]] GLMatrix getGLMatrix() const {
        if (!this) return {};
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::clientInstance_glMatrix;
            if (addr < 0x10000) return {};
            return *reinterpret_cast<const GLMatrix*>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return {};
        }
    }

    [[nodiscard]] bool world_to_screen(const math::Vec3& worldPos, math::Vec2& screenPos) const;

    void releaseMouse();
    void grabMouse();

    [[nodiscard]] static std::string getScreenName();
    [[nodiscard]] static bool isGameUiOpen();
    [[nodiscard]] static bool isClientUiOpen();

private:
    static inline std::atomic<ClientInstance*> s_instance{ nullptr };
    static inline std::atomic<LocalPlayer*> s_localPlayer{ nullptr };
};

// ==============================================================================
// HEADER: src/events/actor_set_rot_event.h
// ==============================================================================
class Actor;
class LocalPlayer;

struct ActorSetRotEvent : public Event {
    Actor* actor = nullptr;
    LocalPlayer* localPlayer = nullptr;

    ActorSetRotEvent(Actor* targetActor, LocalPlayer* player = nullptr)
        : actor(targetActor), localPlayer(player) {
    }

    [[nodiscard]] bool isLocalPlayer() const {
        return localPlayer != nullptr;
    }
};

// ==============================================================================
// HEADER: src/minecraft/hooks/actor_tick_hook.h
// ==============================================================================
namespace minecraft::hooks {
    void set_actor_tick_address(std::uintptr_t normalTickAddr);
    void install_actor_tick_hook(void* localPlayer, void* clientInstance);
    void remove_actor_tick_hook();
}

// ==============================================================================
// HEADER: src/minecraft/hooks/attack_hook.h
// ==============================================================================
class LocalPlayer;

namespace minecraft::hooks {
    void install_attack_hook(LocalPlayer* localPlayer);
    void remove_attack_hook();
}

// ==============================================================================
// HEADER: src/minecraft/hooks/hook_memory.h
// ==============================================================================
namespace minecraft::hook_memory {

    inline std::uintptr_t find_signature(std::string_view signatureStr) {
        const auto parsed = ::hat::parse_signature(signatureStr);
        if (!parsed.has_value()) {
            return 0;
        }

        const auto result = ::hat::find_pattern(parsed.value(), ".text");
        if (!result.has_result() || !result.get()) {
            return 0;
        }

        return reinterpret_cast<std::uintptr_t>(result.get());
    }

    inline std::uintptr_t resolve_signature(const char* name, std::string_view signatureStr) {
        const std::uintptr_t addr = find_signature(signatureStr);
        if (addr) {
            LOG_INFO("[Signature] {:<32} -> {} ({:#x})", name, color(TextColor::Green, "OK"), addr);
        }
        else {
            LOG_ERROR("[Signature] {:<32} -> {}!", name, color(TextColor::Red, "FAILED"));
        }
        return addr;
    }

    template <typename SigT>
    inline std::uintptr_t resolve_signature(const SigT& sig) {
        return resolve_signature(SigT::display_name, SigT::pattern);
    }

    inline std::uintptr_t vtable_entry(void* instance, int index) {
        if (!instance) {
            return 0;
        }

        __try {
            const auto table = *reinterpret_cast<std::uintptr_t**>(instance);
            return table ? table[index] : 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    template <typename Return, typename... Args>
    Return call_vtable(void* instance, int index, Args... args) {
        using Fn = Return(__fastcall*)(void*, Args...);
        const std::uintptr_t target = vtable_entry(instance, index);
        if (!target) {
            return Return{};
        }

        return reinterpret_cast<Fn>(target)(instance, args...);
    }

    template <typename Return, typename Instance, typename... Args>
    Return call_function(std::uintptr_t funcAddress, Instance* instance, Args... args) {
        using Fn = Return(__fastcall*)(Instance*, Args...);
        if (!funcAddress || !instance) {
            return Return{};
        }
        return reinterpret_cast<Fn>(funcAddress)(instance, args...);
    }

    inline std::uintptr_t resolve_call(std::uintptr_t addr) {
        if (!addr) return 0;
        __try {
            const auto disp = *reinterpret_cast<std::int32_t*>(addr + 1);
            return addr + 5 + static_cast<std::uintptr_t>(disp);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    inline std::uintptr_t resolve_rip_relative(std::uintptr_t addr, std::size_t dispOffset, std::size_t instrLen) {
        if (!addr) return 0;
        __try {
            const auto disp = *reinterpret_cast<std::int32_t*>(addr + dispOffset);
            return addr + instrLen + static_cast<std::uintptr_t>(disp);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

}

// ==============================================================================
// HEADER: src/events/attack_event.h
// ==============================================================================
class GameMode;
class LocalPlayer;
class Actor;

struct AttackEvent : public Event {
    GameMode* gameMode = nullptr;
    LocalPlayer* localPlayer = nullptr;
    Actor* target = nullptr;

    AttackEvent(GameMode* gm, LocalPlayer* player, Actor* targetActor)
        : gameMode(gm), localPlayer(player), target(targetActor) {
    }
};

// ==============================================================================
// HEADER: src/minecraft/hooks/client_instance_hook.h
// ==============================================================================
namespace minecraft::hooks {
    bool install_client_instance_hook(std::uintptr_t updateAddr);
    void remove_client_instance_hook();
}

// ==============================================================================
// HEADER: src/minecraft/hooks/screen_hook.h
// ==============================================================================
class ClientInstance;

namespace minecraft::hooks {
    bool install_screen_hook(std::uintptr_t screenNameAddr);
    void remove_screen_hook();

    void update_screen_name(ClientInstance* self);
    std::string get_current_screen_name();
    bool is_game_ui_open();

    void set_screen_override(const std::string& screenName);
    void clear_screen_override();
}

// ==============================================================================
// HEADER: src/minecraft/hooks/grab_mouse_hook.h
// ==============================================================================
class ClientInstance;

namespace minecraft::hooks {
    bool install_grab_mouse_hook(std::uintptr_t grabMouseAddr, std::uintptr_t releaseMouseAddr);
    void remove_grab_mouse_hook();

    void grab_mouse(ClientInstance* instance = nullptr);
    void release_mouse(ClientInstance* instance = nullptr);
}

// ==============================================================================
// HEADER: src/events/mouse_grab_event.h
// ==============================================================================
class ClientInstance;

struct MouseGrabEvent : public Event {
    ClientInstance* clientInstance = nullptr;

    explicit MouseGrabEvent(ClientInstance* instance)
        : clientInstance(instance) {
    }
};

// ==============================================================================
// HEADER: src/events/perspective_event.h
// ==============================================================================
enum class PerspectiveView : int {
    FirstPerson = 0,
    ThirdPersonBack = 1,
    ThirdPersonFront = 2
};

struct PerspectiveEvent : public Event {
    int perspective = 0;
    int overridePerspective = -1;

    explicit PerspectiveEvent(int currentPerspective)
        : perspective(currentPerspective) {
    }

    [[nodiscard]] PerspectiveView view() const {
        return static_cast<PerspectiveView>(perspective);
    }

    [[nodiscard]] bool isFirstPerson() const {
        return perspective == 0;
    }

    [[nodiscard]] bool isThirdPerson() const {
        return perspective > 0;
    }

    [[nodiscard]] bool isOverridden() const {
        return overridePerspective >= 0;
    }

    void setOverride(int newPerspective) {
        overridePerspective = newPerspective;
    }

    void setOverride(PerspectiveView newPerspective) {
        overridePerspective = static_cast<int>(newPerspective);
    }
};

using PerspectiveViewEvent = PerspectiveEvent;

// ==============================================================================
// HEADER: src/minecraft/hooks/perspective_hook.h
// ==============================================================================
namespace minecraft::hooks {
    bool install_perspective_hook(std::uintptr_t funcAddr);
    void remove_perspective_hook();

    int get_perspective();
    void set_perspective(int perspective);
}

// ==============================================================================
// HEADER: src/minecraft/hooks/level_render_hook.h
// ==============================================================================
namespace minecraft::hooks {
    bool install_level_render_hook(std::uintptr_t callSigAddr);
    void remove_level_render_hook();
}

// ==============================================================================
// HEADER: src/minecraft/sdk/render/screen_context.h
// ==============================================================================
class Tessellator;

namespace mce {
    struct Color {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    };
}

class ScreenContext {
public:
    [[nodiscard]] mce::Color* getShaderColor() const {
        if (!this) return nullptr;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::screenContext_shaderColor;
            if (addr < 0x10000) return nullptr;
            return *reinterpret_cast<mce::Color**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    [[nodiscard]] Tessellator* getTessellator() const {
        if (!this) return nullptr;
        __try {
            const auto addr = reinterpret_cast<std::uintptr_t>(this) + minecraft::BedrockOffsets::screenContext_tessellator;
            if (addr < 0x10000) return nullptr;
            return *reinterpret_cast<Tessellator**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    static inline ScreenContext* s_instance3d = nullptr;
};

// ==============================================================================
// HEADER: src/events/screen_change_event.h
// ==============================================================================
class ClientInstance;

struct ScreenChangeEvent : public Event {
    ClientInstance* clientInstance = nullptr;
    std::string previousScreen;
    std::string currentScreen;
    std::string overrideScreen;
    bool overridden = false;

    ScreenChangeEvent(ClientInstance* instance, std::string prev, std::string current)
        : clientInstance(instance), previousScreen(std::move(prev)), currentScreen(std::move(current)) {
    }

    [[nodiscard]] bool isOverridden() const {
        return overridden;
    }

    void setOverride(std::string newScreen) {
        overrideScreen = std::move(newScreen);
        overridden = true;
    }
};

// ==============================================================================
// HEADER: src/minecraft/sdk/render/material_ptr.h
// ==============================================================================
struct RenderMaterialInfo;

class MaterialPtr {
public:
    std::shared_ptr<RenderMaterialInfo> self;

    MaterialPtr() = default;
    MaterialPtr(const MaterialPtr& other) : self(other.self) {}
    MaterialPtr& operator=(const MaterialPtr& other) { self = other.self; return *this; }

    [[nodiscard]] void* info() const { return reinterpret_cast<void*>(self.get()); }

    static MaterialPtr* getUIColor();
};

// ==============================================================================
// HEADER: src/minecraft/sdk/render/tessellator.h
// ==============================================================================
enum class Primitive : int {
    Quad = 1,
    TriangleFan = 2,
    TriangleStrip = 3,
    LineList = 4
};

class Tessellator {
public:
    void vertex(float x, float y, float z);
    void begin(Primitive primitive, int numVertices = 0);
    void color(float r, float g, float b, float a);
    void color(const mce::Color& col);
};

// ==============================================================================
// HEADER: src/minecraft/sdk/render/draw_3d.h
// ==============================================================================
class Draw3D {
public:
    Draw3D();
    ~Draw3D();

    [[nodiscard]] bool is_valid() const;

    void draw_line(const math::Vec3& from, const math::Vec3& to, const mce::Color& color);
    void draw_box(const math::Vec3& min, const math::Vec3& max, const mce::Color& color);
    void fill_box(const math::Vec3& min, const math::Vec3& max, const mce::Color& color);
    void flush();

private:
    struct ColorVertex {
        float x, y, z;
        float r, g, b, a;
    };

    ScreenContext* m_screenContext = nullptr;
    Tessellator* m_tessellator = nullptr;
    MaterialPtr* m_material = nullptr;
    math::Vec3 m_origin{};

    std::vector<ColorVertex> m_lineVerts;
    std::vector<ColorVertex> m_quadVerts;

    void push_line(const math::Vec3& from, const math::Vec3& to, const mce::Color& color);
    void push_quad(const math::Vec3& a, const math::Vec3& b, const math::Vec3& c, const math::Vec3& d, const mce::Color& color);
};

// ==============================================================================
// HEADER: src/minecraft/sdk/render/mesh_helpers.h
// ==============================================================================
class ScreenContext;
class Tessellator;
class MaterialPtr;

namespace MeshHelpers {
    void renderMeshImmediately(ScreenContext* ctx, Tessellator* tess, MaterialPtr* material);
}

// ==============================================================================
// HEADER: src/ui/clickgui.h
// ==============================================================================
namespace clickgui {
void render();
}

namespace menu {
inline void render() { clickgui::render(); }
}

// ==============================================================================
// HEADER: src/ui/ui_settings.h
// ==============================================================================
namespace ui_settings {
float ui_scale();
void set_ui_scale(float scale);
void apply_ui_scale();
void reset_ui_scale_cache();
}

// (was repeated at file scope in several .cpp files)
using namespace minecraft;

// ==============================================================================
// SOURCE: src/client.cpp
// ==============================================================================
Client& Client::instance() {
    static Client client;
    return client;
}

void Client::initialize(HMODULE module) {
    m_module = module;

    // Initialize Logger
    Logger::instance().initialize("MCBE Client", true, true);

    LOG_INFO("Initializing MCBE DX12 ModBase...");

    // Initialize module manager
    ModuleManager::instance().initialize();

    // Install hooks & event listeners
    if (!minecraft::hooks::initialize()) {
        LOG_ERROR("Failed to initialize game hooks!");
        shutdown();
        FreeLibraryAndExitThread(m_module, 0);
        return;
    }

    // Start DX12/ImGui hook
    if (!dx12_hook::start(m_module)) {
        LOG_ERROR("Failed to start DX12 overlay hooks!");
        shutdown();
        FreeLibraryAndExitThread(m_module, 0);
        return;
    }

    // Safely defer config loading until ClientInstance is ready
    EventBus::instance().subscribe<ClientInstanceUpdateEvent>([this](ClientInstanceUpdateEvent&) {
        if (!m_configLoaded) {
            m_configLoaded = true;
            LOG_INFO("ClientInstance initialized. Loading default configuration profile...");
            ConfigManager::instance().load_config("default");
        }
        });

    LOG_INFO("MCBE Client initialized successfully!");
}

void Client::shutdown() {
    LOG_INFO("Shutting down MCBE Client...");
    dx12_hook::stop();
    minecraft::hooks::shutdown();
    ModuleManager::instance().shutdown();
    Logger::instance().shutdown();
    LOG_INFO("MCBE Client shutdown complete.");
}

// ==============================================================================
// SOURCE: src/config/config_manager.cpp
// ==============================================================================
ConfigManager& ConfigManager::instance() {
    static ConfigManager manager;
    return manager;
}

std::filesystem::path ConfigManager::get_config_directory() const {
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path path = std::filesystem::path(appdata) / "MCBE_DX12_ModBase" / "configs";
        free(appdata);
        return path;
    }
    return std::filesystem::current_path() / "configs";
}

void ConfigManager::ensure_directory_exists() const {
    std::filesystem::create_directories(get_config_directory());
}

bool ConfigManager::save_config(const std::string& configName) {
    try {
        ensure_directory_exists();
        std::filesystem::path filePath = get_config_directory() / (configName + ".json");

        nlohmann::json root = nlohmann::json::object();
        nlohmann::json modulesJson = nlohmann::json::object();

        for (Module* module : ModuleManager::instance().modules()) {
            if (!module) continue;

            nlohmann::json modData;
            modData["enabled"] = module->enabled();
            modData["keybind"] = module->keybind();
            modData["hold_to_enable"] = module->hold_to_enable();

            nlohmann::json settingsJson = nlohmann::json::object();
            for (const auto& setting : module->settings()) {
                if (setting) {
                    settingsJson[setting->name()] = setting->serialize();
                }
            }
            modData["settings"] = settingsJson;

            modulesJson[module->name()] = modData;
        }

        root["modules"] = modulesJson;

        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for saving config: '{}'", configName);
            return false;
        }

        file << root.dump(4);
        LOG_INFO("Config profile '{}' saved successfully.", configName);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception occurred while saving config '{}': {}", configName, e.what());
        return false;
    }
}

bool ConfigManager::load_config(const std::string& configName) {
    try {
        std::filesystem::path filePath = get_config_directory() / (configName + ".json");
        if (!std::filesystem::exists(filePath)) {
            LOG_WARN("Config profile '{}' does not exist.", configName);
            return false;
        }

        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file: '{}'", configName);
            return false;
        }

        nlohmann::json root;
        file >> root;

        if (!root.contains("modules") || !root["modules"].is_object()) {
            LOG_ERROR("Invalid config file structure for profile: '{}'", configName);
            return false;
        }

        const auto& modulesJson = root["modules"];

        for (Module* module : ModuleManager::instance().modules()) {
            if (!module || !modulesJson.contains(module->name())) continue;

            const auto& modData = modulesJson[module->name()];

            if (modData.contains("enabled") && modData["enabled"].is_boolean()) {
                module->set_enabled(modData["enabled"].get<bool>());
            }
            if (modData.contains("keybind") && modData["keybind"].is_number_integer()) {
                module->set_keybind(modData["keybind"].get<int>());
            }
            if (modData.contains("hold_to_enable") && modData["hold_to_enable"].is_boolean()) {
                module->set_hold_to_enable(modData["hold_to_enable"].get<bool>());
            }

            if (modData.contains("settings") && modData["settings"].is_object()) {
                const auto& settingsJson = modData["settings"];
                for (const auto& setting : module->settings()) {
                    if (setting && settingsJson.contains(setting->name())) {
                        setting->deserialize(settingsJson[setting->name()]);
                    }
                }
            }
        }

        LOG_INFO("Config profile '{}' loaded successfully.", configName);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception occurred while loading config '{}': {}", configName, e.what());
        return false;
    }
}

bool ConfigManager::delete_config(const std::string& configName) {
    try {
        std::filesystem::path filePath = get_config_directory() / (configName + ".json");
        if (std::filesystem::exists(filePath)) {
            bool removed = std::filesystem::remove(filePath);
            if (removed) {
                LOG_INFO("Deleted config profile: '{}'", configName);
            }
            return removed;
        }
        return false;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to delete config profile '{}': {}", configName, e.what());
        return false;
    }
}

std::vector<std::string> ConfigManager::get_available_configs() const {
    std::vector<std::string> configs;
    try {
        ensure_directory_exists();
        for (const auto& entry : std::filesystem::directory_iterator(get_config_directory())) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                configs.push_back(entry.path().stem().string());
            }
        }
    }
    catch (...) {}
    return configs;
}

// ==============================================================================
// SOURCE: src/input/input_manager.cpp
// ==============================================================================
InputManager& InputManager::instance() {
    static InputManager manager;
    return manager;
}

bool InputManager::is_key_down(int vkCode) const {
    if (vkCode <= 0 || vkCode >= 256) return false;
    return m_keyStates[vkCode];
}

void InputManager::clear_key_states() {
    m_keyStates.fill(false);
}

void InputManager::begin_binding(Module* module) {
    m_bindingModule = module;
}

void InputManager::cancel_binding() {
    m_bindingModule = nullptr;
}

bool InputManager::is_binding() const {
    return m_bindingModule != nullptr;
}

Module* InputManager::binding_module() const {
    return m_bindingModule;
}

int InputManager::extract_mouse_key(UINT msg, WPARAM wparam, bool& isDown) {
    switch (msg) {
    case WM_LBUTTONDOWN: isDown = true;  return VK_LBUTTON;
    case WM_LBUTTONUP:   isDown = false; return VK_LBUTTON;
    case WM_RBUTTONDOWN: isDown = true;  return VK_RBUTTON;
    case WM_RBUTTONUP:   isDown = false; return VK_RBUTTON;
    case WM_MBUTTONDOWN: isDown = true;  return VK_MBUTTON;
    case WM_MBUTTONUP:   isDown = false; return VK_MBUTTON;
    case WM_XBUTTONDOWN: isDown = true;  return (HIWORD(wparam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
    case WM_XBUTTONUP:   isDown = false; return (HIWORD(wparam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
    default: return 0;
    }
}

bool InputManager::handle_win32_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    bool isKeyDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
    bool isKeyUp = (msg == WM_KEYUP || msg == WM_SYSKEYUP);
    bool isMouse = false;
    int vkCode = 0;

    if (isKeyDown || isKeyUp) {
        vkCode = static_cast<int>(wparam);
    }
    else {
        vkCode = extract_mouse_key(msg, wparam, isKeyDown);
        if (vkCode != 0) {
            isMouse = true;
            isKeyUp = !isKeyDown;
        }
    }

    if (vkCode <= 0 || vkCode >= 256) {
        return false;
    }

    const bool repeated = isKeyDown && ((lparam & (1 << 30)) != 0);
    const KeyAction action = isKeyUp ? KeyAction::Release : (repeated ? KeyAction::Repeat : KeyAction::Press);

    if (action == KeyAction::Press)   m_keyStates[vkCode] = true;
    if (action == KeyAction::Release) m_keyStates[vkCode] = false;

    if (m_bindingModule && action == KeyAction::Press) {
        if (vkCode == VK_ESCAPE) {
            cancel_binding();
        }
        else if (vkCode == VK_DELETE || vkCode == VK_BACK) {
            m_bindingModule->set_keybind(0);
            cancel_binding();
        }
        else {
            m_bindingModule->set_keybind(vkCode);
            cancel_binding();
        }
        return true; // Consume message during binding
    }

    KeyEvent event(vkCode, action, isMouse);
    EventBus::instance().publish(event);

    return event.is_cancelled();
}

// ==============================================================================
// SOURCE: src/input/key_names.cpp
// ==============================================================================
namespace input {

    struct KeyNameEntry {
        int vk;
        const char* name;
    };

    static constexpr KeyNameEntry kKeyTable[] = {
        // Mouse
        { VK_LBUTTON, "M1" }, { VK_RBUTTON, "M2" }, { VK_MBUTTON, "M3" },
        { VK_XBUTTON1, "M4" }, { VK_XBUTTON2, "M5" },

        // Special Keys
        { VK_BACK, "Back" }, { VK_TAB, "Tab" }, { VK_RETURN, "Enter" },
        { VK_SHIFT, "Shift" }, { VK_LSHIFT, "LShift" }, { VK_RSHIFT, "RShift" },
        { VK_CONTROL, "Ctrl" }, { VK_LCONTROL, "LCtrl" }, { VK_RCONTROL, "RCtrl" },
        { VK_MENU, "Alt" }, { VK_LMENU, "LAlt" }, { VK_RMENU, "RAlt" },
        { VK_PAUSE, "Pause" }, { VK_CAPITAL, "Caps" }, { VK_ESCAPE, "Esc" },
        { VK_SPACE, "Space" }, { VK_PRIOR, "PgUp" }, { VK_NEXT, "PgDn" },
        { VK_END, "End" }, { VK_HOME, "Home" }, { VK_LEFT, "Left" },
        { VK_UP, "Up" }, { VK_RIGHT, "Right" }, { VK_DOWN, "Down" },
        { VK_INSERT, "Ins" }, { VK_DELETE, "Del" },

        // Numpad
        { VK_NUMPAD0, "N0" }, { VK_NUMPAD1, "N1" }, { VK_NUMPAD2, "N2" },
        { VK_NUMPAD3, "N3" }, { VK_NUMPAD4, "N4" }, { VK_NUMPAD5, "N5" },
        { VK_NUMPAD6, "N6" }, { VK_NUMPAD7, "N7" }, { VK_NUMPAD8, "N8" },
        { VK_NUMPAD9, "N9" }, { VK_MULTIPLY, "N*" }, { VK_ADD, "N+" },
        { VK_SEPARATOR, "N," }, { VK_SUBTRACT, "N-" }, { VK_DECIMAL, "N." },
        { VK_DIVIDE, "N/" },

        // Function Keys
        { VK_F1, "F1" }, { VK_F2, "F2" }, { VK_F3, "F3" }, { VK_F4, "F4" },
        { VK_F5, "F5" }, { VK_F6, "F6" }, { VK_F7, "F7" }, { VK_F8, "F8" },
        { VK_F9, "F9" }, { VK_F10, "F10" }, { VK_F11, "F11" }, { VK_F12, "F12" },

        // OEM / Punctuation
        { VK_OEM_1, ";" }, { VK_OEM_PLUS, "=" }, { VK_OEM_COMMA, "," },
        { VK_OEM_MINUS, "-" }, { VK_OEM_PERIOD, "." }, { VK_OEM_2, "/" },
        { VK_OEM_3, "`" }, { VK_OEM_4, "[" }, { VK_OEM_5, "\\" },
        { VK_OEM_6, "]" }, { VK_OEM_7, "'" }
    };

    const char* get_key_name(int vkCode) {
        if (vkCode == 0) return "None";

        for (const auto& entry : kKeyTable) {
            if (entry.vk == vkCode) {
                return entry.name;
            }
        }

        // Thread safe dynamic fallback for Standard Alphanumeric A-Z, 0-9
        thread_local static char text[2] = {};
        if (vkCode > 0 && vkCode < 256 && std::isalnum(vkCode)) {
            text[0] = static_cast<char>(vkCode);
            text[1] = '\0';
            return text;
        }

        return "?";
    }

}

// ==============================================================================
// SOURCE: src/minecraft/hooks/actor_set_rot_hook.cpp
// ==============================================================================
namespace {
    using ActorSetRotFn = float*(__fastcall*)(void* rotationAngles, void* rotationContext, Actor* actor);
    ActorSetRotFn g_originalActorSetRot = nullptr;
    void* g_actorSetRotTarget = nullptr;

    float* __fastcall hook_actor_set_rot(void* rotationAngles, void* rotationContext, Actor* actor) {
        float* result = g_originalActorSetRot 
            ? g_originalActorSetRot(rotationAngles, rotationContext, actor) 
            : nullptr;

        LocalPlayer* localPlayer = ClientInstance::getLocalPlayer();
        if (actor != nullptr && localPlayer != nullptr && actor == reinterpret_cast<Actor*>(localPlayer)) {
            ActorSetRotEvent event(actor, localPlayer);
            EventBus::instance().publish(event);
        }

        return result;
    }
}

namespace minecraft::hooks {
    bool install_actor_set_rot_hook(std::uintptr_t setRotAddr) {
        if (!setRotAddr) {
            LOG_ERROR("Invalid Actor::setRotation address provided!");
            return false;
        }

        LOG_INFO("Hooking Actor::setRotation at address: {:#x}", setRotAddr);

        g_actorSetRotTarget = reinterpret_cast<void*>(setRotAddr);
        if (MH_CreateHook(g_actorSetRotTarget, reinterpret_cast<LPVOID>(&hook_actor_set_rot), reinterpret_cast<void**>(&g_originalActorSetRot)) != MH_OK) {
            LOG_ERROR("Failed to create Actor::setRotation hook!");
            g_actorSetRotTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_actorSetRotTarget) != MH_OK) {
            LOG_ERROR("Failed to enable Actor::setRotation hook!");
            return false;
        }

        LOG_INFO("Actor::setRotation hook enabled.");
        return true;
    }

    void remove_actor_set_rot_hook() {
        if (g_actorSetRotTarget) {
            MH_DisableHook(g_actorSetRotTarget);
            MH_RemoveHook(g_actorSetRotTarget);
            g_actorSetRotTarget = nullptr;
            g_originalActorSetRot = nullptr;
            LOG_INFO("Actor::setRotation hook removed.");
        }
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/actor_tick_hook.cpp
// ==============================================================================
namespace {
    using ActorTickFn = void(__fastcall*)(void* self);
    ActorTickFn g_originalActorTick = nullptr;
    void* g_actorTickTarget = nullptr;
    std::uintptr_t g_normalTickAddr = 0;

    std::atomic<void*> actor_tick_hook_g_currentLocalPlayer{ nullptr };
    std::atomic<void*> g_currentClientInstance{ nullptr };
    std::mutex g_hookMutex;

    void __fastcall hook_actor_normal_tick(void* self) {
        if (g_originalActorTick) {
            g_originalActorTick(self);
        }

        void* expectedPlayer = actor_tick_hook_g_currentLocalPlayer.load(std::memory_order_acquire);
        void* clientInst = g_currentClientInstance.load(std::memory_order_acquire);

        if (self != nullptr && expectedPlayer != nullptr && self == expectedPlayer && clientInst != nullptr) {
            ClientInstance::setLocalPlayer(static_cast<LocalPlayer*>(self));
            ActorTickEvent event(static_cast<ClientInstance*>(clientInst), static_cast<LocalPlayer*>(self));
            EventBus::instance().publish(event);
        }
    }
}

namespace minecraft::hooks {
    void set_actor_tick_address(std::uintptr_t normalTickAddr) {
        g_normalTickAddr = normalTickAddr;
    }

    void install_actor_tick_hook(void* localPlayer, void* clientInstance) {
        actor_tick_hook_g_currentLocalPlayer.store(localPlayer, std::memory_order_release);
        g_currentClientInstance.store(clientInstance, std::memory_order_release);

        if (!localPlayer) return;

        std::lock_guard<std::mutex> lock(g_hookMutex);
        if (g_actorTickTarget != nullptr || !g_normalTickAddr) {
            return;
        }

        g_actorTickTarget = reinterpret_cast<void*>(g_normalTickAddr);
        if (MH_CreateHook(g_actorTickTarget, reinterpret_cast<LPVOID>(&hook_actor_normal_tick), reinterpret_cast<void**>(&g_originalActorTick)) == MH_OK) {
            MH_EnableHook(g_actorTickTarget);
            LOG_INFO("Actor::normalTick hook attached successfully.");
        }
        else {
            LOG_ERROR("Failed to create Actor::normalTick hook!");
            g_actorTickTarget = nullptr;
        }
    }

    void remove_actor_tick_hook() {
        std::lock_guard<std::mutex> lock(g_hookMutex);

        ClientInstance::setLocalPlayer(nullptr);
        actor_tick_hook_g_currentLocalPlayer.store(nullptr, std::memory_order_release);
        g_currentClientInstance.store(nullptr, std::memory_order_release);

        if (g_actorTickTarget) {
            MH_DisableHook(g_actorTickTarget);
            MH_RemoveHook(g_actorTickTarget);
            g_actorTickTarget = nullptr;
            g_originalActorTick = nullptr;
        }
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/attack_hook.cpp
// ==============================================================================
namespace {
    using GameModeAttackFn = void(__fastcall*)(void* self, void* target);
    GameModeAttackFn g_originalAttack = nullptr;
    std::uintptr_t g_currentHookedAddress = 0;

    std::atomic<LocalPlayer*> attack_hook_g_currentLocalPlayer{ nullptr };
    std::mutex g_attackMutex;
    std::chrono::steady_clock::time_point g_lastHookCheck;

    // Passive observer hook
    void __fastcall hook_gamemode_attack(void* self, void* target) {
        if (g_originalAttack) {
            g_originalAttack(self, target);
        }

        if (self && target) {
            LOG_INFO("[AttackHook] Attack triggered on target: {:p}", target);

            LocalPlayer* player = attack_hook_g_currentLocalPlayer.load(std::memory_order_relaxed);
            AttackEvent event(reinterpret_cast<GameMode*>(self), player, reinterpret_cast<Actor*>(target));
            EventBus::instance().publish(event);
        }
    }
}

namespace minecraft::hooks {
    void install_attack_hook(LocalPlayer* localPlayer) {
        attack_hook_g_currentLocalPlayer.store(localPlayer, std::memory_order_relaxed);

        if (!localPlayer) {
            return;
        }

        GameMode* gameMode = localPlayer->getGameMode();
        if (!gameMode) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        if (g_currentHookedAddress != 0 && std::chrono::duration_cast<std::chrono::seconds>(now - g_lastHookCheck).count() < 2) {
            return;
        }
        g_lastHookCheck = now;

        const std::uintptr_t funcAddr = hook_memory::vtable_entry(gameMode, BedrockVtableSlots::gameMode_Attack);
        if (!funcAddr) {
            return;
        }

        std::lock_guard<std::mutex> lock(g_attackMutex);
        if (g_currentHookedAddress == funcAddr) {
            return;
        }

        if (g_currentHookedAddress != 0) {
            MH_DisableHook(reinterpret_cast<LPVOID>(g_currentHookedAddress));
            MH_RemoveHook(reinterpret_cast<LPVOID>(g_currentHookedAddress));
            g_currentHookedAddress = 0;
            g_originalAttack = nullptr;
        }

        if (MH_CreateHook(reinterpret_cast<LPVOID>(funcAddr), reinterpret_cast<LPVOID>(&hook_gamemode_attack), reinterpret_cast<void**>(&g_originalAttack)) == MH_OK) {
            if (MH_EnableHook(reinterpret_cast<LPVOID>(funcAddr)) == MH_OK) {
                g_currentHookedAddress = funcAddr;
                LOG_INFO("[AttackHook] GameMode::attack hooked successfully at {:#x}", funcAddr);
            }
        }
    }

    void remove_attack_hook() {
        std::lock_guard<std::mutex> lock(g_attackMutex);
        attack_hook_g_currentLocalPlayer.store(nullptr, std::memory_order_relaxed);

        if (g_currentHookedAddress != 0) {
            MH_DisableHook(reinterpret_cast<LPVOID>(g_currentHookedAddress));
            MH_RemoveHook(reinterpret_cast<LPVOID>(g_currentHookedAddress));
            g_currentHookedAddress = 0;
            g_originalAttack = nullptr;
            LOG_INFO("[AttackHook] GameMode::attack unhooked.");
        }
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/client_instance_hook.cpp
// ==============================================================================
namespace {
    using ClientUpdateFn = bool(__fastcall*)(ClientInstance* self, bool isInitFinished);
    ClientUpdateFn g_originalClientUpdate = nullptr;
    void* g_clientUpdateTarget = nullptr;

    bool __fastcall hook_client_instance_update(ClientInstance* self, bool isInitFinished) {
        if (self) {
            ClientInstance::set(self);
            minecraft::hooks::update_screen_name(self);
        }

        ClientInstanceUpdateEvent event(self, isInitFinished);
        EventBus::instance().publish(event);

        return g_originalClientUpdate ? g_originalClientUpdate(self, isInitFinished) : false;
    }
}

namespace minecraft::hooks {
    bool install_client_instance_hook(std::uintptr_t updateAddr) {
        if (!updateAddr) {
            LOG_ERROR("Invalid ClientInstance::update address provided!");
            return false;
        }

        LOG_INFO("Hooking ClientInstance::update at address: {:#x}", updateAddr);

        g_clientUpdateTarget = reinterpret_cast<void*>(updateAddr);
        if (MH_CreateHook(g_clientUpdateTarget, reinterpret_cast<LPVOID>(&hook_client_instance_update), reinterpret_cast<void**>(&g_originalClientUpdate)) != MH_OK) {
            LOG_ERROR("Failed to create ClientInstance::update hook!");
            g_clientUpdateTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_clientUpdateTarget) != MH_OK) {
            LOG_ERROR("Failed to enable ClientInstance::update hook!");
            return false;
        }

        LOG_INFO("ClientInstance update hook enabled.");
        return true;
    }

    void remove_client_instance_hook() {
        ClientInstance::set(nullptr);
        ClientInstance::setLocalPlayer(nullptr);

        if (g_clientUpdateTarget) {
            MH_DisableHook(g_clientUpdateTarget);
            MH_RemoveHook(g_clientUpdateTarget);
            g_clientUpdateTarget = nullptr;
            g_originalClientUpdate = nullptr;
            LOG_INFO("ClientInstance hook removed.");
        }
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/grab_mouse_hook.cpp
// ==============================================================================
namespace {
    using GrabMouseFn = void(__fastcall*)(ClientInstance* self);
    using ReleaseMouseFn = void(__fastcall*)(ClientInstance* self);

    GrabMouseFn g_originalGrabMouse = nullptr;
    void* g_grabMouseTarget = nullptr;
    std::uintptr_t g_releaseMouseAddr = 0;
    EventBus::ListenerId g_updateListener = 0;

    void __fastcall hook_client_instance_grab_mouse(ClientInstance* self) {
        MouseGrabEvent event(self);
        EventBus::instance().publish(event);

        if (event.is_cancelled() || dx12_hook::is_menu_open()) {
            return;
        }

        if (g_originalGrabMouse) {
            g_originalGrabMouse(self);
        }
    }

    void grab_mouse_hook_on_client_update(ClientInstanceUpdateEvent& event) {
        if (!event.clientInstance) return;

        static bool wasMenuOpen = false;
        const bool menuOpen = dx12_hook::is_menu_open();
        if (menuOpen != wasMenuOpen) {
            if (menuOpen) {
                minecraft::hooks::release_mouse(event.clientInstance);
            } else {
                if (ClientInstance::getLocalPlayer() && !event.clientInstance->isGameUiOpen()) {
                    minecraft::hooks::grab_mouse(event.clientInstance);
                }
            }
            wasMenuOpen = menuOpen;
        }
    }
}

namespace minecraft::hooks {
    bool install_grab_mouse_hook(std::uintptr_t grabMouseAddr, std::uintptr_t releaseMouseAddr) {
        if (!grabMouseAddr) {
            LOG_ERROR("Invalid ClientInstance::grabMouse address provided!");
            return false;
        }

        g_releaseMouseAddr = releaseMouseAddr;

        LOG_INFO("Hooking ClientInstance::grabMouse at address: {:#x}", grabMouseAddr);

        g_grabMouseTarget = reinterpret_cast<void*>(grabMouseAddr);
        if (MH_CreateHook(g_grabMouseTarget, reinterpret_cast<LPVOID>(&hook_client_instance_grab_mouse), reinterpret_cast<void**>(&g_originalGrabMouse)) != MH_OK) {
            LOG_ERROR("Failed to create ClientInstance::grabMouse hook!");
            g_grabMouseTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_grabMouseTarget) != MH_OK) {
            LOG_ERROR("Failed to enable ClientInstance::grabMouse hook!");
            return false;
        }

        g_updateListener = EventBus::instance().subscribe<ClientInstanceUpdateEvent>(grab_mouse_hook_on_client_update);

        LOG_INFO("ClientInstance grabMouse hook enabled.");
        return true;
    }

    void remove_grab_mouse_hook() {
        if (g_updateListener != 0) {
            EventBus::instance().unsubscribe(g_updateListener);
            g_updateListener = 0;
        }

        if (g_grabMouseTarget) {
            MH_DisableHook(g_grabMouseTarget);
            MH_RemoveHook(g_grabMouseTarget);
            g_grabMouseTarget = nullptr;
            g_originalGrabMouse = nullptr;
            LOG_INFO("ClientInstance grabMouse hook removed.");
        }

        g_releaseMouseAddr = 0;
    }

    void grab_mouse(ClientInstance* instance) {
        if (!instance) {
            instance = ClientInstance::get();
        }
        if (!instance || !g_grabMouseTarget) return;

        __try {
            reinterpret_cast<GrabMouseFn>(g_grabMouseTarget)(instance);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void release_mouse(ClientInstance* instance) {
        if (!instance) {
            instance = ClientInstance::get();
        }
        if (!instance || !g_releaseMouseAddr) return;

        __try {
            reinterpret_cast<ReleaseMouseFn>(g_releaseMouseAddr)(instance);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/hook_manager.cpp
// ==============================================================================
namespace minecraft::hooks {

    namespace {
        std::uintptr_t s_getLocalPlayerAddr = 0;

        void hook_manager_on_client_update(ClientInstanceUpdateEvent& event) {
            if (!s_getLocalPlayerAddr) return;

            void* localPlayer = hook_memory::call_function<void*>(
                s_getLocalPlayerAddr,
                event.clientInstance);

            if (localPlayer) {
                ClientInstance::setLocalPlayer(static_cast<LocalPlayer*>(localPlayer));
                install_actor_tick_hook(localPlayer, event.clientInstance);
                install_attack_hook(static_cast<LocalPlayer*>(localPlayer));
            }
        }
    }

    bool initialize() {
        LOG_INFO("Initializing MinHook framework...");
        if (MH_Initialize() != MH_OK) {
            LOG_ERROR("Failed to initialize MinHook!");
            return false;
        }

        LOG_INFO("Resolving game signatures...");

        const std::uintptr_t updateAddr = hook_memory::resolve_signature(BedrockSignatures::clientInstance_Update);
        const std::uintptr_t grabMouseAddr = hook_memory::resolve_signature(BedrockSignatures::clientInstance_grabMouse);
        const std::uintptr_t releaseMouseAddr = hook_memory::resolve_signature(BedrockSignatures::clientInstance_releaseMouse);
        const std::uintptr_t screenNameAddr = hook_memory::resolve_signature(BedrockSignatures::clientInstance_getScreenName);
        s_getLocalPlayerAddr = hook_memory::resolve_signature(BedrockSignatures::clientInstance_getLocalPlayer);
        const std::uintptr_t actorTickAddr = hook_memory::resolve_signature(BedrockSignatures::actor_normalTick);
        const std::uintptr_t actorSetRotAddr = hook_memory::resolve_signature(BedrockSignatures::actor_setRotation);
        const std::uintptr_t perspectiveAddr = hook_memory::resolve_signature(BedrockSignatures::options_getPerspective);
        const std::uintptr_t levelRenderCallAddr = hook_memory::resolve_signature(BedrockSignatures::render_levelRender);

        if (!updateAddr || !s_getLocalPlayerAddr || !actorTickAddr || !levelRenderCallAddr) {
            LOG_FATAL("One or more required signatures failed to resolve!");
            return false;
        }

        set_actor_tick_address(actorTickAddr);

        EventBus::instance().subscribe<ClientInstanceUpdateEvent>(hook_manager_on_client_update);

        if (!install_client_instance_hook(updateAddr)) {
            LOG_ERROR("Failed to install ClientInstance::update hook!");
            return false;
        }

        if (grabMouseAddr) {
            if (!install_grab_mouse_hook(grabMouseAddr, releaseMouseAddr)) {
                LOG_ERROR("Failed to install ClientInstance::grabMouse hook!");
            }
        }

        if (screenNameAddr) {
            if (!install_screen_hook(screenNameAddr)) {
                LOG_ERROR("Failed to install ClientInstance::getScreenName hook!");
            }
        }

        if (actorSetRotAddr) {
            if (!install_actor_set_rot_hook(actorSetRotAddr)) {
                LOG_ERROR("Failed to install Actor::setRotation hook!");
            }
        }

        if (perspectiveAddr) {
            if (!install_perspective_hook(perspectiveAddr)) {
                LOG_ERROR("Failed to install Options::getPerspective hook!");
            }
        }

        if (!install_level_render_hook(levelRenderCallAddr)) {
            LOG_ERROR("Failed to install LevelRenderer::renderLevel hook!");
            return false;
        }

        LOG_INFO("All game hooks initialized successfully.");
        return true;
    }

    void shutdown() {
        LOG_INFO("Shutting down game hooks...");
        ClientInstance::setLocalPlayer(nullptr);
        remove_screen_hook();
        remove_grab_mouse_hook();
        remove_level_render_hook();
        remove_perspective_hook();
        remove_actor_set_rot_hook();
        remove_attack_hook();
        remove_actor_tick_hook();
        remove_client_instance_hook();
        MH_Uninitialize();
    }

}

// ==============================================================================
// SOURCE: src/minecraft/hooks/level_render_hook.cpp
// ==============================================================================
namespace {
    using LevelRenderFn = __int64(__fastcall*)(LevelRenderer*, void*, __int64);
    LevelRenderFn g_originalLevelRender = nullptr;
    void* g_levelRenderTarget = nullptr;

    __int64 __fastcall hook_level_renderer_render_level(
        LevelRenderer* self, void* screenContext, __int64 renderParams)
    {
        ScreenContext::s_instance3d = reinterpret_cast<ScreenContext*>(screenContext);

        __int64 result = g_originalLevelRender
            ? g_originalLevelRender(self, screenContext, renderParams)
            : 0;

        if (self && ClientInstance::getLocalPlayer()) {
            LevelRenderEvent event(self, ScreenContext::s_instance3d);
            EventBus::instance().publish(event);
        }

        ScreenContext::s_instance3d = nullptr;
        return result;
    }
}

namespace minecraft::hooks {

    bool install_level_render_hook(std::uintptr_t callSigAddr) {
        if (!callSigAddr) {
            LOG_ERROR("Invalid LevelRenderer::renderLevel call signature address!");
            return false;
        }

        std::uintptr_t funcAddr = minecraft::hook_memory::resolve_call(callSigAddr);
        if (!funcAddr) {
            LOG_ERROR("Failed to resolve LevelRenderer::renderLevel call target!");
            return false;
        }

        LOG_INFO("Hooking LevelRenderer::renderLevel at {:#x} (call at {:#x})", funcAddr, callSigAddr);

        g_levelRenderTarget = reinterpret_cast<void*>(funcAddr);
        if (MH_CreateHook(g_levelRenderTarget, &hook_level_renderer_render_level,
                          reinterpret_cast<void**>(&g_originalLevelRender)) != MH_OK) {
            LOG_ERROR("Failed to create LevelRenderer::renderLevel hook!");
            g_levelRenderTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_levelRenderTarget) != MH_OK) {
            LOG_ERROR("Failed to enable LevelRenderer::renderLevel hook!");
            return false;
        }

        LOG_INFO("LevelRenderer::renderLevel hook enabled.");
        return true;
    }

    void remove_level_render_hook() {
        if (g_levelRenderTarget) {
            MH_DisableHook(g_levelRenderTarget);
            MH_RemoveHook(g_levelRenderTarget);
            g_levelRenderTarget = nullptr;
            g_originalLevelRender = nullptr;
            LOG_INFO("LevelRenderer::renderLevel hook removed.");
        }
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/perspective_hook.cpp
// ==============================================================================
namespace {
    using GetPerspectiveFn = int(__fastcall*)(void* options);
    GetPerspectiveFn g_originalGetPerspective = nullptr;
    void* g_perspectiveTarget = nullptr;

    std::atomic<int> g_currentPerspective{ 0 };
    std::atomic<int> g_overridePerspective{ -1 };

    int __fastcall hook_get_perspective(void* options) {
        const int actualValue = g_originalGetPerspective ? g_originalGetPerspective(options) : 0;
        g_currentPerspective.store(actualValue, std::memory_order_relaxed);

        PerspectiveEvent event(actualValue);
        EventBus::instance().publish(event);

        if (event.isOverridden()) {
            return event.overridePerspective;
        }

        const int overrideVal = g_overridePerspective.load(std::memory_order_relaxed);
        if (overrideVal >= 0) {
            return overrideVal;
        }

        return actualValue;
    }
}

namespace minecraft::hooks {
    bool install_perspective_hook(std::uintptr_t funcAddr) {
        if (!funcAddr) {
            LOG_ERROR("Invalid Options::getPerspective address provided!");
            return false;
        }

        LOG_INFO("Hooking Options::getPerspective at address: {:#x}", funcAddr);

        g_perspectiveTarget = reinterpret_cast<void*>(funcAddr);
        if (MH_CreateHook(g_perspectiveTarget, reinterpret_cast<LPVOID>(&hook_get_perspective), reinterpret_cast<void**>(&g_originalGetPerspective)) != MH_OK) {
            LOG_ERROR("Failed to create Options::getPerspective hook!");
            g_perspectiveTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_perspectiveTarget) != MH_OK) {
            LOG_ERROR("Failed to enable Options::getPerspective hook!");
            return false;
        }

        LOG_INFO("Options::getPerspective hook enabled.");
        return true;
    }

    void remove_perspective_hook() {
        g_overridePerspective.store(-1, std::memory_order_relaxed);
        if (g_perspectiveTarget) {
            MH_DisableHook(g_perspectiveTarget);
            MH_RemoveHook(g_perspectiveTarget);
            g_perspectiveTarget = nullptr;
            g_originalGetPerspective = nullptr;
            LOG_INFO("Options::getPerspective hook removed.");
        }
    }

    int get_perspective() {
        const int overrideVal = g_overridePerspective.load(std::memory_order_relaxed);
        if (overrideVal >= 0) {
            return overrideVal;
        }
        return g_currentPerspective.load(std::memory_order_relaxed);
    }

    void set_perspective(int perspective) {
        g_overridePerspective.store(perspective, std::memory_order_relaxed);
    }
}

// ==============================================================================
// SOURCE: src/minecraft/hooks/screen_hook.cpp
// ==============================================================================
namespace {
    using GetScreenNameFn = std::string&(__fastcall*)(ClientInstance* self, std::string& out);
    GetScreenNameFn g_originalGetScreenName = nullptr;
    void* g_screenTarget = nullptr;

    std::string s_currentScreenName;
    std::string s_overrideScreenName;
    std::mutex s_screenMutex;

    static bool call_get_screen_name_raw(ClientInstance* self, GetScreenNameFn fn, std::string* out) {
        __try {
            fn(self, *out);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    std::string& __fastcall hook_client_instance_get_screen_name(ClientInstance* self, std::string& out) {
        if (!g_originalGetScreenName) {
            return out;
        }

        std::string& result = g_originalGetScreenName(self, out);

        std::string prevScreen;
        bool changed = false;

        {
            std::lock_guard<std::mutex> lock(s_screenMutex);
            if (result != s_currentScreenName) {
                prevScreen = s_currentScreenName;
                s_currentScreenName = result;
                changed = true;
            }
        }

        if (changed) {
            ScreenChangeEvent event(self, prevScreen, result);
            EventBus::instance().publish(event);

            if (event.isOverridden()) {
                std::lock_guard<std::mutex> lock(s_screenMutex);
                result = event.overrideScreen;
                s_currentScreenName = event.overrideScreen;
            }
        } else {
            std::lock_guard<std::mutex> lock(s_screenMutex);
            if (!s_overrideScreenName.empty()) {
                result = s_overrideScreenName;
            }
        }

        return result;
    }
}

namespace minecraft::hooks {
    bool install_screen_hook(std::uintptr_t screenNameAddr) {
        if (!screenNameAddr) {
            LOG_ERROR("Invalid ClientInstance::getScreenName address provided!");
            return false;
        }

        LOG_INFO("Hooking ClientInstance::getScreenName at address: {:#x}", screenNameAddr);

        g_screenTarget = reinterpret_cast<void*>(screenNameAddr);
        if (MH_CreateHook(g_screenTarget, reinterpret_cast<LPVOID>(&hook_client_instance_get_screen_name), reinterpret_cast<void**>(&g_originalGetScreenName)) != MH_OK) {
            LOG_ERROR("Failed to create ClientInstance::getScreenName hook!");
            g_screenTarget = nullptr;
            return false;
        }

        if (MH_EnableHook(g_screenTarget) != MH_OK) {
            LOG_ERROR("Failed to enable ClientInstance::getScreenName hook!");
            return false;
        }

        LOG_INFO("ClientInstance getScreenName hook enabled.");
        return true;
    }

    void remove_screen_hook() {
        {
            std::lock_guard<std::mutex> lock(s_screenMutex);
            s_currentScreenName.clear();
            s_overrideScreenName.clear();
        }

        if (g_screenTarget) {
            MH_DisableHook(g_screenTarget);
            MH_RemoveHook(g_screenTarget);
            g_screenTarget = nullptr;
            g_originalGetScreenName = nullptr;
            LOG_INFO("ClientInstance getScreenName hook removed.");
        }
    }

    void update_screen_name(ClientInstance* self) {
        if (!self || !g_screenTarget) return;

        GetScreenNameFn fn = g_originalGetScreenName ? g_originalGetScreenName : reinterpret_cast<GetScreenNameFn>(g_screenTarget);
        if (!fn) return;

        std::string temp;
        if (!call_get_screen_name_raw(self, fn, &temp)) {
            return;
        }

        std::string prevScreen;
        bool changed = false;

        {
            std::lock_guard<std::mutex> lock(s_screenMutex);
            if (!temp.empty() && temp != s_currentScreenName) {
                prevScreen = s_currentScreenName;
                s_currentScreenName = temp;
                changed = true;
            }
        }

        if (changed) {
            ScreenChangeEvent event(self, prevScreen, temp);
            EventBus::instance().publish(event);

            InputManager::instance().clear_key_states();
        }
    }

    std::string get_current_screen_name() {
        std::lock_guard<std::mutex> lock(s_screenMutex);
        return s_currentScreenName;
    }

    bool is_game_ui_open() {
        // If there is no local player, we are in menus
        if (!ClientInstance::getLocalPlayer()) {
            return true;
        }

        const std::string screen = get_current_screen_name();
        if (screen.empty()) {
            return false;
        }
        return screen != "hud_screen";
    }

    void set_screen_override(const std::string& screenName) {
        std::lock_guard<std::mutex> lock(s_screenMutex);
        s_overrideScreenName = screenName;
    }

    void clear_screen_override() {
        std::lock_guard<std::mutex> lock(s_screenMutex);
        s_overrideScreenName.clear();
    }
}

// ==============================================================================
// SOURCE: src/minecraft/sdk/client/client_instance.cpp
// ==============================================================================
bool ClientInstance::world_to_screen(const math::Vec3& worldPos, math::Vec2& screenPos) const {
    if (!this) return false;

    __try {
        LevelRenderer* lr = getLevelRenderer();
        if (!lr) return false;

        LevelRendererPlayer* rp = lr->getLevelRendererPlayer();
        if (!rp) return false;

        GuiData* gui = getGuiData();
        if (!gui) return false;

        const math::Vec3 origin = rp->getOrigin();
        const math::Vec3 rel = worldPos - origin;

        const GLMatrix mat = getGLMatrix();
        const math::Vec3 transformed = mat.transform(rel);

        if (transformed.z > 0.0f) {
            return false;
        }

        const float divisor = -transformed.z;
        if (divisor < 0.001f) {
            return false;
        }

        const math::Vec2 screenSize = gui->getWindowSize();
        const float halfW = screenSize.x * 0.5f;
        const float halfH = screenSize.y * 0.5f;
        const float fovX = rp->getFovX();
        const float fovY = rp->getFovY();

        screenPos.x = halfW + (halfW * transformed.x / divisor * fovX);
        screenPos.y = halfH - (halfH * transformed.y / divisor * fovY);

        return std::isfinite(screenPos.x) && std::isfinite(screenPos.y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void ClientInstance::releaseMouse() {
    minecraft::hooks::release_mouse(this);
}

void ClientInstance::grabMouse() {
    minecraft::hooks::grab_mouse(this);
}

std::string ClientInstance::getScreenName() {
    return minecraft::hooks::get_current_screen_name();
}

bool ClientInstance::isGameUiOpen() {
    return minecraft::hooks::is_game_ui_open();
}

bool ClientInstance::isClientUiOpen() {
    return dx12_hook::is_menu_open();
}

// ==============================================================================
// SOURCE: src/minecraft/sdk/render/draw_3d.cpp
// ==============================================================================
Draw3D::Draw3D() {
    m_screenContext = ScreenContext::s_instance3d;
    if (!m_screenContext) return;

    m_tessellator = m_screenContext->getTessellator();
    m_material = MaterialPtr::getUIColor();

    auto* ci = ClientInstance::get();
    if (!ci) return;
    auto* lr = ci->getLevelRenderer();
    if (!lr) return;
    auto* lrp = lr->getLevelRendererPlayer();
    if (!lrp) return;
    m_origin = lrp->getOrigin();
}

Draw3D::~Draw3D() {
    flush();
}

bool Draw3D::is_valid() const {
    return m_screenContext && m_tessellator && m_material;
}

void Draw3D::push_line(const math::Vec3& from, const math::Vec3& to, const mce::Color& color) {
    math::Vec3 a = from - m_origin;
    math::Vec3 b = to - m_origin;
    m_lineVerts.push_back({ a.x, a.y, a.z, color.r, color.g, color.b, color.a });
    m_lineVerts.push_back({ b.x, b.y, b.z, color.r, color.g, color.b, color.a });
}

void Draw3D::push_quad(const math::Vec3& a, const math::Vec3& b, const math::Vec3& c, const math::Vec3& d, const mce::Color& color) {
    math::Vec3 va = a - m_origin;
    math::Vec3 vb = b - m_origin;
    math::Vec3 vc = c - m_origin;
    math::Vec3 vd = d - m_origin;
    m_quadVerts.push_back({ va.x, va.y, va.z, color.r, color.g, color.b, color.a });
    m_quadVerts.push_back({ vb.x, vb.y, vb.z, color.r, color.g, color.b, color.a });
    m_quadVerts.push_back({ vc.x, vc.y, vc.z, color.r, color.g, color.b, color.a });
    m_quadVerts.push_back({ vd.x, vd.y, vd.z, color.r, color.g, color.b, color.a });
}

void Draw3D::draw_line(const math::Vec3& from, const math::Vec3& to, const mce::Color& color) {
    push_line(from, to, color);
}

void Draw3D::draw_box(const math::Vec3& min, const math::Vec3& max, const mce::Color& color) {
    math::Vec3 corners[8] = {
        { min.x, min.y, min.z }, { max.x, min.y, min.z },
        { max.x, min.y, max.z }, { min.x, min.y, max.z },
        { min.x, max.y, min.z }, { max.x, max.y, min.z },
        { max.x, max.y, max.z }, { min.x, max.y, max.z }
    };

    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (const auto& e : edges) {
        push_line(corners[e[0]], corners[e[1]], color);
    }
}

void Draw3D::fill_box(const math::Vec3& min, const math::Vec3& max, const mce::Color& color) {
    math::Vec3 c[8] = {
        { min.x, min.y, min.z }, { max.x, min.y, min.z },
        { max.x, min.y, max.z }, { min.x, min.y, max.z },
        { min.x, max.y, min.z }, { max.x, max.y, min.z },
        { max.x, max.y, max.z }, { min.x, max.y, max.z }
    };

    static constexpr int faces[6][4] = {
        {0,1,2,3}, {4,5,6,7},
        {0,1,5,4}, {3,2,6,7},
        {0,3,7,4}, {1,2,6,5}
    };

    for (const auto& f : faces) {
        push_quad(c[f[0]], c[f[1]], c[f[2]], c[f[3]], color);
        push_quad(c[f[3]], c[f[2]], c[f[1]], c[f[0]], color);
    }
}

void Draw3D::flush() {
    if (!is_valid()) {
        m_lineVerts.clear();
        m_quadVerts.clear();
        return;
    }

    auto* shaderColor = m_screenContext->getShaderColor();
    if (shaderColor) {
        *shaderColor = mce::Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    if (!m_quadVerts.empty()) {
        m_tessellator->begin(Primitive::Quad, static_cast<int>(m_quadVerts.size()));
        for (const auto& v : m_quadVerts) {
            m_tessellator->color(v.r, v.g, v.b, v.a);
            m_tessellator->vertex(v.x, v.y, v.z);
        }
        MeshHelpers::renderMeshImmediately(m_screenContext, m_tessellator, m_material);
        m_quadVerts.clear();
    }

    if (!m_lineVerts.empty()) {
        m_tessellator->begin(Primitive::LineList, static_cast<int>(m_lineVerts.size()));
        for (const auto& v : m_lineVerts) {
            m_tessellator->color(v.r, v.g, v.b, v.a);
            m_tessellator->vertex(v.x, v.y, v.z);
        }
        MeshHelpers::renderMeshImmediately(m_screenContext, m_tessellator, m_material);
        m_lineVerts.clear();
    }
}

// ==============================================================================
// SOURCE: src/minecraft/sdk/render/material_ptr.cpp
// ==============================================================================
MaterialPtr* MaterialPtr::getUIColor() {
    static MaterialPtr* resolved = nullptr;
    static bool attempted = false;

    if (!attempted) {
        attempted = true;
        std::uintptr_t sig = hook_memory::resolve_signature(BedrockSignatures::render_uiFillColor);
        if (sig) {
            auto addr = hook_memory::resolve_rip_relative(sig, 7, 11);
            if (addr) {
                resolved = reinterpret_cast<MaterialPtr*>(addr);
            }
        }
    }

    return resolved;
}

// ==============================================================================
// SOURCE: src/minecraft/sdk/render/mesh_helpers.cpp
// ==============================================================================
void MeshHelpers::renderMeshImmediately(ScreenContext* ctx, Tessellator* tess, MaterialPtr* material) {
    static std::uintptr_t func = hook_memory::resolve_signature(BedrockSignatures::render_meshImmediate);
    if (!func || !ctx || !tess || !material) return;

    char pad[0x58]{};
    using Fn = void(*)(ScreenContext*, Tessellator*, MaterialPtr*, char*);
    reinterpret_cast<Fn>(func)(ctx, tess, material, pad);
}

// ==============================================================================
// SOURCE: src/minecraft/sdk/render/tessellator.cpp
// ==============================================================================
void Tessellator::vertex(float x, float y, float z) {
    static std::uintptr_t func = hook_memory::resolve_signature(BedrockSignatures::render_tessVertex);
    if (!func) return;
    reinterpret_cast<void(*)(Tessellator*, float, float, float)>(func)(this, x, y, z);
}

void Tessellator::begin(Primitive primitive, int numVertices) {
    static std::uintptr_t func = hook_memory::resolve_signature(BedrockSignatures::render_tessBegin);
    if (!func) return;
    reinterpret_cast<void(*)(Tessellator*, int, Primitive, int, bool)>(func)(
        this, 0, primitive, numVertices, false);
}

void Tessellator::color(float r, float g, float b, float a) {
    static std::uintptr_t func = hook_memory::resolve_signature(BedrockSignatures::render_tessColor);
    if (!func) return;
    mce::Color col{ r, g, b, a };
    reinterpret_cast<void(*)(Tessellator*, const mce::Color&)>(func)(this, col);
}

void Tessellator::color(const mce::Color& col) {
    color(col.r, col.g, col.b, col.a);
}

// ==============================================================================
// SOURCE: src/modules/combat/Killaura.cpp
// ==============================================================================
Killaura::Killaura()
    : Module("Combat", "Killaura", "Automatically attacks nearby entities.") {
    m_range = add_setting<config::FloatSetting>("Range", "Maximum attack reach in blocks.", 3.8f, 1.0f, 8.0f);
    m_delay = add_setting<config::IntSetting>("Attack Delay", "Game ticks between attacks.", 2, 0, 20);
    m_targetPlayers = add_setting<config::BoolSetting>("Target Players", "Attack player entities.", true);
    m_targetMobs = add_setting<config::BoolSetting>("Target Mobs", "Attack non-player mobs.", false);
}

void Killaura::on_enable() {
    m_tickCounter = 0;
    m_tickListener = EventBus::instance().subscribe<ActorTickEvent>([this](ActorTickEvent& event) {
        if (!event.localPlayer) return;

        if (++m_tickCounter < m_delay->value()) return;

        GameMode* gameMode = event.localPlayer->getGameMode();
        if (!gameMode) return;

        const Vec3 localPos = event.localPlayer->getPosition();
        Actor* bestTarget = nullptr;
        float bestDistance = m_range->value();

        for (Actor* target : Actor::get_all(event.localPlayer)) {
            if (target == event.localPlayer) continue;

            const bool isPlayer = target->isPlayer();
            if (isPlayer && !m_targetPlayers->value()) continue;
            if (!isPlayer && !m_targetMobs->value()) continue;

            const float dist = localPos.distance(target->getPosition());
            if (dist < bestDistance) {
                bestDistance = dist;
                bestTarget = target;
            }
        }

        if (bestTarget) {
            gameMode->attack(bestTarget);
            m_tickCounter = 0;
        }
    });
}

void Killaura::on_disable() {
    EventBus::instance().unsubscribe(m_tickListener);
}

// ==============================================================================
// SOURCE: src/modules/module.cpp
// ==============================================================================
Module::Module(std::string category, std::string name, std::string tooltip)
    : m_category(std::move(category)),
    m_name(std::move(name)),
    m_tooltip(std::move(tooltip)) {
}

const std::string& Module::category() const { return m_category; }
const std::string& Module::name() const { return m_name; }
const std::string& Module::tooltip() const { return m_tooltip; }

bool Module::enabled() const { return m_enabled.load(std::memory_order_acquire); }
int Module::keybind() const { return m_keybind.load(std::memory_order_relaxed); }
bool Module::hold_to_enable() const { return m_holdToEnable.load(std::memory_order_relaxed); }

void Module::set_enabled(bool enabled) {
    bool expected = !enabled;
    if (!m_enabled.compare_exchange_strong(expected, enabled, std::memory_order_acq_rel)) {
        return;
    }

    if (enabled) {
        LOG_INFO("Module {} -> {}", color(TextColor::Yellow, m_name), color(TextColor::Green, "ENABLED"));
        on_enable();
    }
    else {
        LOG_INFO("Module {} -> {}", color(TextColor::Yellow, m_name), color(TextColor::Red, "DISABLED"));
        on_disable();
    }
}

void Module::set_keybind(int keybind) {
    m_keybind.store(keybind, std::memory_order_relaxed);
}

void Module::set_hold_to_enable(bool holdToEnable) {
    m_holdToEnable.store(holdToEnable, std::memory_order_relaxed);
}

void Module::toggle() {
    set_enabled(!enabled());
}

void Module::on_settings_render() {
    for (const auto& setting : m_settings) {
        if (setting && setting->is_visible()) {
            setting->render_ui();
        }
    }
}

bool Module::has_settings() const {
    return !m_settings.empty();
}

void Module::on_enable() {}
void Module::on_disable() {}
void Module::on_imgui_render() {}

// ==============================================================================
// SOURCE: src/modules/module_manager.cpp
// ==============================================================================
ModuleManager& ModuleManager::instance() {
    static ModuleManager manager;
    return manager;
}

void ModuleManager::initialize() {
    std::unique_lock lock(m_mutex);
    if (m_initialized) {
        return;
    }

    LOG_INFO("Initializing ModuleManager...");

    m_modules.push_back(std::make_unique<Jetpack>());
    m_modules.push_back(std::make_unique<Fly>());
    m_modules.push_back(std::make_unique<ArrayList>());
    m_modules.push_back(std::make_unique<ESP>());
    m_modules.push_back(std::make_unique<Trail>());
    m_modules.push_back(std::make_unique<Freelook>());
    m_modules.push_back(std::make_unique<Killaura>());

    rebuild_categories();

    EventBus::instance().subscribe<KeyEvent>([this](KeyEvent& event) {
        if (InputManager::instance().is_binding()) return;
        if (ClientInstance::isClientUiOpen() || ClientInstance::isGameUiOpen()) return;

        std::shared_lock lock(m_mutex);
        for (const auto& module : m_modules) {
            if (!module || module->keybind() != event.key) continue;

            if (module->hold_to_enable()) {
                if (event.action == KeyAction::Press)   module->set_enabled(true);
                if (event.action == KeyAction::Release) module->set_enabled(false);
            }
            else {
                if (event.action == KeyAction::Press) {
                    module->toggle();
                }
            }
        }
        });

    LOG_INFO("Registered {} modules across {} categories.", m_modules.size(), m_categories.size());
    m_initialized = true;
}

void ModuleManager::shutdown() {
    std::unique_lock lock(m_mutex);
    LOG_INFO("Shutting down ModuleManager...");
    m_categories.clear();
    m_modules.clear();
    m_initialized = false;
}

void ModuleManager::on_imgui_render() {
    std::shared_lock lock(m_mutex);
    for (const auto& module : m_modules) {
        if (module && module->enabled()) {
            module->on_imgui_render();
        }
    }
}

std::vector<Module*> ModuleManager::modules() const {
    std::shared_lock lock(m_mutex);
    std::vector<Module*> result;
    result.reserve(m_modules.size());
    for (const auto& mod : m_modules) {
        if (mod) result.push_back(mod.get());
    }
    return result;
}

std::vector<std::string> ModuleManager::categories() const {
    std::shared_lock lock(m_mutex);
    return m_categories;
}

std::vector<Module*> ModuleManager::modules_in_category(const std::string& category) const {
    std::shared_lock lock(m_mutex);
    std::vector<Module*> result;

    for (const auto& module : m_modules) {
        if (module && module->category() == category) {
            result.push_back(module.get());
        }
    }

    return result;
}

void ModuleManager::add_module(std::unique_ptr<Module> module) {
    m_modules.push_back(std::move(module));
}

void ModuleManager::rebuild_categories() {
    m_categories.clear();

    constexpr std::array<const char*, 5> defaultCategories{
        "Combat",
        "Movement",
        "Visual",
        "Player",
        "Configs"
    };

    for (const char* category : defaultCategories) {
        m_categories.emplace_back(category);
    }

    for (const auto& module : m_modules) {
        if (!module) {
            continue;
        }

        const std::string& category = module->category();
        if (std::find(m_categories.begin(), m_categories.end(), category) == m_categories.end()) {
            m_categories.push_back(category);
        }
    }
}

// ==============================================================================
// SOURCE: src/modules/movement/Fly.cpp
// ==============================================================================
Fly::Fly() : Module("Movement", "Fly", "Allows creative-style flight.") {
    m_speed = add_setting<config::FloatSetting>("Speed", "Horizontal speed", 1.5f, 0.1f, 10.0f);
    m_verticalSpeed = add_setting<config::FloatSetting>("Vertical Speed", "Ascent/descent speed", 0.8f, 0.1f, 5.0f);
    m_glide = add_setting<config::FloatSetting>("Glide", "Downward pull", 0.0f, 0.0f, 0.5f);
}

void Fly::on_enable() {
    m_tickListener = EventBus::instance().subscribe<ActorTickEvent>([this](ActorTickEvent& event) {
        if (!event.localPlayer) return;

        const bool isAnyUiOpen = ClientInstance::isGameUiOpen() || ClientInstance::isClientUiOpen();

        Vec3 velocity{};

        if (!isAnyUiOpen) {
            Vec3 forward, right;
            math::get_horizontal_vectors(event.localPlayer->getRotation().yaw, forward, right);

            Vec3 move{};
            if (InputManager::instance().is_key_down('W')) move += forward;
            if (InputManager::instance().is_key_down('S')) move -= forward;
            if (InputManager::instance().is_key_down('D')) move += right;
            if (InputManager::instance().is_key_down('A')) move -= right;

            if (move.length_sq() > 0.001f) {
                velocity = move.normalized() * m_speed->value();
            }

            if (InputManager::instance().is_key_down(VK_SPACE)) {
                velocity.y = m_verticalSpeed->value();
            } else if (InputManager::instance().is_key_down(VK_SHIFT)) {
                velocity.y = -m_verticalSpeed->value();
            } else {
                velocity.y = -m_glide->value();
            }
        } else {
            velocity = Vec3{ 0.0f, 0.0f, 0.0f };
        }

        event.localPlayer->setVelocity(velocity);
    });
}

void Fly::on_disable() {
    EventBus::instance().unsubscribe(m_tickListener);
    if (auto* lp = ClientInstance::getLocalPlayer()) {
        lp->setVelocity(Vec3{ 0.0f, 0.0f, 0.0f });
    }
}

// ==============================================================================
// SOURCE: src/modules/movement/Jetpack.cpp
// ==============================================================================
Jetpack::Jetpack() : Module("Movement", "Jetpack", "Flies in direction you look.") {
    m_speed = add_setting<config::FloatSetting>("Speed", "Speed", 1.5f, 0.1f, 10.0f);
}

void Jetpack::on_enable() {
    m_tickListener = EventBus::instance().subscribe<ActorTickEvent>([this](ActorTickEvent& event) {
        if (!event.localPlayer) return;

        Vec3 direction = math::get_direction(event.localPlayer->getRotation());
        event.localPlayer->setVelocity(direction * m_speed->value());
        });
}

void Jetpack::on_disable() {
    EventBus::instance().unsubscribe(m_tickListener);
}

// ==============================================================================
// SOURCE: src/modules/visual/Arraylist.cpp
// ==============================================================================
ArrayList::ArrayList() : Module("Visual", "ArrayList", "Displays enabled modules on screen.") {
    m_position = add_setting<config::EnumSetting>(
        "Position",
        "Screen corner placement",
        0,
        std::vector<std::string>{"Top-Right", "Top-Left", "Bottom-Right", "Bottom-Left"}
    );

    m_showSideBar = add_setting<config::BoolSetting>(
        "Bar Indicator",
        "Draws an accent color indicator bar on the side of entries.",
        true
    );

    m_bgOpacity = add_setting<config::FloatSetting>(
        "Background Opacity",
        "Opacity of entry background boxes.",
        0.65f, 0.0f, 1.0f
    );

    m_hideSelf = add_setting<config::BoolSetting>(
        "Hide Self",
        "Hides the ArrayList module itself from the list.",
        true
    );

    set_enabled(true);
}

void ArrayList::on_imgui_render() {
    std::vector<Element> activeElements;
    const auto modules = ModuleManager::instance().modules();

    for (Module* mod : modules) {
        if (!mod || !mod->enabled()) continue;
        if (m_hideSelf->value() && mod == this) continue;

        Element elem;
        elem.text = mod->name();
        elem.width = ImGui::CalcTextSize(elem.text.c_str()).x;
        activeElements.push_back(std::move(elem));
    }

    if (activeElements.empty()) return;

    const int posMode = m_position->value();
    const bool isRightAligned = (posMode == TopRight || posMode == BottomRight);
    const bool isBottomAligned = (posMode == BottomRight || posMode == BottomLeft);

    std::sort(activeElements.begin(), activeElements.end(), [isRightAligned](const Element& a, const Element& b) {
        return isRightAligned ? (a.width > b.width) : (a.width > b.width);
        });

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

    if (!ImGui::Begin("##ArrayListOverlay", nullptr, windowFlags)) {
        ImGui::End();
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ui::Palette& p = ui::get_palette();

    const float paddingX = 8.0f;
    const float paddingY = 4.0f;
    const float textHeight = ImGui::GetTextLineHeight();
    const float itemHeight = textHeight + (paddingY * 2.0f);
    const float barThickness = 3.0f;
    const float margin = 12.0f;

    float currentY = isBottomAligned
        ? (viewport->WorkSize.y - margin - itemHeight)
        : (viewport->WorkPos.y + margin);

    const ImU32 bgBaseColor = p.bg_dark;
    const float alpha = m_bgOpacity->value();
    const ImU32 bgFinalColor = (bgBaseColor & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 255.0f) << 24);

    for (const auto& elem : activeElements) {
        const float boxWidth = elem.width + (paddingX * 2.0f) + (m_showSideBar->value() ? barThickness : 0.0f);

        float currentX = isRightAligned
            ? (viewport->WorkSize.x - margin - boxWidth)
            : (viewport->WorkPos.x + margin);

        ImVec2 rectMin(currentX, currentY);
        ImVec2 rectMax(currentX + boxWidth, currentY + itemHeight);

        drawList->AddRectFilled(rectMin, rectMax, bgFinalColor, 3.0f);

        float textX = currentX + paddingX;
        if (!isRightAligned && m_showSideBar->value()) {
            textX += barThickness;
        }

        ImVec2 textPos(textX, currentY + paddingY);

        drawList->AddText(textPos, p.text_primary, elem.text.c_str());

        if (m_showSideBar->value()) {
            ImVec2 barMin, barMax;
            if (isRightAligned) {
                barMin = ImVec2(rectMax.x - barThickness, rectMin.y);
                barMax = rectMax;
            }
            else {
                barMin = rectMin;
                barMax = ImVec2(rectMin.x + barThickness, rectMax.y);
            }
            drawList->AddRectFilled(barMin, barMax, p.accent, 2.0f);
        }

        if (isBottomAligned) {
            currentY -= (itemHeight + 2.0f);
        }
        else {
            currentY += (itemHeight + 2.0f);
        }
    }

    ImGui::End();
}

// ==============================================================================
// SOURCE: src/modules/visual/ESP.cpp
// ==============================================================================
ESP::ESP() : Module("Visual", "ESP", "Renders 3D bounding boxes around entities.") {
    m_targetPlayers = add_setting<config::BoolSetting>("Players", "Render ESP on players.", true);
    m_targetMobs = add_setting<config::BoolSetting>("Mobs", "Render ESP on mobs.", false);
    m_color = add_setting<config::ColorSetting>("Color", "Bounding box color.", 1.0f, 0.2f, 0.2f, 1.0f);
    m_thickness = add_setting<config::FloatSetting>("Thickness", "Line thickness.", 1.5f, 0.5f, 5.0f);
    m_fillOpacity = add_setting<config::FloatSetting>("Fill Opacity", "Interior fill opacity.", 0.2f, 0.0f, 1.0f);
}

void ESP::on_enable() {
    m_renderListener = EventBus::instance().subscribe<LevelRenderEvent>([this](LevelRenderEvent& event) {
        render(event);
        });
}

void ESP::on_disable() {
    if (m_renderListener != 0) {
        EventBus::instance().unsubscribe(m_renderListener);
        m_renderListener = 0;
    }
}

void ESP::render(LevelRenderEvent&) {
    LocalPlayer* localPlayer = ClientInstance::getLocalPlayer();
    if (!localPlayer) return;

    Draw3D dc;
    if (!dc.is_valid()) return;

    const ImVec4 col = m_color->as_imvec4();
    const mce::Color boxColor{ col.x, col.y, col.z, col.w };
    const float fillOpacity = m_fillOpacity->value();
    const mce::Color fillColor{ col.x, col.y, col.z, col.w * fillOpacity };
    const bool targetPlayers = m_targetPlayers->value();
    const bool targetMobs = m_targetMobs->value();

    for (Actor* actor : Actor::get_all(localPlayer)) {
        if (!actor || actor == localPlayer) continue;

        const bool isPlayer = actor->isPlayer();
        if (isPlayer && !targetPlayers) continue;
        if (!isPlayer && !targetMobs) continue;

        const math::Vec3 renderPos = actor->getRenderPosition();
        const float yOffset = isPlayer ? -1.62f : 0.0f;
        const math::Vec3 center = { renderPos.x, renderPos.y + yOffset, renderPos.z };

        float halfW = 0.3f;
        float h = 1.8f;
        if (auto* aabb = actor->getAABB()) {
            halfW = aabb->mWidth * 0.5f;
            h = aabb->mHeight;
        }

        const math::Vec3 min = { center.x - halfW, center.y, center.z - halfW };
        const math::Vec3 max = { center.x + halfW, center.y + h, center.z + halfW };

        if (fillOpacity > 0.001f) {
            dc.fill_box(min, max, fillColor);
        }

        dc.draw_box(min, max, boxColor);
    }

    dc.flush();
}

// ==============================================================================
// SOURCE: src/modules/visual/Freelook.cpp
// ==============================================================================
Freelook::Freelook()
    : Module("Visual", "Freelook", "Locks player facing direction while allowing free camera look.") {
    set_hold_to_enable(true);
}

void Freelook::on_enable() {
    m_hasTarget = false;

    if (auto* localPlayer = ClientInstance::getLocalPlayer()) {
        m_targetRotation = localPlayer->getRotation();
        m_hasTarget = true;
    }

    minecraft::hooks::set_perspective(1);

    m_rotListener = EventBus::instance().subscribe<ActorSetRotEvent>([this](ActorSetRotEvent& event) {
        if (!event.localPlayer) return;

        if (!m_hasTarget) {
            m_targetRotation = event.localPlayer->getRotation();
            m_hasTarget = true;
        }

        event.localPlayer->setRotation(m_targetRotation);
        event.localPlayer->setHeadYaw(m_targetRotation.yaw);
    });
}

void Freelook::on_disable() {
    EventBus::instance().unsubscribe(m_rotListener);
    m_rotListener = 0;
    m_hasTarget = false;

    minecraft::hooks::set_perspective(-1);
}

// ==============================================================================
// SOURCE: src/modules/visual/Trail.cpp
// ==============================================================================
Trail::Trail() : Module("Visual", "Trail", "Draws a trailing path behind the player using ImGui.") {
    m_maxAge = add_setting<config::FloatSetting>("Duration", "How long trail points last (seconds).", 5.0f, 1.0f, 30.0f);
    m_spacing = add_setting<config::FloatSetting>("Spacing", "Minimum distance between trail points.", 0.3f, 0.1f, 2.0f);
    m_color = add_setting<config::ColorSetting>("Color", "Trail color.", 0.2f, 0.6f, 1.0f, 1.0f);
    m_thickness = add_setting<config::FloatSetting>("Thickness", "Line thickness.", 2.0f, 0.5f, 5.0f);
}

void Trail::on_enable() {
    m_tickListener = EventBus::instance().subscribe<ActorTickEvent>([this](ActorTickEvent& event) {
        if (!event.localPlayer) return;

        math::Vec3 pos = event.localPlayer->getPosition();
        pos.y -= 1.62f;
        const float maxAge = m_maxAge->value();
        const float minSpacing = m_spacing->value();

        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& point : m_points) {
            point.age += 0.05f;
        }

        std::erase_if(m_points, [maxAge](const TrailPoint& p) { return p.age >= maxAge; });
        bool shouldAdd = m_points.empty();
        if (!shouldAdd) {
            const float dist = pos.distance(m_points.back().position);
            shouldAdd = dist >= minSpacing;
        }

        if (shouldAdd) {
            m_points.push_back({ pos, 0.0f });
        }
    });
}

void Trail::on_disable() {
    if (m_tickListener != 0) {
        EventBus::instance().unsubscribe(m_tickListener);
        m_tickListener = 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_points.clear();
}

void Trail::on_imgui_render() {
    auto* ci = ClientInstance::get();
    if (!ci || !ci->getLocalPlayer()) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    std::vector<TrailPoint> points;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_points.size() < 2) return;
        points = m_points;
    }

    const ImVec4 col = m_color->as_imvec4();
    const float thickness = m_thickness->value();
    const float maxAge = m_maxAge->value();

    for (size_t i = 1; i < points.size(); ++i) {
        math::Vec2 screenA, screenB;
        if (!ci->world_to_screen(points[i - 1].position, screenA)) continue;
        if (!ci->world_to_screen(points[i].position, screenB)) continue;

        // Fade older points
        const float alpha = 1.0f - (points[i].age / maxAge);
        const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(col.x, col.y, col.z, col.w * alpha));

        drawList->AddLine(
            ImVec2(screenA.x, screenA.y),
            ImVec2(screenB.x, screenB.y),
            lineColor,
            thickness
        );
    }
}

// ==============================================================================
// SOURCE: src/render/dx12/dx12_hook.cpp
// ==============================================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    constexpr UINT kFramesInFlight = 3;

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    PresentFn g_originalPresent = nullptr;
    ResizeBuffersFn g_originalResizeBuffers = nullptr;
    ExecuteCommandListsFn g_originalExecuteCommandLists = nullptr;

    HWND g_window = nullptr;
    WNDPROC g_originalWndProc = nullptr;

    ID3D12Device* g_device = nullptr;
    ID3D12CommandQueue* g_commandQueue = nullptr;
    ID3D12DescriptorHeap* g_rtvHeap = nullptr;
    ID3D12DescriptorHeap* g_srvHeap = nullptr;
    ID3D12GraphicsCommandList* g_commandList = nullptr;
    ID3D12Fence* g_fence = nullptr;
    HANDLE g_fenceEvent = nullptr;

    struct FrameContext {
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12Resource* renderTarget = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
        UINT64 fenceValue = 0;
    };

    std::vector<FrameContext> g_frames;
    UINT g_bufferCount = 0;
    UINT g_rtvDescriptorSize = 0;
    DXGI_FORMAT g_renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT64 g_nextFenceValue = 1;

    std::mutex g_mutex;
    std::atomic_bool g_initialized = false;
    std::atomic_bool g_shuttingDown = false;
    std::atomic_bool g_menuOpen = false;
    std::atomic_int g_skipFrames = 0;
    std::atomic<unsigned long long> g_resumeRenderAtTick = 0;

    void request_render_pause(unsigned int frames, unsigned int milliseconds);

    int mouse_message_key(UINT msg, WPARAM wparam, bool& down) {
        switch (msg) {
        case WM_LBUTTONDOWN:
            down = true;
            return VK_LBUTTON;
        case WM_LBUTTONUP:
            down = false;
            return VK_LBUTTON;
        case WM_RBUTTONDOWN:
            down = true;
            return VK_RBUTTON;
        case WM_RBUTTONUP:
            down = false;
            return VK_RBUTTON;
        case WM_MBUTTONDOWN:
            down = true;
            return VK_MBUTTON;
        case WM_MBUTTONUP:
            down = false;
            return VK_MBUTTON;
        case WM_XBUTTONDOWN:
            down = true;
            return HIWORD(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        case WM_XBUTTONUP:
            down = false;
            return HIWORD(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        default:
            return 0;
        }
    }

    void release_gameplay_input(HWND hwnd) {
        if (!g_originalWndProc || !hwnd) return;

        InputManager::instance().clear_key_states();

        auto release_key = [hwnd](WPARAM key) {
            LPARAM keyUpLParam = 0xC0000001;
            const UINT scanCode = MapVirtualKey(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
            keyUpLParam |= static_cast<LPARAM>(scanCode) << 16;
            CallWindowProc(g_originalWndProc, hwnd, WM_KEYUP, key, keyUpLParam);
        };

        release_key('W');
        release_key('A');
        release_key('S');
        release_key('D');
        release_key(VK_SPACE);
        release_key(VK_SHIFT);
        release_key(VK_CONTROL);

        CallWindowProc(g_originalWndProc, hwnd, WM_LBUTTONUP, 0, 0);
        CallWindowProc(g_originalWndProc, hwnd, WM_RBUTTONUP, 0, 0);
        CallWindowProc(g_originalWndProc, hwnd, WM_MBUTTONUP, 0, 0);
    }

    void force_non_client_mouse(HWND hwnd) {
        if (!hwnd || !g_originalWndProc) return;

        ReleaseCapture();
        ClipCursor(nullptr);

        POINT pt{};
        GetCursorPos(&pt);
        const LPARAM screenPos = MAKELPARAM(static_cast<short>(pt.x), static_cast<short>(pt.y));

        CallWindowProc(g_originalWndProc, hwnd, WM_NCMOUSEMOVE, HTCAPTION, screenPos);
        CallWindowProc(g_originalWndProc, hwnd, WM_MOUSELEAVE, 0, 0);
    }

    LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        const bool keyDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        const bool keyUp = (msg == WM_KEYUP || msg == WM_SYSKEYUP);

        if ((keyDown && wparam == VK_F11) || msg == WM_SIZE || msg == WM_DISPLAYCHANGE) {
            request_render_pause(180, 3000);
        }

        if (keyUp && wparam == VK_INSERT) {
            const bool opening = !g_menuOpen.load();
            g_menuOpen.store(opening);

            if (opening) {
                release_gameplay_input(hwnd);
                force_non_client_mouse(hwnd);
            }
            return 0;
        }

        if (keyDown && wparam == VK_ESCAPE && g_menuOpen.load()) {
            if (InputManager::instance().is_binding()) {
                InputManager::instance().cancel_binding();
                return 0;
            }
            g_menuOpen.store(false);
            return 0;
        }

        const bool isInputMessage = (
            msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
            msg == WM_KEYUP || msg == WM_SYSKEYUP ||
            msg == WM_CHAR ||
            msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK ||
            msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP || msg == WM_MBUTTONDBLCLK ||
            msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK ||
            msg == WM_MOUSEWHEEL || msg == WM_MOUSEMOVE
        );

        if (g_menuOpen.load()) {
            ReleaseCapture();
            ClipCursor(nullptr);

            if (InputManager::instance().is_binding()) {
                InputManager::instance().handle_win32_message(msg, wparam, lparam);
            }

            if (ImGui::GetCurrentContext()) {
                ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
            }

            if (isInputMessage) {
                return 0;
            }
        }

        const bool consumedByInput = InputManager::instance().handle_win32_message(msg, wparam, lparam);
        if (consumedByInput) {
            return 0;
        }

        return CallWindowProc(g_originalWndProc, hwnd, msg, wparam, lparam);
    }

    void request_render_pause(unsigned int frames, unsigned int milliseconds) {
        g_skipFrames.store(static_cast<int>(frames));
        g_resumeRenderAtTick.store(GetTickCount64() + milliseconds);
    }

    void wait_for_fence(UINT64 fenceValue) {
        if (!g_fence || !g_fenceEvent || fenceValue == 0) {
            return;
        }

        if (g_fence->GetCompletedValue() >= fenceValue) {
            return;
        }

        if (SUCCEEDED(g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent))) {
            WaitForSingleObject(g_fenceEvent, INFINITE);
        }
    }

    void wait_for_overlay_work() {
        for (FrameContext& frame : g_frames) {
            wait_for_fence(frame.fenceValue);
            frame.fenceValue = 0;
        }
    }

    void apply_overlay_style() {
        ui::apply_theme();
    }

    void release_frame_resources() {
        wait_for_overlay_work();

        for (FrameContext& frame : g_frames) {
            if (frame.renderTarget) {
                frame.renderTarget->Release();
                frame.renderTarget = nullptr;
            }

            if (frame.allocator) {
                frame.allocator->Release();
                frame.allocator = nullptr;
            }
        }

        g_frames.clear();
        g_bufferCount = 0;

        if (g_rtvHeap) {
            g_rtvHeap->Release();
            g_rtvHeap = nullptr;
        }
    }

    void cleanup_imgui() {
        if (!g_initialized.exchange(false)) {
            return;
        }

        LOG_INFO("Cleaning up ImGui & DX12 render resources...");

        wait_for_overlay_work();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (g_originalWndProc && g_window) {
            SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
            g_originalWndProc = nullptr;
        }

        release_frame_resources();

        if (g_commandList) {
            g_commandList->Release();
            g_commandList = nullptr;
        }

        if (g_srvHeap) {
            g_srvHeap->Release();
            g_srvHeap = nullptr;
        }

        if (g_fence) {
            g_fence->Release();
            g_fence = nullptr;
        }

        if (g_fenceEvent) {
            CloseHandle(g_fenceEvent);
            g_fenceEvent = nullptr;
        }

        if (g_device) {
            g_device->Release();
            g_device = nullptr;
        }

        g_window = nullptr;
        g_commandQueue = nullptr;
        LOG_INFO("ImGui & DX12 render resources cleaned up successfully.");
    }

    bool create_render_targets(IDXGISwapChain3* swapChain) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapChain->GetDesc(&desc))) {
            return false;
        }

        g_bufferCount = desc.BufferCount;
        g_frames.resize(g_bufferCount);

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = g_bufferCount;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvHeap)))) {
            return false;
        }

        g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < g_bufferCount; ++i) {
            FrameContext& frame = g_frames[i];

            if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)))) {
                return false;
            }

            if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&frame.renderTarget)))) {
                return false;
            }

            frame.rtvHandle = rtvHandle;
            g_device->CreateRenderTargetView(frame.renderTarget, nullptr, frame.rtvHandle);
            rtvHandle.ptr += g_rtvDescriptorSize;
        }

        return true;
    }

    bool initialize_imgui(IDXGISwapChain3* swapChain) {
        if (!g_commandQueue) {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapChain->GetDesc(&desc)) || !desc.OutputWindow) {
            return false;
        }

        g_window = desc.OutputWindow;
        g_renderTargetFormat = desc.BufferDesc.Format == DXGI_FORMAT_UNKNOWN
            ? DXGI_FORMAT_R8G8B8A8_UNORM
            : desc.BufferDesc.Format;

        LOG_INFO("Initializing ImGui DX12 overlay for window handle: {:p}...", static_cast<void*>(g_window));

        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&g_device)))) {
            LOG_ERROR("Failed to acquire D3D12 device from swapchain!");
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.NumDescriptors = 1;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(g_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_srvHeap)))) {
            LOG_ERROR("Failed to create SRV descriptor heap!");
            return false;
        }

        if (!create_render_targets(swapChain)) {
            LOG_ERROR("Failed to create render target views!");
            return false;
        }

        if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
            LOG_ERROR("Failed to create D3D12 synchronization fence!");
            return false;
        }

        g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!g_fenceEvent) {
            LOG_ERROR("Failed to create fence event handle!");
            return false;
        }

        if (FAILED(g_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            g_frames[0].allocator,
            nullptr,
            IID_PPV_ARGS(&g_commandList)))) {
            LOG_ERROR("Failed to create graphics command list!");
            return false;
        }
        g_commandList->Close();

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        static const char* fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        if (std::filesystem::exists(fontPath)) {
            io.Fonts->AddFontFromFileTTF(fontPath, 22.0f);
        }

        apply_overlay_style();
        ui_settings::reset_ui_scale_cache();
        ImGui_ImplWin32_Init(g_window);
        ImGui_ImplDX12_Init(
            g_device,
            static_cast<int>(kFramesInFlight),
            g_renderTargetFormat,
            g_srvHeap,
            g_srvHeap->GetCPUDescriptorHandleForHeapStart(),
            g_srvHeap->GetGPUDescriptorHandleForHeapStart());

        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wnd_proc)));

        g_initialized.store(true);
        LOG_INFO("ImGui DX12 overlay initialized successfully.");
        return true;
    }

    void render_imgui(IDXGISwapChain3* swapChain) {
        const UINT index = swapChain->GetCurrentBackBufferIndex();
        if (index >= g_frames.size()) {
            return;
        }

        FrameContext& frame = g_frames[index];
        wait_for_fence(frame.fenceValue);
        frame.fenceValue = 0;

        frame.allocator->Reset();

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = frame.renderTarget;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        g_commandList->Reset(frame.allocator, nullptr);
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);
        g_commandList->SetDescriptorHeaps(1, &g_srvHeap);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ui_settings::apply_ui_scale();
        ImGui::NewFrame();

        ModuleManager::instance().on_imgui_render();

        if (g_menuOpen.load()) {
            clickgui::render();
        }

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->Close();

        ID3D12CommandList* lists[] = { g_commandList };
        g_commandQueue->ExecuteCommandLists(1, lists);

        const UINT64 fenceValue = g_nextFenceValue++;
        if (SUCCEEDED(g_commandQueue->Signal(g_fence, fenceValue))) {
            frame.fenceValue = fenceValue;
        }
    }

    void __stdcall hook_execute_command_lists(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
        if (!g_commandQueue && queue) {
            g_commandQueue = queue;
        }

        g_originalExecuteCommandLists(queue, count, lists);
    }

    HRESULT __stdcall hook_present(IDXGISwapChain3* swapChain, UINT syncInterval, UINT flags) {
        if (!g_shuttingDown.load()) {
            std::lock_guard lock(g_mutex);

            const int skipFrames = g_skipFrames.load();
            const unsigned long long now = GetTickCount64();
            const bool inCooldown = now < g_resumeRenderAtTick.load();

            if (skipFrames > 0 || inCooldown) {
                if (skipFrames > 0) {
                    g_skipFrames.store(skipFrames - 1);
                }

                if (g_initialized.load()) {
                    cleanup_imgui();
                }

                return g_originalPresent(swapChain, syncInterval, flags);
            }

            if (!g_initialized.load()) {
                initialize_imgui(swapChain);
            }

            if (g_initialized.load()) {
                render_imgui(swapChain);
            }
        }

        return g_originalPresent(swapChain, syncInterval, flags);
    }

    HRESULT __stdcall hook_resize_buffers(
        IDXGISwapChain3* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags) {
        std::lock_guard lock(g_mutex);

        if (g_initialized.load()) {
            cleanup_imgui();
        }

        return g_originalResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
    }
}

namespace dx12_hook {
    bool start(HMODULE) {
        g_shuttingDown.store(false);

        LOG_INFO("Initializing Kiero D3D12 hook...");

        if (kiero::init(kiero::RenderType::D3D12) != kiero::Status::Success) {
            LOG_ERROR("Failed to initialize Kiero D3D12!");
            return false;
        }

        constexpr uint16_t executeCommandListsIndex = 54;
        constexpr uint16_t presentIndex = 140;
        constexpr uint16_t resizeBuffersIndex = 145;

        if (kiero::bind(executeCommandListsIndex, reinterpret_cast<void**>(&g_originalExecuteCommandLists), hook_execute_command_lists) != kiero::Status::Success) {
            LOG_ERROR("Failed to bind ExecuteCommandLists hook!");
            return false;
        }

        if (kiero::bind(presentIndex, reinterpret_cast<void**>(&g_originalPresent), hook_present) != kiero::Status::Success) {
            LOG_ERROR("Failed to bind Present hook!");
            return false;
        }

        if (kiero::bind(resizeBuffersIndex, reinterpret_cast<void**>(&g_originalResizeBuffers), hook_resize_buffers) != kiero::Status::Success) {
            LOG_ERROR("Failed to bind ResizeBuffers hook!");
            return false;
        }

        LOG_INFO("DX12 hooks bound successfully.");
        return true;
    }

    void stop() {
        LOG_INFO("Stopping DX12 hook...");
        g_shuttingDown.store(true);

        {
            std::lock_guard lock(g_mutex);
            cleanup_imgui();
        }

        kiero::shutdown();
        LOG_INFO("DX12 hook stopped.");
    }

    bool is_menu_open() {
        return g_menuOpen.load();
    }

    void set_menu_open(bool open) {
        g_menuOpen.store(open);
    }
}

// ==============================================================================
// SOURCE: src/ui/clickgui.cpp
// ==============================================================================
namespace {

    constexpr float CAT_WIDTH = 215.0f;
    constexpr float CAT_HEIGHT = 32.0f;
    constexpr float MOD_HEIGHT = 30.0f;
    constexpr float CAT_GAP = 18.0f;
    constexpr float CORNER_RADIUS = 8.0f;

    const ImColor kDarkHeader(18, 20, 24, 255);
    const ImColor kModOffColor(24, 26, 32, 255);
    const ImColor kModHoverColor(32, 35, 44, 255);
    const ImColor kSettingBgColor(18, 20, 24, 255);
    const ImColor kEnumSubBg(14, 16, 20, 255);
    const ImColor kTextWhite(255, 255, 255, 255);
    const ImColor kTextMuted(160, 170, 180, 255);
    const ImColor kTextDisabled(110, 120, 130, 255);

    const ImColor kEmeraldAccent(16, 185, 129, 255);  
    const ImColor kEmeraldDark(5, 150, 105, 255);      
    const ImColor kEmeraldLight(52, 211, 153, 255);    
    const ImColor kEmeraldGlow(16, 185, 129, 65);

    struct CategoryState {
        float x = 0.0f;
        float y = 0.0f;
        bool isDragging = false;
        bool isExtended = true;
        float extendAnim = 1.0f;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
        float scrollEase = 0.0f;
        float yOffset = 0.0f;
    };

    static std::map<std::string, CategoryState> g_catStates;
    static bool g_initPositions = false;

    static config::Setting* g_activeDraggedSlider = nullptr;

    static config::ColorSetting* g_activeColorSetting = nullptr;
    static bool g_showColorPicker = false;

    static float g_configPanelHeight = 220.0f;

    static std::map<Module*, float> g_modToggleScale;
    static std::map<Module*, float> g_modSettingsAnim;
    static std::map<Module*, float> g_modHoldAnim;
    static std::map<Module*, bool>  g_modShowSettings;

    static std::map<config::Setting*, float> g_settingBoolScale;
    static std::map<config::Setting*, float> g_settingSliderEase;
    static std::map<config::Setting*, float> g_settingEnumSlide;
    static std::map<config::Setting*, bool>  g_settingEnumOpen;

    inline float Animate(float target, float current, float speed) {
        float dt = ImGui::GetIO().DeltaTime;
        return current + (target - current) * std::clamp(dt * speed, 0.0f, 1.0f);
    }

    inline bool IsMouseOver(const ImVec4& r) {
        ImVec2 m = ImGui::GetIO().MousePos;
        return m.x >= r.x && m.y >= r.y && m.x < r.z && m.y < r.w;
    }

    void FillRectangle(const ImVec4& pos, const ImColor& color, float alpha, float radius, ImDrawFlags flags, ImDrawList* dl) {
        if (pos.z <= pos.x || pos.w <= pos.y) return;
        dl->AddRectFilled(
            ImVec2(pos.x, pos.y), ImVec2(pos.z, pos.w),
            ImColor(color.Value.x, color.Value.y, color.Value.z, color.Value.w * alpha),
            radius, flags
        );
    }

    void FillRoundedGradientRectangle(const ImVec4& pos, const ImColor& col1, const ImColor& col2, float radius, float alpha, ImDrawFlags flags, ImDrawList* dl) {
        if (pos.z <= pos.x || pos.w <= pos.y) return;
        ImVec2 topLeft(pos.x, pos.y);
        ImVec2 bottomRight(pos.z, pos.w);

        ImU32 uCol1 = ImColor(col1.Value.x, col1.Value.y, col1.Value.z, alpha);
        ImU32 uCol2 = ImColor(col2.Value.x, col2.Value.y, col2.Value.z, alpha);

        int start = dl->VtxBuffer.Size;
        dl->AddRectFilled(topLeft, bottomRight, uCol1, radius, flags);
        int end = dl->VtxBuffer.Size;

        ImGui::ShadeVertsLinearColorGradientKeepAlpha(
            dl, start, end, topLeft, bottomRight,
            uCol1, uCol2
        );
    }

    void FillShadowCircle(const ImVec2& pos, float radius, const ImColor& color, float alpha, float thickness, ImDrawList* dl) {
        dl->AddShadowCircle(
            pos, radius,
            ImColor(color.Value.x, color.Value.y, color.Value.z, color.Value.w * alpha),
            thickness, ImVec2(0.0f, 0.0f), 0, 16
        );
    }

    void DrawCheckMark(const ImVec2& pos, float size, const ImColor& color, float alpha, float thickness, ImDrawList* dl) {
        ImVec2 end1(pos.x + (3.0f * size), pos.y + (3.0f * size));
        ImVec2 end2(pos.x + (7.0f * size), pos.y - (3.0f * size));
        ImU32 col = ImColor(color.Value.x, color.Value.y, color.Value.z, color.Value.w * alpha);
        dl->AddLine(pos, end1, col, thickness);
        dl->AddLine(end1, end2, col, thickness);
    }

    void render_config_panel(float catX, float startY, float catWidth, float visibleHeight, float yOffset) {
        if (visibleHeight < 5.0f) return;

        ImGui::SetNextWindowPos(ImVec2(catX, startY));
        ImGui::SetNextWindowSize(ImVec2(catWidth, visibleHeight));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoCollapse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        if (ImGui::Begin("##ConfigsCategoryContent", nullptr, flags)) {
            ImGui::SetScrollY(yOffset);

            ImGui::PushItemWidth(catWidth - 16.0f);

            static char configNameBuf[64] = "default";
            ui::InputText("##ConfigProfileName", configNameBuf, sizeof(configNameBuf));

            ImGui::Spacing();
            const float btnW = (ImGui::GetContentRegionAvail().x - 6.0f) * 0.5f;
            if (ui::PrimaryButton("Save", ImVec2(btnW, 26.0f))) {
                if (configNameBuf[0] != '\0') {
                    ConfigManager::instance().save_config(configNameBuf);
                }
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ui::SecondaryButton("Load", ImVec2(btnW, 26.0f))) {
                if (configNameBuf[0] != '\0') {
                    ConfigManager::instance().load_config(configNameBuf);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(kTextMuted.Value, "Saved Profiles:");
            ImGui::Spacing();

            auto configs = ConfigManager::instance().get_available_configs();
            if (configs.empty()) {
                ImGui::TextDisabled("No profiles found.");
            } else {
                for (const auto& cfg : configs) {
                    ImGui::PushID(cfg.c_str());
                    const bool isSel = (cfg == configNameBuf);
                    if (ImGui::Selectable(cfg.c_str(), isSel)) {
                        strncpy_s(configNameBuf, cfg.c_str(), sizeof(configNameBuf) - 1);
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        ConfigManager::instance().load_config(cfg);
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Load Profile")) {
                            ConfigManager::instance().load_config(cfg);
                            strncpy_s(configNameBuf, cfg.c_str(), sizeof(configNameBuf) - 1);
                        }
                        if (ImGui::MenuItem("Overwrite Profile")) {
                            ConfigManager::instance().save_config(cfg);
                        }
                        if (ImGui::MenuItem("Delete Profile")) {
                            ConfigManager::instance().delete_config(cfg);
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }

            float measuredH = std::ceil(ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y + 6.0f);
            if (measuredH > 50.0f) {
                g_configPanelHeight = measuredH;
            }

            ImGui::PopItemWidth();
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

} // anonymous namespace

namespace clickgui {

    void render() {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screen = io.DisplaySize;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        ModuleManager& moduleManager = ModuleManager::instance();
        const auto& categories = moduleManager.categories();

        {
            float glowTop = screen.y - (screen.y / 3.0f);
            ImColor glowCol(16, 185, 129, 36);
            dl->AddRectFilledMultiColor(
                ImVec2(0.0f, glowTop), screen,
                ImColor(glowCol.Value.x, glowCol.Value.y, glowCol.Value.z, 0.0f),
                ImColor(glowCol.Value.x, glowCol.Value.y, glowCol.Value.z, 0.0f),
                glowCol, glowCol
            );
        }

        std::vector<std::string> visibleCategories;
        for (const auto& cat : categories) {
            auto mods = moduleManager.modules_in_category(cat);
            if (!mods.empty() || cat == "Configs") {
                visibleCategories.push_back(cat);
            }
        }

        if (!g_initPositions || g_catStates.size() != visibleCategories.size()) {
            float totalWidth = static_cast<float>(visibleCategories.size()) * (CAT_WIDTH + CAT_GAP) - CAT_GAP;
            float startX = (screen.x - totalWidth) * 0.5f;
            float startY = 60.0f;

            for (size_t i = 0; i < visibleCategories.size(); ++i) {
                auto& st = g_catStates[visibleCategories[i]];
                if (st.x == 0.0f && st.y == 0.0f) {
                    st.x = std::round((startX + static_cast<float>(i) * (CAT_WIDTH + CAT_GAP)) / 2.0f) * 2.0f;
                    st.y = std::round(startY / 2.0f) * 2.0f;
                }
            }
            g_initPositions = true;
        }

        std::string hoveredTooltip = "";

        for (size_t catIdx = 0; catIdx < visibleCategories.size(); ++catIdx) {
            const std::string& category = visibleCategories[catIdx];
            auto& catState = g_catStates[category];

            std::vector<Module*> modules = moduleManager.modules_in_category(category);

            float totalContentHeight = 0.0f;
            if (category == "Configs") {
                totalContentHeight = g_configPanelHeight;
            } else {
                for (auto* mod : modules) {
                    if (!mod) continue;
                    totalContentHeight += MOD_HEIGHT;

                    bool targetSettingsOpen = g_modShowSettings[mod];
                    g_modSettingsAnim[mod] = Animate(targetSettingsOpen ? 1.0f : 0.0f, g_modSettingsAnim[mod], 14.0f);
                    float sAnim = g_modSettingsAnim[mod];

                    if (sAnim > 0.005f) {
                        totalContentHeight += MOD_HEIGHT * sAnim;

                        for (const auto& s : mod->settings()) {
                            if (!s || !s->is_visible()) continue;
                            totalContentHeight += (s->type() == config::SettingType::FLOAT || s->type() == config::SettingType::INT ? (MOD_HEIGHT + 2.0f) : MOD_HEIGHT) * sAnim;

                            if (s->type() == config::SettingType::ENUM) {
                                bool targetEnumOpen = g_settingEnumOpen[s.get()];
                                g_settingEnumSlide[s.get()] = Animate(targetEnumOpen ? 1.0f : 0.0f, g_settingEnumSlide[s.get()], 14.0f);
                                float enumAnim = g_settingEnumSlide[s.get()];
                                auto* es = static_cast<config::EnumSetting*>(s.get());
                                if (enumAnim > 0.005f) {
                                    totalContentHeight += static_cast<float>(es->options().size()) * MOD_HEIGHT * enumAnim * sAnim;
                                }
                            }
                        }
                    }
                }
            }

            float catX = catState.x;
            float catY = catState.y;
            ImVec4 catHeaderRect(catX, catY, catX + CAT_WIDTH, catY + CAT_HEIGHT);

            if (catState.isDragging) {
                if (ImGui::IsMouseDown(0)) {
                    catState.x = std::clamp(io.MousePos.x - catState.dragOffsetX, 0.0f, screen.x - CAT_WIDTH);
                    catState.y = std::clamp(io.MousePos.y - catState.dragOffsetY, 0.0f, screen.y - CAT_HEIGHT);
                    catState.x = std::round(catState.x / 2.0f) * 2.0f;
                    catState.y = std::round(catState.y / 2.0f) * 2.0f;
                } else {
                    catState.isDragging = false;
                }
            } else if (IsMouseOver(catHeaderRect) && ImGui::IsMouseClicked(0) && !io.WantCaptureMouse) {
                catState.isDragging = true;
                catState.dragOffsetX = io.MousePos.x - catState.x;
                catState.dragOffsetY = io.MousePos.y - catState.y;
            }

            if (IsMouseOver(catHeaderRect) && ImGui::IsMouseClicked(1) && !io.WantCaptureMouse) {
                catState.isExtended = !catState.isExtended;
            }

            catState.extendAnim = Animate(catState.isExtended ? 1.0f : 0.0f, catState.extendAnim, 14.0f);

            const float maxVisibleH = (std::max)(60.0f, screen.y - catY - 70.0f);
            const float maxScroll = (std::max)(0.0f, totalContentHeight - maxVisibleH);

            catState.scrollEase = std::clamp(catState.scrollEase, 0.0f, maxScroll);
            if (catState.extendAnim <= 0.005f || maxScroll <= 0.0f) {
                catState.scrollEase = 0.0f;
            }
            catState.yOffset = Animate(catState.scrollEase, catState.yOffset, 14.0f);
            if (catState.extendAnim <= 0.005f) {
                catState.yOffset = 0.0f;
            }

            const float targetContentH = (totalContentHeight > maxVisibleH && maxVisibleH > 0.0f) ? maxVisibleH : totalContentHeight;
            const float visibleHeight = targetContentH * catState.extendAnim;
            const float catBottomY = catY + CAT_HEIGHT + visibleHeight;
            ImVec4 catWindowRect(catX, catY, catX + CAT_WIDTH, catBottomY);

            if (IsMouseOver(catWindowRect) && catState.extendAnim > 0.01f) {
                if (io.MouseWheel != 0.0f) {
                    catState.scrollEase -= io.MouseWheel * MOD_HEIGHT * 2.0f;
                    catState.scrollEase = std::clamp(catState.scrollEase, 0.0f, maxScroll);
                }
            }

            FillRectangle(
                catHeaderRect, kDarkHeader, 1.0f, CORNER_RADIUS,
                (catState.extendAnim > 0.005f) ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll,
                dl
            );

            ImVec2 dotPos(catX + 14.0f, catY + CAT_HEIGHT * 0.5f);
            dl->AddCircleFilled(dotPos, 5.0f, static_cast<ImU32>(kEmeraldGlow));
            dl->AddCircleFilled(dotPos, 2.5f, static_cast<ImU32>(kEmeraldAccent));

            ImVec2 titleSize = ImGui::CalcTextSize(category.c_str());
            float titleX = catX + (CAT_WIDTH - titleSize.x) * 0.5f;
            float titleY = catY + (CAT_HEIGHT - titleSize.y) * 0.5f;
            dl->AddText(ImVec2(titleX, titleY), static_cast<ImU32>(kTextWhite), category.c_str());

            const char* collapseIcon = (catState.isExtended ? "-" : "+");
            ImVec2 collSz = ImGui::CalcTextSize(collapseIcon);
            dl->AddText(ImVec2(catX + CAT_WIDTH - collSz.x - 12.0f, catY + (CAT_HEIGHT - collSz.y) * 0.5f), static_cast<ImU32>(kTextMuted), collapseIcon);

            dl->AddLine(
                ImVec2(catX, catY + CAT_HEIGHT),
                ImVec2(catX + CAT_WIDTH, catY + CAT_HEIGHT),
                static_cast<ImU32>(kEmeraldGlow), 1.0f
            );

            if (catState.extendAnim <= 0.005f) {
                continue;
            }

            FillRectangle(
                ImVec4(catX, catY + CAT_HEIGHT, catX + CAT_WIDTH, catBottomY),
                kModOffColor, 1.0f, CORNER_RADIUS,
                ImDrawFlags_RoundCornersBottom, dl
            );

            ImVec4 catClipRect(catX - 2.0f, catY + CAT_HEIGHT, catX + CAT_WIDTH + 2.0f, catBottomY);
            dl->PushClipRect(ImVec2(catClipRect.x, catClipRect.y), ImVec2(catClipRect.z, catClipRect.w), true);

            float currentY = catY + CAT_HEIGHT - catState.yOffset;

            if (category == "Configs") {
                dl->PopClipRect();
                render_config_panel(catX, catY + CAT_HEIGHT, CAT_WIDTH, visibleHeight, catState.yOffset);
                continue;
            }

            for (size_t mIdx = 0; mIdx < modules.size(); ++mIdx) {
                auto* mod = modules[mIdx];
                if (!mod) continue;

                bool isLastModule = (mIdx == modules.size() - 1);
                float targetToggle = mod->enabled() ? 1.0f : 0.0f;
                g_modToggleScale[mod] = Animate(targetToggle, g_modToggleScale[mod], 14.0f);
                float toggleScale = g_modToggleScale[mod];

                bool targetSettingsOpen = g_modShowSettings[mod];
                float settingsAnim = g_modSettingsAnim[mod];

                ImVec4 modRect(catX, currentY, catX + CAT_WIDTH, currentY + MOD_HEIGHT);
                float bottomRadius = (isLastModule && settingsAnim < 0.05f) ? CORNER_RADIUS : 0.0f;
                if (modRect.w >= catBottomY - 1.0f) {
                    modRect.w = catBottomY;
                    bottomRadius = CORNER_RADIUS;
                }

                const bool isHovered = IsMouseOver(modRect) && io.MousePos.y <= catBottomY && !io.WantCaptureMouse;

                if (modRect.w > modRect.y) {
                    ImColor bgCol = isHovered ? kModHoverColor : kModOffColor;
                    FillRectangle(modRect, bgCol, 1.0f, bottomRadius, ImDrawFlags_RoundCornersBottom, dl);

                    if (toggleScale > 0.01f) {
                        FillRoundedGradientRectangle(
                            modRect, kEmeraldAccent, kEmeraldDark,
                            bottomRadius, toggleScale,
                            ImDrawFlags_RoundCornersBottom, dl
                        );
                    }
                }

                const float textH = ImGui::GetTextLineHeight();
                const float textY = currentY + (MOD_HEIGHT - textH) * 0.5f;
                ImColor textCol = (toggleScale > 0.5f) ? kTextWhite : (isHovered ? kTextWhite : kTextMuted);
                dl->AddText(ImVec2(catX + 12.0f, textY), static_cast<ImU32>(textCol), mod->name().c_str());

                const bool isBindingThis = (InputManager::instance().binding_module() == mod);
                if (isBindingThis) {
                    std::string bindText = "[ ... ]";
                    ImVec2 bSz = ImGui::CalcTextSize(bindText.c_str());
                    dl->AddText(ImVec2(catX + CAT_WIDTH - bSz.x - 24.0f, textY), static_cast<ImU32>(kEmeraldLight), bindText.c_str());
                } else if (mod->keybind() != 0) {
                    std::string keyStr = "[" + std::string(input::get_key_name(mod->keybind())) + "]";
                    ImVec2 bSz = ImGui::CalcTextSize(keyStr.c_str());
                    dl->AddText(ImVec2(catX + CAT_WIDTH - bSz.x - 24.0f, textY), static_cast<ImU32>(kTextDisabled), keyStr.c_str());
                }

                const char* chevron = targetSettingsOpen ? "v" : ">";
                dl->AddText(ImVec2(catX + CAT_WIDTH - 14.0f, textY), static_cast<ImU32>(kTextDisabled), chevron);

                if (isHovered) {
                    if (!mod->tooltip().empty()) {
                        hoveredTooltip = mod->tooltip();
                    }

                    if (ImGui::IsMouseClicked(0)) {
                        mod->toggle();
                    }
                    else if (ImGui::IsMouseClicked(1)) {
                        g_modShowSettings[mod] = !g_modShowSettings[mod];
                    }
                    else if (ImGui::IsMouseClicked(2)) {
                        if (isBindingThis) {
                            InputManager::instance().cancel_binding();
                        } else {
                            InputManager::instance().begin_binding(mod);
                        }
                    }
                }

                currentY += MOD_HEIGHT;

                if (settingsAnim > 0.005f) {
                    float drawerStartY = currentY;

                    float drawerContentH = MOD_HEIGHT * settingsAnim; // hold mode
                    for (const auto& s : mod->settings()) {
                        if (!s || !s->is_visible()) continue;
                        drawerContentH += (s->type() == config::SettingType::FLOAT || s->type() == config::SettingType::INT ? (MOD_HEIGHT + 2.0f) : MOD_HEIGHT) * settingsAnim;
                        if (s->type() == config::SettingType::ENUM) {
                            float enumAnim = g_settingEnumSlide[s.get()];
                            auto* es = static_cast<config::EnumSetting*>(s.get());
                            if (enumAnim > 0.005f) {
                                drawerContentH += static_cast<float>(es->options().size()) * MOD_HEIGHT * enumAnim * settingsAnim;
                            }
                        }
                    }

                    float drawerEndY = drawerStartY + drawerContentH;
                    float drawerRadius = isLastModule ? (CORNER_RADIUS * settingsAnim) : 0.0f;
                    if (drawerEndY >= catBottomY - 1.0f) {
                        drawerEndY = catBottomY;
                        drawerRadius = CORNER_RADIUS;
                    }

                    if (drawerEndY > drawerStartY) {
                        FillRectangle(
                            ImVec4(catX, drawerStartY, catX + CAT_WIDTH, drawerEndY),
                            kSettingBgColor, settingsAnim, drawerRadius,
                            ImDrawFlags_RoundCornersBottom, dl
                        );
                    }

                    {
                        float rowH = MOD_HEIGHT * settingsAnim;
                        if (rowH >= 1.0f) {
                            ImVec4 rowRect(catX, currentY, catX + CAT_WIDTH, currentY + rowH);
                            float textRowY = currentY + (rowH - textH) * 0.5f;

                            dl->PushClipRect(ImVec2(rowRect.x, rowRect.y), ImVec2(rowRect.z, rowRect.w + 0.5f), true);

                            dl->AddText(ImVec2(catX + 14.0f, textRowY), static_cast<ImU32>(kTextWhite), "Hold Mode");

                            ImVec2 checkCenter(catX + CAT_WIDTH - 18.0f, currentY + rowH * 0.5f);
                            ImVec2 boxMin(checkCenter.x - 7.0f, checkCenter.y - 7.0f);
                            ImVec2 boxMax(checkCenter.x + 7.0f, checkCenter.y + 7.0f);

                            dl->AddRectFilled(boxMin, boxMax, IM_COL32(32, 32, 32, static_cast<int>(240 * settingsAnim)), 3.5f);
                            dl->AddRect(boxMin, boxMax, IM_COL32(75, 75, 75, static_cast<int>(220 * settingsAnim)), 3.5f, 0, 1.0f);

                            float targetHold = mod->hold_to_enable() ? 1.0f : 0.0f;
                            g_modHoldAnim[mod] = Animate(targetHold, g_modHoldAnim[mod], 14.0f);
                            float hScale = g_modHoldAnim[mod];

                            if (hScale > 0.01f) {
                                FillShadowCircle(checkCenter, 5.5f, kEmeraldAccent, hScale * 0.85f * settingsAnim, 20.0f, dl);
                                dl->AddRectFilled(boxMin, boxMax, ImColor(kEmeraldAccent.Value.x, kEmeraldAccent.Value.y, kEmeraldAccent.Value.z, hScale * settingsAnim), 3.5f);
                                dl->AddRect(boxMin, boxMax, ImColor(kEmeraldDark.Value.x, kEmeraldDark.Value.y, kEmeraldDark.Value.z, hScale * settingsAnim), 3.5f, 0, 1.0f);
                            }

                            if (hScale > 0.02f) {
                                ImVec4 checkClip(boxMin.x, boxMin.y, boxMin.x + 14.0f * hScale, boxMax.y);
                                dl->PushClipRect(ImVec2(checkClip.x, checkClip.y), ImVec2(checkClip.z, checkClip.w), true);
                                DrawCheckMark(ImVec2(checkCenter.x - 4.5f, checkCenter.y - 1.0f), 1.25f, kTextWhite, settingsAnim, 2.0f, dl);
                                dl->PopClipRect();
                            }

                            if (IsMouseOver(rowRect) && !io.WantCaptureMouse) {
                                hoveredTooltip = "When enabled, module is only active while holding its bound key.";
                                if (ImGui::IsMouseClicked(0)) {
                                    mod->set_hold_to_enable(!mod->hold_to_enable());
                                }
                            }

                            dl->PopClipRect();
                            currentY += rowH;
                        }
                    }

                    for (size_t sIdx = 0; sIdx < mod->settings().size(); ++sIdx) {
                        auto* setting = mod->settings()[sIdx].get();
                        if (!setting || !setting->is_visible()) continue;

                        const bool isSlider = (setting->type() == config::SettingType::FLOAT || setting->type() == config::SettingType::INT);
                        float setHeight = (isSlider ? (MOD_HEIGHT + 2.0f) : MOD_HEIGHT) * settingsAnim;
                        if (setHeight < 1.0f) continue;

                        ImVec4 setRect(catX, currentY, catX + CAT_WIDTH, currentY + setHeight);
                        dl->PushClipRect(ImVec2(setRect.x, setRect.y), ImVec2(setRect.z, setRect.w + 0.5f), true);

                        if (setting->type() == config::SettingType::BOOL) {
                            auto* bs = static_cast<config::BoolSetting*>(setting);
                            float targetBool = bs->value() ? 1.0f : 0.0f;
                            g_settingBoolScale[setting] = Animate(targetBool, g_settingBoolScale[setting], 14.0f);
                            float bScale = g_settingBoolScale[setting];

                            float textRowY = currentY + (setHeight - textH) * 0.5f;
                            dl->AddText(ImVec2(catX + 14.0f, textRowY), static_cast<ImU32>(kTextWhite), setting->name().c_str());

                            ImVec2 checkCenter(catX + CAT_WIDTH - 18.0f, currentY + setHeight * 0.5f);
                            ImVec2 boxMin(checkCenter.x - 7.0f, checkCenter.y - 7.0f);
                            ImVec2 boxMax(checkCenter.x + 7.0f, checkCenter.y + 7.0f);

                            dl->AddRectFilled(boxMin, boxMax, IM_COL32(32, 32, 32, static_cast<int>(240 * settingsAnim)), 3.5f);
                            dl->AddRect(boxMin, boxMax, IM_COL32(75, 75, 75, static_cast<int>(220 * settingsAnim)), 3.5f, 0, 1.0f);

                            if (bScale > 0.01f) {
                                FillShadowCircle(checkCenter, 5.5f, kEmeraldAccent, bScale * 0.85f * settingsAnim, 20.0f, dl);
                                dl->AddRectFilled(boxMin, boxMax, ImColor(kEmeraldAccent.Value.x, kEmeraldAccent.Value.y, kEmeraldAccent.Value.z, bScale * settingsAnim), 3.5f);
                                dl->AddRect(boxMin, boxMax, ImColor(kEmeraldDark.Value.x, kEmeraldDark.Value.y, kEmeraldDark.Value.z, bScale * settingsAnim), 3.5f, 0, 1.0f);
                            }

                            if (bScale > 0.02f) {
                                ImVec4 checkClip(boxMin.x, boxMin.y, boxMin.x + 14.0f * bScale, boxMax.y);
                                dl->PushClipRect(ImVec2(checkClip.x, checkClip.y), ImVec2(checkClip.z, checkClip.w), true);
                                DrawCheckMark(ImVec2(checkCenter.x - 4.5f, checkCenter.y - 1.0f), 1.25f, kTextWhite, settingsAnim, 2.0f, dl);
                                dl->PopClipRect();
                            }

                            if (IsMouseOver(setRect) && !io.WantCaptureMouse) {
                                if (!setting->description().empty()) hoveredTooltip = setting->description();
                                if (ImGui::IsMouseClicked(0)) {
                                    bs->set_value(!bs->value());
                                }
                            }
                        }
                        else if (isSlider) {
                            float val = 0.0f, minVal = 0.0f, maxVal = 1.0f;
                            char valBuf[32];
                            if (setting->type() == config::SettingType::FLOAT) {
                                auto* fs = static_cast<config::FloatSetting*>(setting);
                                val = fs->value(); minVal = fs->min(); maxVal = fs->max();
                                snprintf(valBuf, sizeof(valBuf), fs->format(), val);
                            } else {
                                auto* is = static_cast<config::IntSetting*>(setting);
                                val = static_cast<float>(is->value()); minVal = static_cast<float>(is->min()); maxVal = static_cast<float>(is->max());
                                snprintf(valBuf, sizeof(valBuf), "%d", static_cast<int>(val));
                            }

                            float textTopY = currentY + 3.0f;
                            dl->AddText(ImVec2(catX + 14.0f, textTopY), static_cast<ImU32>(kTextWhite), setting->name().c_str());

                            ImVec2 valSz = ImGui::CalcTextSize(valBuf);
                            dl->AddText(ImVec2(catX + CAT_WIDTH - valSz.x - 12.0f, textTopY), static_cast<ImU32>(kEmeraldLight), valBuf);

                            // Bottom Track Bar
                            float trackX1 = catX + 14.0f;
                            float trackX2 = catX + CAT_WIDTH - 12.0f;
                            float trackW = trackX2 - trackX1;
                            float trackY = currentY + setHeight - 6.0f;

                            float fraction = (maxVal > minVal) ? std::clamp((val - minVal) / (maxVal - minVal), 0.0f, 1.0f) : 0.0f;
                            float targetSliderWidth = fraction * trackW;
                            g_settingSliderEase[setting] = Animate(targetSliderWidth, g_settingSliderEase[setting], 16.0f);
                            float fillWidth = g_settingSliderEase[setting];

                            dl->AddRectFilled(ImVec2(trackX1, trackY - 1.5f), ImVec2(trackX2, trackY + 1.5f), IM_COL32(45, 45, 45, static_cast<int>(200 * settingsAnim)), 2.0f);

                            ImVec4 fillRect(trackX1, trackY - 2.0f, trackX1 + fillWidth, trackY + 2.0f);
                            FillRectangle(fillRect, kEmeraldAccent, settingsAnim, 2.0f, 0, dl);

                            ImVec2 handlePos(trackX1 + fillWidth, trackY);
                            FillShadowCircle(handlePos, 4.5f, kEmeraldAccent, 0.85f * settingsAnim, 20.0f, dl);
                            dl->AddCircleFilled(handlePos, 4.0f, static_cast<ImU32>(kTextWhite));

                            if (IsMouseOver(setRect) && !io.WantCaptureMouse) {
                                if (!setting->description().empty()) hoveredTooltip = setting->description();
                                if (ImGui::IsMouseDown(0)) {
                                    g_activeDraggedSlider = setting;
                                }
                            }

                            if (g_activeDraggedSlider == setting) {
                                if (ImGui::IsMouseDown(0)) {
                                    float newFrac = std::clamp((io.MousePos.x - trackX1) / trackW, 0.0f, 1.0f);
                                    float newVal = minVal + newFrac * (maxVal - minVal);
                                    if (setting->type() == config::SettingType::FLOAT) {
                                        static_cast<config::FloatSetting*>(setting)->set_value(newVal);
                                    } else {
                                        static_cast<config::IntSetting*>(setting)->set_value(static_cast<int>(std::round(newVal)));
                                    }
                                } else {
                                    g_activeDraggedSlider = nullptr;
                                }
                            }
                        }
                        else if (setting->type() == config::SettingType::ENUM) {
                            auto* es = static_cast<config::EnumSetting*>(setting);
                            float textRowY = currentY + (setHeight - textH) * 0.5f;

                            dl->AddText(ImVec2(catX + 14.0f, textRowY), static_cast<ImU32>(kTextWhite), setting->name().c_str());

                            std::string optName = es->current_option();
                            ImVec2 optSz = ImGui::CalcTextSize(optName.c_str());
                            dl->AddText(ImVec2(catX + CAT_WIDTH - optSz.x - 12.0f, textRowY), static_cast<ImU32>(kTextMuted), optName.c_str());

                            if (IsMouseOver(setRect) && !io.WantCaptureMouse) {
                                if (!setting->description().empty()) hoveredTooltip = setting->description();
                                if (ImGui::IsMouseClicked(0)) {
                                    int curVal = es->value();
                                    int nextVal = (curVal + 1) % static_cast<int>(es->options().size());
                                    es->set_value(nextVal);
                                } else if (ImGui::IsMouseClicked(1)) {
                                    g_settingEnumOpen[setting] = !g_settingEnumOpen[setting];
                                }
                            }
                        }
                        else if (setting->type() == config::SettingType::COLOR) {
                            auto* cs = static_cast<config::ColorSetting*>(setting);
                            float textRowY = currentY + (setHeight - textH) * 0.5f;
                            dl->AddText(ImVec2(catX + 14.0f, textRowY), static_cast<ImU32>(kTextWhite), setting->name().c_str());

                            ImVec4 swatchRect(catX + CAT_WIDTH - 24.0f, currentY + 7.0f, catX + CAT_WIDTH - 8.0f, currentY + setHeight - 7.0f);
                            float colArr[4];
                            cs->get_color(colArr);
                            ImColor imCol(colArr[0], colArr[1], colArr[2], colArr[3]);

                            dl->AddRectFilled(ImVec2(swatchRect.x, swatchRect.y), ImVec2(swatchRect.z, swatchRect.w), imCol, 3.0f);
                            dl->AddRect(ImVec2(swatchRect.x, swatchRect.y), ImVec2(swatchRect.z, swatchRect.w), IM_COL32(255, 255, 255, 70), 3.0f);

                            if (IsMouseOver(setRect) && !io.WantCaptureMouse) {
                                if (!setting->description().empty()) hoveredTooltip = setting->description();
                                if (ImGui::IsMouseClicked(0)) {
                                    g_activeColorSetting = cs;
                                    g_showColorPicker = true;
                                }
                            }
                        }

                        dl->PopClipRect();
                        currentY += setHeight;

                        if (setting->type() == config::SettingType::ENUM) {
                            auto* es = static_cast<config::EnumSetting*>(setting);
                            float enumAnim = g_settingEnumSlide[setting];
                            if (enumAnim > 0.005f) {
                                int curIdx = es->value();
                                for (size_t oIdx = 0; oIdx < es->options().size(); ++oIdx) {
                                    float optHeight = MOD_HEIGHT * enumAnim * settingsAnim;
                                    if (optHeight < 1.0f) continue;

                                    ImVec4 optRect(catX, currentY, catX + CAT_WIDTH, currentY + optHeight);
                                    dl->PushClipRect(ImVec2(optRect.x, optRect.y), ImVec2(optRect.z, optRect.w + 0.5f), true);

                                    FillRectangle(optRect, kEnumSubBg, enumAnim * settingsAnim, 0.0f, 0, dl);

                                    const bool isSelected = (static_cast<int>(oIdx) == curIdx);
                                    if (isSelected) {
                                        FillRectangle(ImVec4(optRect.x, optRect.y, optRect.x + 2.5f, optRect.w), kEmeraldAccent, enumAnim * settingsAnim, 0.0f, 0, dl);
                                    }

                                    float optTextY = currentY + (optHeight - textH) * 0.5f;
                                    ImColor optCol = isSelected ? kTextWhite : kTextMuted;
                                    dl->AddText(ImVec2(catX + 18.0f, optTextY), static_cast<ImU32>(optCol), es->options()[oIdx].c_str());

                                    if (IsMouseOver(optRect) && ImGui::IsMouseClicked(0) && !io.WantCaptureMouse) {
                                        es->set_value(static_cast<int>(oIdx));
                                    }

                                    dl->PopClipRect();
                                    currentY += optHeight;
                                }
                            }
                        }
                    }
                }
            }

            dl->PopClipRect();
        }

        {
            const bool isBinding = InputManager::instance().is_binding();
            const char* keyPrefix = isBinding ? "[Binding]" : "[Middle Click]";
            std::string hintSuffix = isBinding
                ? (" Press any key for " + (InputManager::instance().binding_module() ? InputManager::instance().binding_module()->name() : "module") + " ESC to cancel")
                : " to set keybind";

            ImVec2 preSz = ImGui::CalcTextSize(keyPrefix);
            ImVec2 sufSz = ImGui::CalcTextSize(hintSuffix.c_str());
            const float dotW = 14.0f;
            const float pillW = preSz.x + sufSz.x + dotW + 20.0f;
            const float pillH = 24.0f;
            const float pillX = (screen.x - pillW) * 0.5f;
            const float pillY = screen.y - 34.0f;

            ImVec4 pillRect(pillX, pillY, pillX + pillW, pillY + pillH);

            FillRectangle(pillRect, kDarkHeader, 0.90f, 12.0f, ImDrawFlags_RoundCornersAll, dl);
            dl->AddRect(ImVec2(pillRect.x, pillRect.y), ImVec2(pillRect.z, pillRect.w), static_cast<ImU32>(kEmeraldGlow), 12.0f, 0, 1.0f);

            ImVec2 dotPos(pillX + 11.0f, pillY + pillH * 0.5f);
            dl->AddCircleFilled(dotPos, 4.5f, static_cast<ImU32>(kEmeraldGlow));
            dl->AddCircleFilled(dotPos, 2.2f, static_cast<ImU32>(kEmeraldAccent));

            float textY = pillY + (pillH - ImGui::GetTextLineHeight()) * 0.5f;
            dl->AddText(ImVec2(pillX + dotW + 8.0f, textY), static_cast<ImU32>(kEmeraldLight), keyPrefix);
            dl->AddText(ImVec2(pillX + dotW + 8.0f + preSz.x, textY), static_cast<ImU32>(kTextMuted), hintSuffix.c_str());
        }

        if (!hoveredTooltip.empty()) {
            ImVec2 m = io.MousePos;
            ImVec2 ttSize = ImGui::CalcTextSize(hoveredTooltip.c_str());
            ImVec4 ttRect(m.x + 12.0f, m.y + 12.0f, m.x + ttSize.x + 24.0f, m.y + ttSize.y + 20.0f);

            if (ttRect.z > screen.x - 8.0f) {
                float shift = ttRect.z - (screen.x - 8.0f);
                ttRect.x -= shift;
                ttRect.z -= shift;
            }

            ImDrawList* fg = ImGui::GetForegroundDrawList();
            FillRectangle(ttRect, kDarkHeader, 0.96f, 6.0f, ImDrawFlags_RoundCornersAll, fg);
            fg->AddRect(ImVec2(ttRect.x, ttRect.y), ImVec2(ttRect.z, ttRect.w), static_cast<ImU32>(kEmeraldGlow), 6.0f, 0, 1.0f);
            fg->AddText(ImVec2(ttRect.x + 6.0f, ttRect.y + 4.0f), static_cast<ImU32>(kTextWhite), hoveredTooltip.c_str());
        }

        if (g_showColorPicker && g_activeColorSetting) {
            ImGui::SetNextWindowSize(ImVec2(240.0f, 260.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Color Picker", &g_showColorPicker, ImGuiWindowFlags_NoCollapse)) {
                float colArr[4];
                g_activeColorSetting->get_color(colArr);
                if (ImGui::ColorPicker4("##ColorPickerDiag", colArr, ImGuiColorEditFlags_AlphaBar)) {
                    g_activeColorSetting->set_color(colArr[0], colArr[1], colArr[2], colArr[3]);
                }
            }
            ImGui::End();
        }
    }

} // namespace clickgui

// ==============================================================================
// SOURCE: src/ui/ui_settings.cpp
// ==============================================================================
namespace {
float g_uiScale = 1.0f;
float g_appliedUiScale = 0.0f;
ImGuiStyle g_baseStyle{};
bool g_hasBaseStyle = false;
}

namespace ui_settings {
float ui_scale() {
    return g_uiScale;
}

void set_ui_scale(float scale) {
    g_uiScale = std::clamp(scale, 0.65f, 1.75f);
}

void apply_ui_scale() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    if (!g_hasBaseStyle) {
        g_baseStyle = ImGui::GetStyle();
        g_hasBaseStyle = true;
    }

    if (g_appliedUiScale == g_uiScale) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style = g_baseStyle;
    style.ScaleAllSizes(g_uiScale);
    ImGui::GetIO().FontGlobalScale = g_uiScale;
    g_appliedUiScale = g_uiScale;
}

void reset_ui_scale_cache() {
    g_appliedUiScale = 0.0f;
    g_baseStyle = ImGuiStyle{};
    g_hasBaseStyle = false;
}
}

// ==============================================================================
// SOURCE: src/ui/widgets/widgets.cpp
// ==============================================================================
namespace ui {

    static Palette g_palette{};

    const Palette& get_palette() {
        return g_palette;
    }

    static void DrawGlowCircle(const ImVec2& pos, float radius, ImU32 color, ImDrawList* dl) {
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(color);
        dl->AddCircleFilled(pos, radius * 1.8f, ImColor(col.x, col.y, col.z, col.w * 0.12f));
        dl->AddCircleFilled(pos, radius * 1.3f, ImColor(col.x, col.y, col.z, col.w * 0.28f));
        dl->AddCircleFilled(pos, radius, ImColor(col.x, col.y, col.z, col.w * 0.55f));
    }

    static void DrawCheckMark(const ImVec2& pos, float size, ImU32 color, float thickness, ImDrawList* dl) {
        ImVec2 end1(pos.x + (3.0f * size), pos.y + (3.0f * size));
        ImVec2 end2(pos.x + (7.0f * size), pos.y - (3.0f * size));
        dl->AddLine(pos, end1, color, thickness);
        dl->AddLine(end1, end2, color, thickness);
    }

    void apply_theme() {
        const Palette& p = get_palette();
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);

        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 4.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(p.text_primary);
        colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(p.text_secondary);
        colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(p.bg_dark);
        colors[ImGuiCol_ChildBg] = ImGui::ColorConvertU32ToFloat4(p.panel_bg);
        colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(p.panel_bg);
        colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(p.border);
        colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(p.card_bg);
        colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(p.card_hover);
        colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(p.card_hover);
        colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(p.panel_bg);
        colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(p.panel_bg);
        colors[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(p.bg_dark);
        colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(p.card_hover);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(p.border);
        colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(p.accent);
        colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(p.accent);
        colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(p.accent);
        colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(p.accent_active);
        colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(p.card_bg);
        colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(p.card_hover);
        colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(p.accent_muted);
        colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(p.accent_muted);
        colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(p.card_hover);
        colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(p.accent);
        colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(p.border);
    }

    bool Checkbox(const char* label, bool* v) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        const float boxSize = 15.0f;
        const float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
        }

        ImVec2 boxPos(cursor.x + availW - boxSize, cursor.y + (ImGui::GetTextLineHeight() - boxSize) * 0.5f);

        ImGui::SetCursorScreenPos(boxPos);
        ImGui::InvisibleButton("##chk_btn", ImVec2(boxSize, boxSize));

        const bool clicked = ImGui::IsItemClicked();
        const bool hovered = ImGui::IsItemHovered();

        if (clicked) {
            *v = !(*v);
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 boxMax(boxPos.x + boxSize, boxPos.y + boxSize);
        ImVec2 boxCenter(boxPos.x + boxSize * 0.5f, boxPos.y + boxSize * 0.5f);

        if (*v) {
            DrawGlowCircle(boxCenter, 6.0f, p.accent_muted, dl);
            dl->AddRectFilled(boxPos, boxMax, p.accent, 3.5f);
            dl->AddRect(boxPos, boxMax, p.accent_active, 3.5f, 0, 1.0f);
            DrawCheckMark(ImVec2(boxCenter.x - 4.5f, boxCenter.y - 0.5f), 1.25f, p.text_primary, 2.0f, dl);
        } else {
            const ImU32 bg = hovered ? p.card_hover : p.card_bg;
            dl->AddRectFilled(boxPos, boxMax, bg, 3.5f);
            dl->AddRect(boxPos, boxMax, hovered ? p.border_focused : p.border, 3.5f, 0, 1.0f);
        }

        ImGui::PopID();
        return clicked;
    }

    bool ToggleSwitch(const char* label, bool* v) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        const float switchW = 32.0f;
        const float switchH = 17.0f;
        const float availW = ImGui::GetContentRegionAvail().x;

        ImVec2 cursor = ImGui::GetCursorScreenPos();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
        }

        ImVec2 switchPos(cursor.x + availW - switchW, cursor.y + (ImGui::GetTextLineHeight() - switchH) * 0.5f);

        ImGui::SetCursorScreenPos(switchPos);
        ImGui::InvisibleButton("##sw_btn", ImVec2(switchW, switchH));

        const bool clicked = ImGui::IsItemClicked();
        const bool hovered = ImGui::IsItemHovered();

        if (clicked) {
            *v = !(*v);
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 switchMax(switchPos.x + switchW, switchPos.y + switchH);

        if (*v) {
            DrawGlowCircle(ImVec2(switchPos.x + switchW - 8.5f, switchPos.y + switchH * 0.5f), 7.0f, p.accent_muted, dl);
            dl->AddRectFilled(switchPos, switchMax, p.accent, 8.5f);
            dl->AddRect(switchPos, switchMax, p.accent_active, 8.5f, 0, 1.0f);

            const float knobX = switchPos.x + switchW - 8.5f;
            dl->AddCircleFilled(ImVec2(knobX, switchPos.y + switchH * 0.5f), 5.5f, p.text_primary);
        } else {
            const ImU32 bg = hovered ? p.card_hover : p.card_bg;
            dl->AddRectFilled(switchPos, switchMax, bg, 8.5f);
            dl->AddRect(switchPos, switchMax, hovered ? p.border_focused : p.border, 8.5f, 0, 1.0f);

            const float knobX = switchPos.x + 8.5f;
            dl->AddCircleFilled(ImVec2(knobX, switchPos.y + switchH * 0.5f), 5.5f, p.text_secondary);
        }

        ImGui::PopID();
        return clicked;
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        const float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        char valBuf[64];
        std::snprintf(valBuf, sizeof(valBuf), format, *v);
        const ImVec2 valSize = ImGui::CalcTextSize(valBuf);

        const float rowHeight = 28.0f;
        ImGui::InvisibleButton("##slider_area", ImVec2(availW, rowHeight));

        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        bool changed = false;

        const float trackX1 = cursor.x;
        const float trackX2 = cursor.x + availW;
        const float trackW = trackX2 - trackX1;
        const float trackY = cursor.y + rowHeight - 6.0f;

        if (active) {
            float mouseX = ImGui::GetIO().MousePos.x - trackX1;
            float percent = ImClamp(mouseX / trackW, 0.0f, 1.0f);
            float newVal = v_min + percent * (v_max - v_min);
            if (*v != newVal) {
                *v = newVal;
                changed = true;
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            dl->AddText(cursor, p.text_primary, label);
        }
        dl->AddText(ImVec2(trackX2 - valSize.x, cursor.y), p.accent_hover, valBuf);

        float fraction = (v_max > v_min) ? ImClamp((*v - v_min) / (v_max - v_min), 0.0f, 1.0f) : 0.0f;
        float fillW = fraction * trackW;

        dl->AddRectFilled(ImVec2(trackX1, trackY - 2.0f), ImVec2(trackX2, trackY + 2.0f), p.card_hover, 2.0f);
        if (fillW > 0.0f) {
            dl->AddRectFilled(ImVec2(trackX1, trackY - 2.0f), ImVec2(trackX1 + fillW, trackY + 2.0f), active ? p.accent_active : p.accent, 2.0f);
        }

        ImVec2 knobPos(trackX1 + fillW, trackY);
        if (hovered || active) {
            DrawGlowCircle(knobPos, 7.0f, p.accent_muted, dl);
            dl->AddCircleFilled(knobPos, 5.0f, p.text_primary);
        } else {
            dl->AddCircleFilled(knobPos, 4.0f, p.accent_hover);
        }

        ImGui::PopID();
        return changed;
    }

    bool SliderInt(const char* label, int* v, int v_min, int v_max) {
        float f_val = static_cast<float>(*v);
        bool changed = SliderFloat(label, &f_val, static_cast<float>(v_min), static_cast<float>(v_max), "%.0f");
        if (changed) {
            *v = static_cast<int>(std::round(f_val));
        }
        return changed;
    }

    bool Combo(const char* label, int* current_item, const std::vector<std::string>& items) {
        if (items.empty()) return false;
        ImGui::PushID(label);
        const Palette& p = get_palette();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::TextUnformatted(label);
            ImGui::Spacing();
        }

        std::string currentStr = (*current_item >= 0 && *current_item < static_cast<int>(items.size()))
            ? items[*current_item] : "Select...";

        bool changed = false;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.card_bg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.card_hover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, p.card_hover);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, p.panel_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, p.border);
        ImGui::PushStyleColor(ImGuiCol_Header, p.accent_muted);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, p.accent_muted);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, p.accent);
        ImGui::PushStyleColor(ImGuiCol_Text, p.text_primary);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 5.0f));

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##combo_box", currentStr.c_str())) {
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                const bool isSelected = (*current_item == i);
                if (ImGui::Selectable(items[i].c_str(), isSelected)) {
                    *current_item = i;
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(9);
        ImGui::PopID();
        return changed;
    }

    bool ColorPicker(const char* label, float col[4]) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
        }

        ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.card_bg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.card_hover);
        bool changed = ImGui::ColorEdit4("##color_edit", col, flags);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImGui::PopID();
        return changed;
    }

    bool InputText(const char* label, char* buf, size_t buf_size) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::TextUnformatted(label);
            ImGui::Spacing();
        }

        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.card_bg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.card_hover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, p.panel_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, p.border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::SetNextItemWidth(-1.0f);
        bool changed = ImGui::InputText("##text_input", buf, buf_size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return changed;
    }

    bool KeybindBadge(const char* label, int keybind, bool is_binding) {
        ImGui::PushID(label);
        const Palette& p = get_palette();

        if (label && !(label[0] == '#' && label[1] == '#')) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
        }

        std::string keyStr = is_binding ? "[ ... ]" : (std::string("[ ") + input::get_key_name(keybind) + " ]");
        ImVec2 textSz = ImGui::CalcTextSize(keyStr.c_str());
        ImVec2 btnSz(textSz.x + 14.0f, textSz.y + 6.0f);

        const float availW = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 btnPos(cursor.x + availW - btnSz.x, cursor.y + (ImGui::GetTextLineHeight() - btnSz.y) * 0.5f);

        ImGui::SetCursorScreenPos(btnPos);
        ImGui::InvisibleButton("##keybind_btn", btnSz);

        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = is_binding ? p.accent_muted : (hovered ? p.card_hover : p.card_bg);
        ImU32 border = is_binding ? p.accent : (hovered ? p.border_focused : p.border);
        ImU32 textCol = is_binding ? p.accent_hover : (hovered ? p.text_primary : p.text_secondary);

        if (is_binding) {
            DrawGlowCircle(ImVec2(btnPos.x + btnSz.x * 0.5f, btnPos.y + btnSz.y * 0.5f), btnSz.y * 0.6f, p.accent_muted, dl);
        }

        dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSz.x, btnPos.y + btnSz.y), bg, 4.0f);
        dl->AddRect(btnPos, ImVec2(btnPos.x + btnSz.x, btnPos.y + btnSz.y), border, 4.0f, 0, 1.0f);
        dl->AddText(ImVec2(btnPos.x + 7.0f, btnPos.y + 3.0f), textCol, keyStr.c_str());

        ImGui::PopID();
        return clicked;
    }

    bool SearchBar(const char* id, char* buf, size_t buf_size, const char* placeholder) {
        ImGui::PushID(id);
        const Palette& p = get_palette();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, p.card_bg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, p.card_hover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, p.panel_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, p.border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));

        bool changed = ImGui::InputTextWithHint("##search_input", placeholder, buf, buf_size);

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return changed;
    }

    void SectionHeader(const char* title, const char* badge) {
        const Palette& p = get_palette();
        ImGui::TextColored(ImColor(p.text_primary), "%s", title);
        if (badge && badge[0] != '\0') {
            ImGui::SameLine();
            ImGui::TextColored(ImColor(p.accent_hover), "(%s)", badge);
        }
        ImGui::Spacing();
    }

    bool PrimaryButton(const char* label, const ImVec2& size) {
        const Palette& p = get_palette();
        ImGui::PushStyleColor(ImGuiCol_Button, p.accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.accent_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.accent_active);
        ImGui::PushStyleColor(ImGuiCol_Text, p.bg_dark);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

        bool clicked = ImGui::Button(label, size);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        return clicked;
    }

    bool SecondaryButton(const char* label, const ImVec2& size) {
        const Palette& p = get_palette();
        ImGui::PushStyleColor(ImGuiCol_Button, p.card_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.card_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.panel_bg);
        ImGui::PushStyleColor(ImGuiCol_Border, p.border);
        ImGui::PushStyleColor(ImGuiCol_Text, p.text_primary);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool clicked = ImGui::Button(label, size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        return clicked;
    }

    bool DangerButton(const char* label, const ImVec2& size) {
        const Palette& p = get_palette();
        ImGui::PushStyleColor(ImGuiCol_Button, p.danger);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, p.danger_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, p.danger);
        ImGui::PushStyleColor(ImGuiCol_Text, p.text_primary);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

        bool clicked = ImGui::Button(label, size);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
        return clicked;
    }
}

// ==============================================================================
// SOURCE: src/utils/logger.cpp
// ==============================================================================
namespace {
    std::string mc_to_ansi(const std::string& input) {
        static const std::unordered_map<char, const char*> colorMap = {
            {'0', "\033[30m"},  // Black
            {'1', "\033[34m"},  // Dark Blue
            {'2', "\033[32m"},  // Dark Green
            {'3', "\033[36m"},  // Dark Aqua
            {'4', "\033[31m"},  // Dark Red
            {'5', "\033[35m"},  // Dark Purple
            {'6', "\033[33m"},  // Gold
            {'7', "\033[37m"},  // Gray
            {'8', "\033[90m"},  // Dark Gray
            {'9', "\033[94m"},  // Blue
            {'a', "\033[92m"},  // Green
            {'b', "\033[96m"},  // Aqua
            {'c', "\033[91m"},  // Red
            {'d', "\033[95m"},  // Light Purple
            {'e', "\033[93m"},  // Yellow
            {'f', "\033[97m"},  // White
            {'r', "\033[0m"},   // Reset
            {'l', "\033[1m"},   // Bold
            {'o', "\033[3m"}    // Italic
        };

        std::string result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if ((input[i] == '\xA7' || (static_cast<unsigned char>(input[i]) == 0xC2 && i + 1 < input.size() && static_cast<unsigned char>(input[i + 1]) == 0xA7)) && i + 1 < input.size()) {

                size_t codeIdx = (input[i] == '\xA7') ? i + 1 : i + 2;
                if (codeIdx < input.size()) {
                    char code = static_cast<char>(std::tolower(input[codeIdx]));
                    auto it = colorMap.find(code);
                    if (it != colorMap.end()) {
                        result += it->second;
                        i = codeIdx; // Skip code bytes
                        continue;
                    }
                }
            }
            result += input[i];
        }
        return result;
    }

    std::string strip_mc_colors(const std::string& input) {
        std::string result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\xA7' && i + 1 < input.size()) {
                i++; // Skip '\xA7' and code
                continue;
            }
            if (static_cast<unsigned char>(input[i]) == 0xC2 && i + 1 < input.size() && static_cast<unsigned char>(input[i + 1]) == 0xA7 && i + 2 < input.size()) {
                i += 2; // Skip UTF-8 '\xC2\xA7' and code
                continue;
            }
            result += input[i];
        }
        return result;
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

std::filesystem::path Logger::get_log_directory() const {
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path path = std::filesystem::path(appdata) / "MCBE_DX12_ModBase" / "logs";
        free(appdata);
        return path;
    }
    return std::filesystem::current_path() / "logs";
}

void Logger::initialize(const std::string& clientName, bool enableConsole, bool enableFile) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return;

    bool showConsole = enableConsole;

    if (showConsole) {
        if (AllocConsole() || AttachConsole(ATTACH_PARENT_PROCESS)) {
            m_consoleAllocated = true;

            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);

            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONIN$", "r", stdin);
            freopen_s(&dummy, "CONOUT$", "w", stderr);

            std::cout.clear();
            std::cerr.clear();
            std::cin.clear();
            std::ios::sync_with_stdio(true);

            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, dwMode);
                }
            }
            SetConsoleTitleA((clientName + " - Debug Console").c_str());
        }
    }

    if (enableFile) {
        try {
            auto logDir = get_log_directory();
            std::filesystem::create_directories(logDir);
            auto logFilePath = logDir / "latest.log";
            m_logFile.open(logFilePath, std::ios::out | std::ios::trunc);
            m_fileOutput = m_logFile.is_open();
        }
        catch (...) {
            m_fileOutput = false;
        }
    }

    m_initialized = true;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;

    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }

    if (m_consoleAllocated) {
        HWND consoleWnd = GetConsoleWindow();

        FILE* dummy = nullptr;
        freopen_s(&dummy, "NUL", "w", stdout);
        freopen_s(&dummy, "NUL", "r", stdin);
        freopen_s(&dummy, "NUL", "w", stderr);

        FreeConsole();

        if (consoleWnd != nullptr && IsWindow(consoleWnd)) {
            PostMessageA(consoleWnd, WM_CLOSE, 0, 0);
        }

        m_consoleAllocated = false;
    }

    m_initialized = false;
}

void Logger::write_log(LogLevel level, const std::source_location& loc, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm time_info{};
    localtime_s(&time_info, &time_t_now);

    std::string time_str = std::format("{:02d}:{:02d}:{:02d}", time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    std::string filename = std::filesystem::path(loc.file_name()).filename().string();

    const char* level_str = "INFO";
    const char* level_color = "\033[32m"; // Green

    switch (level) {
    case LogLevel::Trace: level_str = "TRACE"; level_color = "\033[90m"; break; // Gray
    case LogLevel::Debug: level_str = "DEBUG"; level_color = "\033[36m"; break; // Cyan
    case LogLevel::Info:  level_str = "INFO";  level_color = "\033[32m"; break; // Green
    case LogLevel::Warn:  level_str = "WARN";  level_color = "\033[33m"; break; // Yellow
    case LogLevel::Error: level_str = "ERROR"; level_color = "\033[31m"; break; // Red
    case LogLevel::Fatal: level_str = "FATAL"; level_color = "\033[35m"; break; // Magenta
    }

    std::string clean_message = strip_mc_colors(message);
    std::string color_message = mc_to_ansi(message);

    std::string plain_line = std::format("[{}] [{}] [{}:{}] {}\n",
        time_str, level_str, filename, loc.line(), clean_message);

    OutputDebugStringA(plain_line.c_str());

    if (m_fileOutput && m_logFile.is_open()) {
        m_logFile << plain_line;
        m_logFile.flush();
    }

    if (m_consoleAllocated) {
        std::string console_line = std::format("\033[90m[{}]\033[0m {}[{}]\033[0m \033[90m[{}:{}]\033[0m {}\033[0m\n",
            time_str, level_color, level_str, filename, loc.line(), color_message);
        std::cout << console_line;
    }
}

// ==============================================================================
// SOURCE: src/platform/windows/dllmain.cpp
// ==============================================================================
DWORD WINAPI main_thread(LPVOID param) {
    const auto module = static_cast<HMODULE>(param);

    Client::instance().initialize(module);

    while (!(GetAsyncKeyState(VK_END) & 1)) {
        Sleep(50);
    }

    LOG_INFO("Unload hotkey (END) detected. Uninjecting DLL...");

    Client::instance().shutdown();

    Sleep(150);

    FreeLibraryAndExitThread(module, 0);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, main_thread, module, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
