#pragma once

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include <functional>
#include <string>
#include <vector>

using namespace ftxui;

namespace UIComponents {

inline Component createButton(const std::string &label, std::function<void()> on_click,
                              const ButtonOption &options = ButtonOption::Animated()) {
    return Button(label, on_click, options);
}

inline auto createStyledButton(const std::string &label, std::function<void()> on_click,
                               Color text_color = Color::White) {
    auto button = Button(label, on_click, ButtonOption::Animated());
    return Renderer(button, [button, text_color] { return button->Render() | color(text_color); });
}

inline Component createMenu(std::vector<std::string> *entries, int *selected_index,
                            const MenuOption &options = MenuOption()) {
    return Menu(entries, selected_index, options);
}

inline auto createTable(std::vector<std::string> *rows, int *selected_index,
                        bool show_frame = true) {
    auto menu = Menu(rows, selected_index);
    return Renderer(menu, [menu, show_frame] {
        auto rendered = menu->Render() | vscroll_indicator | flex;
        if (show_frame) {
            rendered = rendered | frame | border;
        }
        return rendered;
    });
}

struct ModalActions {
    std::string confirm_label = "Confirm";
    std::string cancel_label = "Cancel";
    std::function<void()> on_confirm = nullptr;
    std::function<void()> on_cancel = nullptr;
};

inline auto createModal(const std::string &title, Component content_component,
                        const ModalActions &actions, bool *is_visible) {

    auto confirm_btn = createButton(actions.confirm_label, actions.on_confirm);
    auto cancel_btn = createButton(actions.cancel_label, [is_visible, &actions] {
        if (actions.on_cancel)
            actions.on_cancel();
        *is_visible = false;
    });

    auto button_container = Container::Horizontal({confirm_btn, cancel_btn});
    auto modal_container = Container::Vertical({content_component, button_container});

    return Renderer(modal_container, [modal_container, title] {
        return vbox(
                   {text(title) | bold | center, separator(), modal_container->Render() | center}) |
               border | center | bgcolor(Color::Black);
    });
}

inline auto createInputField(std::string *value, const std::string &placeholder,
                             const std::string &label = "") {

    auto input = Input(value, placeholder);

    if (label.empty()) {
        return Renderer(input, [input] { return input->Render() | border; });
    }

    return Renderer(input,
                    [input, label] { return vbox({text(label) | dim, input->Render() | border}); });
}

inline Element createStatBox(const std::string &label, const std::string &value,
                             Color value_color = Color::Yellow) {
    return hbox({text(label) | dim, text(value) | color(value_color)});
}

inline Element createGaugeBox(float ratio, const std::string &label,
                              const std::string &percentage_text, Color gauge_color = Color::Blue) {
    return hbox({gauge(ratio) | color(gauge_color) | border,
                 vbox({text(label), text(percentage_text) | dim})});
}

inline Component
createActionGroup(const std::vector<std::pair<std::string, std::function<void()>>> &actions) {
    std::vector<Component> buttons;
    for (const auto &action : actions) {
        buttons.push_back(createButton(action.first, action.second));
    }
    return Container::Vertical(buttons);
}

} // namespace UIComponents
