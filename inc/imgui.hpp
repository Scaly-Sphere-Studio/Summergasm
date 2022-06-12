#pragma once

#include "Summergasm.hpp"

constexpr extern bool print_demo = false;
extern SSS::GL::Window::Shared ui_window;

void SetCursor(SSS::GL::Window::Shared window, int shape);

void Tooltip(char const* description);

template <typename _Func, typename ...Args>
bool Tooltip(char const* description, _Func f, Args ...args)
{
    bool ret = f(args...);
    Tooltip(description);
    return ret;
}

bool InputFloatWasEdited(const char* label, float* v, float step = 0.0f,
    float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0);

bool StringButtonEdit(char const* label, std::string& str);

template<typename _T>
bool MapIDCombo(char const* label, std::map<uint32_t, _T> const& map, uint32_t& current_id)
{
    bool ret = false;
    if (ImGui::BeginCombo(label, std::to_string(current_id).c_str())) {
        // Loop over map to display each ID
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            uint32_t const id = it->first;
            bool is_selected = (current_id == id);
            // Display selectable ID
            if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                current_id = id;
                ret = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return ret;
}

// Default, deleted
template<class _Object>
void print_object(std::unique_ptr<_Object> const& ptr) = delete;

// Default, deleted
template<typename _Object>
void create_object(uint32_t id) = delete;

// Default, deleted
template<typename _Object>
void remove_object(uint32_t id) = delete;

template<class _Object>
void print_objects(std::map<uint32_t, std::unique_ptr<_Object>> const& map)
{
    // Tabs, used to display a single camera UI, along
    // with creation & deletion of cameras
    if (ImGui::BeginTabBar("###"))
    {
        // Retrieve all IDs
        std::vector<uint32_t> ids;
        ids.reserve(map.size());
        for (auto it = map.cbegin(); it != map.cend(); it++) {
            ids.push_back(it->first);
        }
        // Display all IDs as tabs
        for (size_t n = 0; n < ids.size(); n++)
        {
            // Option to close tab
            bool open = true;
            char label[32];
            sprintf_s(label, "%04u", ids[n]);
            // Display tab
            if (ImGui::BeginTabItem(label, &open)) {
                // Tab is active, display object UI
                print_object(map.at(ids[n]));
                ImGui::EndTabItem();
            }
            if (!open) {
                // Tab was closed, delete object
                remove_object<_Object>(ids[n]);
            }
        }
        // Bool to set focus on next popup
        static bool set_focus = false;
        // + Button to create new object
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            ImGui::OpenPopup("###");
            set_focus = true;
        }
        // Popup to prompt for new ID
        if (ImGui::BeginPopup("###")) {
            int id = ids.empty() ? 0 : ids.back() + 1;
            if (set_focus) {
                ImGui::SetKeyboardFocusHere(0);
                set_focus = false;
            }
            ImGui::SetNextItemWidth(100.f);
            ImGui::InputInt("ID", &id, 0);
            // If an ID was specified, create object
            if (ImGui::IsItemDeactivated()) {
                uint32_t uid = static_cast<uint32_t>(id);
                // Only create if available ID
                if (map.count(uid) == 0)
                    create_object<_Object>(uid);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndTabBar();
    }
}

void print_window_objects();