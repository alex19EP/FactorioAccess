#include "disclosure.h"

#include "agui.h"
#include "game.h"
#include "log.h"
#include "vocab.h"

#include <windows.h>

#include <cstddef>
#include <cwchar>
#include <string>
#include <string_view>

namespace fa::disclosure {

namespace {

using game::layout;

// The version and the log line name the DLL to Wube, so they read the same in every language.
const std::string kVersionSuffix = " with FactorioAccess native " FA_NATIVE_VERSION;
constexpr const char* kLogLine = "FactorioAccess native " FA_NATIVE_VERSION
                                 " is loaded as winmm.dll. Crash log uploading is off; report crashes at "
                                 "https://github.com/Factorio-Access/FactorioAccess/issues";

// LogLevel::Notice: printed without level or source file, like the game's own version line.
constexpr int kLogNotice = 7;

bool g_logged = false;

template <class T>
T& at(std::byte* object, uint32_t offset) {
   return *reinterpret_cast<T*>(object + offset);
}

std::byte* otherSettings() {
   auto* context = *reinterpret_cast<std::byte* const*>(layout.globalContext);
   return context ? at<std::byte*>(context, layout.globalOtherSettings) : nullptr;
}

// The checkbox of Settings > Other that edits `item`, or null while that page is not open.
const agui::Widget* checkboxFor(const std::byte* item) {
   const agui::Widget* window = agui::menuStateWindow();
   if (!window || !agui::derivesFrom(window, "OtherSettingsGui")) return nullptr;
   auto* gui = reinterpret_cast<std::byte*>(const_cast<agui::Widget*>(window));
   // std::vector<std::unique_ptr<BoolGuiSetting>>: first and last.
   auto* first = at<std::byte**>(gui, layout.otherSettingsBools);
   auto* last = at<std::byte**>(gui, layout.otherSettingsBools + sizeof(void*));
   for (std::byte** entry = first; entry != last; ++entry)
      if (at<std::byte*>(*entry, layout.boolSettingItem) == item)
         return reinterpret_cast<const agui::Widget*>(*entry + layout.boolSettingWidget);
   return nullptr;
}

// The main menu's version label is made once per visit; the first one can come before the hooks.
void markVersionLabel() {
   const agui::Widget* label = agui::versionLabel();
   if (!label) return;
   std::string_view text = agui::text(label);
   if (text.empty() || text.ends_with(kVersionSuffix)) return;
   agui::setLabelText(label, std::string(text) + kVersionSuffix);
}

using VersionFunction = void* (*)(const void* version, void* out);
VersionFunction g_versionOriginal = nullptr;

void* versionForDisplay(const void* version, void* out) {
   void* result = g_versionOriginal(version, out);
   reinterpret_cast<void* (*)(void*, const char*, size_t)>(layout.stringAppend)(result, kVersionSuffix.data(),
                                                                                 kVersionSuffix.size());
   return result;
}

} // namespace

void blockLogUploader() {
   // The crashed game starts the uploader with ShellExecute and never reads its exit code, so this
   // one only tells whoever looks that the block worked.
   constexpr UINT kBlockedExitCode = 0xFA;
   if (std::wcsstr(GetCommandLineW(), L"--upload-log-file")) TerminateProcess(GetCurrentProcess(), kBlockedExitCode);
}

void tick() {
   std::byte* settings = otherSettings();
   if (!settings) return;
   std::byte* item = settings + layout.crashLogItem;
   at<bool>(item, layout.configBoolValue) = false;

   if (!g_logged) {
      g_logged = true;
      reinterpret_cast<void (*)(const char*, unsigned, int, const char*, ...)>(layout.loggingLog)(
         __FILE__, __LINE__, kLogNotice, "%s", kLogLine);
      log::info("Crash log uploading is forced off");
   }

   if (const agui::Widget* checkbox = checkboxFor(item); checkbox && agui::enabled(checkbox)) {
      agui::setEnabled(checkbox, false);
      // The game's own description stays first; ours follows it. A one-string tooltip, as the game
      // gives these checkboxes and as setToolTip makes, keeps its text in the title.
      agui::ToolTip vanilla = agui::toolTip(checkbox);
      std::string text(vanilla.title);
      if (!vanilla.text.empty()) text += (text.empty() ? "" : "\n") + std::string(vanilla.text);
      if (!text.empty()) text += "\n\n";
      agui::setToolTip(checkbox, text + std::string(vocab::kCrashLogUploadOff));
   }
   markVersionLabel();
}

void* versionDetour() { return reinterpret_cast<void*>(&versionForDisplay); }
void** versionOriginal() { return reinterpret_cast<void**>(&g_versionOriginal); }

} // namespace fa::disclosure
