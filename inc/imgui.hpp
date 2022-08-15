#pragma once

#include "Summergasm.hpp"

extern SSS::GL::Window::Shared ui_window;

void SetCursor(SSS::GL::Window::Shared window, int shape);

template <typename ...Args>
void TextCentered(char const* text, Args ...args)
{
    float windowWidth = ImGui::GetWindowSize().x;
    char buff[4096];
    sprintf_s(buff, text, args...);
    float textWidth = ImGui::CalcTextSize(buff).x;

    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::Text(buff);
}

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
void print_object(_Object const& ptr) = delete;

// Default, deleted
template<typename _Object>
void create_object(uint32_t id) = delete;

// Default, deleted
template<typename _Object>
void remove_object(uint32_t id) = delete;

template<typename _Object, class Container>
std::vector<uint32_t> get_ids(Container const& container) = delete;

template<typename _Object>
std::vector<uint32_t> get_ids(std::map<uint32_t, std::unique_ptr<_Object>> const& map)
{
    // Retrieve all IDs
    std::vector<uint32_t> ids;
    ids.reserve(map.size());
    for (auto it = map.cbegin(); it != map.cend(); it++) {
        ids.push_back(it->first);
    }
    return ids;
}

template<typename _Object, size_t size>
std::vector<uint32_t> get_ids(std::array<std::unique_ptr<_Object>, size> const& arr)
{
    // Retrieve all IDs
    std::vector<uint32_t> ids;
    ids.reserve(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i])
            ids.push_back(static_cast<uint32_t>(i));
    }
    return ids;
}

template<typename _Object>
std::vector<uint32_t> get_ids(std::vector<std::shared_ptr<_Object>> const& vec)
{
    // Retrieve all IDs
    std::vector<uint32_t> ids;
    ids.reserve(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        ids.push_back(static_cast<uint32_t>(i));
    }
    return ids;
}

template<class _Object, class Container>
void print_objects(Container const& container)
{
    // Tabs, used to display a single camera UI, along
    // with creation & deletion of cameras
    if (ImGui::BeginTabBar("###"))
    {
        // Retrieve all IDs
        std::vector<uint32_t> ids = get_ids(container);
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
                print_object(container.at(ids[n]));
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
                create_object<_Object>(uid);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndTabBar();
    }
}

void print_window_objects();
void print_audio();