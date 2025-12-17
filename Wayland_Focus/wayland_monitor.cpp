#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <gio/gio.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>

struct WindowInfo {
    std::string appId;
    std::string title;
    bool isActive;
};

class WaylandWindowMonitor {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* fontSmall;
    std::vector<WindowInfo> windows;
    GDBusConnection* dbusConnection;
    bool running;

    std::string exec_command(const char* cmd) {
        char buffer[128];
        std::string result;
        FILE* pipe = popen(cmd, "r");
        if (!pipe) return "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }

    bool tryExtensionDBus() {
        if (!dbusConnection) return false;

        GError* error = nullptr;
        GVariant* result = g_dbus_connection_call_sync(
            dbusConnection,
            "org.gnome.Shell",
            "/org/focus/WindowIntrospect",
            "org.focus.WindowIntrospect",
            "GetWindows",
            nullptr,
            G_VARIANT_TYPE("(s)"),
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            nullptr,
            &error
        );

        if (error) {
            g_error_free(error);
            return false;
        }

        if (result) {
            const gchar* json_str;
            g_variant_get(result, "(&s)", &json_str);
            parseExtensionJson(json_str);
            g_variant_unref(result);
            return !windows.empty();
        }

        return false;
    }

    void parseExtensionJson(const char* json_str) {
        std::string json(json_str);

        size_t pos = 0;
        while ((pos = json.find("{\"title\"", pos)) != std::string::npos) {
            size_t end = json.find("}", pos);
            if (end == std::string::npos) break;

            std::string obj = json.substr(pos, end - pos + 1);
            WindowInfo info;

            size_t title_start = obj.find("\"title\":\"");
            if (title_start != std::string::npos) {
                title_start += 9;
                size_t title_end = obj.find("\"", title_start);
                if (title_end != std::string::npos) {
                    info.title = obj.substr(title_start, title_end - title_start);
                }
            }

            size_t appid_start = obj.find("\"appId\":\"");
            if (appid_start != std::string::npos) {
                appid_start += 9;
                size_t appid_end = obj.find("\"", appid_start);
                if (appid_end != std::string::npos) {
                    info.appId = obj.substr(appid_start, appid_end - appid_start);
                    size_t desktop_pos = info.appId.find(".desktop");
                    if (desktop_pos != std::string::npos) {
                        info.appId = info.appId.substr(0, desktop_pos);
                    }
                }
            }

            if (info.appId.empty()) {
                size_t wm_start = obj.find("\"wmClass\":\"");
                if (wm_start != std::string::npos) {
                    wm_start += 11;
                    size_t wm_end = obj.find("\"", wm_start);
                    if (wm_end != std::string::npos) {
                        info.appId = obj.substr(wm_start, wm_end - wm_start);
                    }
                }
            }

            info.isActive = obj.find("\"hasFocus\":true") != std::string::npos;

            if (!info.title.empty() || !info.appId.empty()) {
                windows.push_back(info);
            }

            pos = end + 1;
        }
    }

    void tryGnomeIntrospect() {
        if (!dbusConnection) return;

        GError* error = nullptr;
        GVariant* result = g_dbus_connection_call_sync(
            dbusConnection,
            "org.gnome.Shell.Introspect",
            "/org/gnome/Shell/Introspect",
            "org.gnome.Shell.Introspect",
            "GetWindows",
            nullptr,
            G_VARIANT_TYPE("(a{ta{sv}})"),
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            nullptr,
            &error
        );

        if (error) {
            g_error_free(error);
            return;
        }

        if (result) {
            GVariantIter* iter;
            g_variant_get(result, "(a{ta{sv}})", &iter);

            guint64 window_id;
            GVariantIter* props_iter;

            while (g_variant_iter_loop(iter, "{ta{sv}}", &window_id, &props_iter)) {
                WindowInfo info;
                const gchar* key;
                GVariant* value;

                while (g_variant_iter_loop(props_iter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "title") == 0 && g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
                        info.title = g_variant_get_string(value, nullptr);
                    } else if (g_strcmp0(key, "app-id") == 0 && g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
                        info.appId = g_variant_get_string(value, nullptr);
                        size_t pos = info.appId.find(".desktop");
                        if (pos != std::string::npos) {
                            info.appId = info.appId.substr(0, pos);
                        }
                    } else if (g_strcmp0(key, "wm-class") == 0 && g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
                        if (info.appId.empty()) {
                            info.appId = g_variant_get_string(value, nullptr);
                        }
                    } else if (g_strcmp0(key, "has-focus") == 0 && g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                        info.isActive = g_variant_get_boolean(value);
                    }
                }

                if (!info.title.empty() || !info.appId.empty()) {
                    windows.push_back(info);
                }
            }

            g_variant_iter_free(iter);
            g_variant_unref(result);
        }
    }

    void updateWindowList() {
        windows.clear();

        if (tryExtensionDBus()) {
            // Got windows from extension
        } else {
            tryGnomeIntrospect();
        }

        // Sort: active window first
        std::sort(windows.begin(), windows.end(), [](const WindowInfo& a, const WindowInfo& b) {
            return a.isActive > b.isActive;
        });
    }

    void renderText(const char* text, int x, int y, SDL_Color color, TTF_Font* f) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(f, text, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_Rect dst = {x, y, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }

    void render() {
        // Clear with dark gray background
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderClear(renderer);

        int y = 20;
        const int rowHeight = 60;
        const int padding = 10;

        SDL_Color white = {255, 255, 255, 255};
        SDL_Color cyan = {0, 255, 255, 255};
        SDL_Color gray = {180, 180, 180, 255};

        for (const auto& win : windows) {
            if (win.isActive) {
                // Black background for active window
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_Rect rect = {10, y - 5, 480, rowHeight};
                SDL_RenderFillRect(renderer, &rect);

                // Cyan text for active window
                std::string appText = "> " + (win.appId.empty() ? "Unknown" : win.appId);
                renderText(appText.c_str(), 20, y, cyan, font);

                std::string title = win.title;
                if (title.length() > 50) title = title.substr(0, 47) + "...";
                renderText(title.c_str(), 20, y + 25, cyan, fontSmall);
            } else {
                // White/gray text for inactive windows
                std::string appText = "  " + (win.appId.empty() ? "Unknown" : win.appId);
                renderText(appText.c_str(), 20, y, white, font);

                std::string title = win.title;
                if (title.length() > 50) title = title.substr(0, 47) + "...";
                renderText(title.c_str(), 20, y + 25, gray, fontSmall);
            }

            y += rowHeight;
        }

        if (windows.empty()) {
            renderText("No windows detected", 20, y, gray, font);
            renderText("Make sure the GNOME extension is installed", 20, y + 30, gray, fontSmall);
        }

        SDL_RenderPresent(renderer);
    }

public:
    WaylandWindowMonitor() : window(nullptr), renderer(nullptr), font(nullptr), fontSmall(nullptr), dbusConnection(nullptr), running(true) {
        // Connect to D-Bus
        GError* error = nullptr;
        dbusConnection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (error) {
            fprintf(stderr, "D-Bus error: %s\n", error->message);
            g_error_free(error);
        }

        // Init SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
            running = false;
            return;
        }

        if (TTF_Init() < 0) {
            fprintf(stderr, "TTF init failed: %s\n", TTF_GetError());
            running = false;
            return;
        }

        window = SDL_CreateWindow("Window Monitor",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            500, 450, SDL_WINDOW_SHOWN);
        if (!window) {
            fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
            running = false;
            return;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
            running = false;
            return;
        }

        // Try common font paths
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
            "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
            nullptr
        };

        for (const char** path = fontPaths; *path && !font; path++) {
            font = TTF_OpenFont(*path, 18);
            if (font) {
                fontSmall = TTF_OpenFont(*path, 14);
            }
        }

        if (!font) {
            fprintf(stderr, "Could not load any font\n");
            running = false;
            return;
        }
    }

    ~WaylandWindowMonitor() {
        if (fontSmall) TTF_CloseFont(fontSmall);
        if (font) TTF_CloseFont(font);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (dbusConnection) g_object_unref(dbusConnection);
        TTF_Quit();
        SDL_Quit();
    }

    void run() {
        Uint32 lastUpdate = 0;

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }

            Uint32 now = SDL_GetTicks();
            if (now - lastUpdate > 1000) {
                updateWindowList();
                lastUpdate = now;
            }

            render();
            SDL_Delay(16); // ~60 FPS
        }
    }
};

int main(int argc, char** argv) {
    WaylandWindowMonitor monitor;
    monitor.run();
    return 0;
}
