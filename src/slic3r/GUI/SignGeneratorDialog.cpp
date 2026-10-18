#include "SignGeneratorDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#include <wx/base64.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <algorithm>
#include <chrono>

namespace Slic3r { namespace GUI {

namespace {
SignGeneratorDialog* g_instance = nullptr;

// Имя файла из страницы: только безопасные символы, без путей.
std::string safe_file_stem(std::string name)
{
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) {
                   return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c < 32;
               }),
               name.end());
    if (name.size() > 80)
        name.resize(80);
    return name.empty() ? std::string("vyveska") : name;
}
} // namespace

void SignGeneratorDialog::open(wxWindow* parent)
{
    if (g_instance == nullptr)
        g_instance = new SignGeneratorDialog(parent);
    g_instance->Show();
    g_instance->Raise();
}

SignGeneratorDialog::SignGeneratorDialog(wxWindow* parent)
    : WebViewHostDialog(parent, wxID_ANY, wxString::FromUTF8("Генератор вывесок"))
{
    if (!create_webview("web/sign_generator/index.html", wxString::FromUTF8("Генератор вывесок"), wxSize(1360, 900), wxSize(960, 640))) {
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8("Для генератора нужен компонент WebView2, он не установлен.")),
                   wxSizerFlags().Border(wxALL, FromDIP(20)));
        SetSizerAndFit(sizer);
    }
    // Окно немодальное: закрытие уничтожает его, следующее открытие создаёт заново.
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) {
        g_instance = nullptr;
        Destroy();
    });
}

SignGeneratorDialog::~SignGeneratorDialog()
{
    if (g_instance == this)
        g_instance = nullptr;
}

void SignGeneratorDialog::on_script_message(const nlohmann::json& payload)
{
    if (!payload.is_object())
        return;
    if (handle_common_script_command(payload))
        return;
    const std::string command = payload.value("command", "");
    if (command == "load_3mf") {
        const std::string name = payload.value("name", "vyveska");
        const std::string data = payload.value("data", "");
        // Скриптовое сообщение может прийти внутри нативного колбэка WebView — работаем после него.
        wxGetApp().CallAfter([this, name, data] {
            if (g_instance == this) // окно могли закрыть, пока сообщение шло
                load_3mf(name, data);
        });
    }
}

void SignGeneratorDialog::load_3mf(const std::string& name, const std::string& base64)
{
    namespace fs = boost::filesystem;
    if (base64.empty()) {
        reply("error", std::string("Страница прислала пустой файл."));
        return;
    }
    const wxMemoryBuffer bytes = wxBase64Decode(base64.c_str(), base64.size());
    if (bytes.GetDataLen() == 0) {
        reply("error", std::string("Не удалось разобрать файл со страницы."));
        return;
    }

    try {
        const fs::path dir = fs::temp_directory_path() / "BukvaSlicer";
        fs::create_directories(dir);
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch()).count();
        const fs::path file = dir / (safe_file_stem(name) + "_" + std::to_string(stamp) + ".3mf");
        {
            boost::nowide::ofstream out(file.string(), std::ios::binary);
            out.write(static_cast<const char*>(bytes.GetData()), std::streamsize(bytes.GetDataLen()));
            if (!out) {
                reply("error", std::string("Не удалось записать временный файл."));
                return;
            }
        }

        Plater* plater = wxGetApp().plater();
        if (plater == nullptr) {
            reply("error", std::string("Стол слайсера ещё не готов."));
            return;
        }
        // Только модель: пресеты принтера и пластиков остаются те, что выбраны в слайсере.
        const std::vector<size_t> loaded = plater->load_files(std::vector<fs::path>{file}, LoadStrategy::LoadModel);
        if (loaded.empty()) {
            reply("error", std::string("Слайсер не смог загрузить модель."));
            return;
        }
        wxGetApp().mainframe->select_tab(wxString(TAB_ID_PREPARE));
        reply("ok", std::string("Буквы добавлены на стол."));
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "SignGeneratorDialog: " << e.what();
        reply("error", e.what());
    }
}

void SignGeneratorDialog::reply(const std::string& status, const std::string& text)
{
    call_web_handler(nlohmann::json{{"command", "load_result"}, {"status", status}, {"message", text}}, wxT("window.onSlicerReply"));
}

}} // namespace Slic3r::GUI
