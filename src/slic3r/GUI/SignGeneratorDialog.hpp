#ifndef slic3r_GUI_SignGeneratorDialog_hpp_
#define slic3r_GUI_SignGeneratorDialog_hpp_

#include "Widgets/WebViewHostDialog.hpp"

#include <string>

namespace Slic3r { namespace GUI {

// BukvaSlicer: окно «Генератор вывесок». Страница resources/web/sign_generator/index.html
// строит объёмные буквы из SVG и присылает готовый 3MF через window.wx.postMessage,
// а диалог кладёт его на текущий стол.
class SignGeneratorDialog : public WebViewHostDialog
{
public:
    // Открывает окно или поднимает уже открытое: генератор держит свои настройки, пока окно живо.
    static void open(wxWindow* parent);

private:
    explicit SignGeneratorDialog(wxWindow* parent);
    ~SignGeneratorDialog() override;

    void on_script_message(const nlohmann::json& payload) override;
    bool append_language_to_url() const override { return false; }
    void load_3mf(const std::string& name, const std::string& base64);
    void reply(const std::string& status, const std::string& text);
};

}} // namespace Slic3r::GUI

#endif
