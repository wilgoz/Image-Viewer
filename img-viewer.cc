#include <SDL2/SDL_image.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

template<typename Fn>
static inline void handle_error(const std::string& msg, Fn fn)
{
    fprintf(stderr, "%s err: %s\n", msg.c_str(), fn());
    exit(EXIT_FAILURE);
}

/**
 * Class responsible for initializing screen properties
 * Initializes the window & renderer
 */
class Screen
{
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer_
    {
        nullptr, SDL_DestroyRenderer
    };
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window_
    {
        nullptr, SDL_DestroyWindow
    };

public:
    unsigned width = 0, height = 0;

    Screen(const std::string& title, unsigned w, unsigned h)
        : width  (w)
        , height (h)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
            handle_error("video init", SDL_GetError);
        window_.reset(
            SDL_CreateWindow(title.c_str(),
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             width, height,
                             SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));
        if (!window_)
            handle_error("create window", SDL_GetError);
        renderer_.reset(
            SDL_CreateRenderer(window_.get(),
                               -1,
                               SDL_RENDERER_ACCELERATED));
        if (!renderer_)
            handle_error("create renderer", SDL_GetError);
    }

    // Manual destruction order: renderer -> window
    ~Screen()
    {
        if (renderer_) renderer_.reset();
        if (window_) window_.reset();
        SDL_Quit();
    }

    inline SDL_Renderer* get_renderer_mutable() const { return renderer_.get(); }

    inline void set_title(const std::string& title)
    {
        if (!title.empty())
            SDL_SetWindowTitle(window_.get(), title.c_str());
    }
};

/**
 * Class responsible for loading, rendering & rolling images
 */
class Image
{
    int roll_ix_ = 0, prev_ix_ = -1;

    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_
    {
        nullptr, SDL_DestroyTexture
    };
    std::vector<std::string> images_;

public:
    enum class RollFlag { IMG_ROLL_REFR, IMG_ROLL_NEXT, IMG_ROLL_PREV };

    Image(const std::vector<std::string>& imgs) : images_(imgs)
    {
        if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
            handle_error("img init", IMG_GetError);
    }

    ~Image() { IMG_Quit(); }

    // Gets the image name of the current roll index
    inline std::string get_name() const
        { return images_.size() ? fs::path(images_[roll_ix_]).filename() : ""; }

    // Resets current image roll with images of the dropped file's directory
    void reset_roll(const std::string& file)
    {
        int old_roll = roll_ix_, old_prev = prev_ix_, iter = 0;
        roll_ix_ = 0; prev_ix_ = -1;

        std::vector<std::string> files;
        std::string dir = fs::is_directory(file)
                            ? file
                            : fs::path(file)
                                .parent_path()
                                .string();

        for (const auto& p: fs::directory_iterator(dir))
            if (p.path().extension() == ".png")
            {
                files.push_back(p.path());
                if (p.path() == file) roll_ix_ = iter;
                ++iter;
            }
        if (files.size())
            { images_  = std::move(files); }
        else
            { roll_ix_ = old_roll; prev_ix_ = old_prev; }
    }

    // Loads & renders image on specific roll flags. Simply renders current image by default
    void roll_image(SDL_Renderer* renderer, int screen_w, int screen_h, RollFlag flag)
    {
        if (!images_.size()) return;
        switch (flag)
        {
        case RollFlag::IMG_ROLL_PREV: roll_ix_ = ((roll_ix_ - 1) + images_.size()) % images_.size(); break;
        case RollFlag::IMG_ROLL_NEXT: roll_ix_ =  (roll_ix_ + 1) % images_.size(); break;
        case RollFlag::IMG_ROLL_REFR:
        default: break;
        }
        // Only loads images on dirty rolls
        if (prev_ix_ != roll_ix_)
        {
            texture_.reset(load_image(renderer));
            prev_ix_ = roll_ix_;
        }
        render_image(renderer, screen_w, screen_h);
    }

private:
    void render_image(SDL_Renderer* renderer, int screen_w, int screen_h)
    {
        SDL_Rect pos;
        SDL_RenderClear(renderer);
        SDL_QueryTexture(texture_.get(), 0, 0, &pos.w, &pos.h);
        // Clamps image to fit screen res
        if (pos.w > screen_w || pos.h > screen_h)
        {
            double scale_factor =
                std::max( static_cast<double>(pos.w) / screen_w,
                          static_cast<double>(pos.h) / screen_h );
            pos.w /= scale_factor;
            pos.h /= scale_factor;
        }
        // Renders image to center
        pos.x = (screen_w - pos.w) / 2;
        pos.y = (screen_h - pos.h) / 2;
        SDL_RenderCopy(renderer, texture_.get(), 0, &pos);
        SDL_RenderPresent(renderer);
    }

    SDL_Texture* load_image(SDL_Renderer* renderer)
    {
        const char* path = images_[roll_ix_].c_str();
        SDL_Texture* texture = IMG_LoadTexture(renderer, path);
        if (!texture)
            handle_error(path, SDL_GetError);
        return texture;
    }
};

static void event_loop(Screen& screen, Image& img)
{
    using RollFlag = Image::RollFlag;

    bool update_title = false,
         quit = false;

    auto roll_handler = [&](RollFlag flag)
    {
        img.roll_image(screen.get_renderer_mutable(),
                       screen.width,
                       screen.height,
                       flag);
        if (update_title)
            screen.set_title(img.get_name());
    };

    auto window_handler = [&](const SDL_Event& e)
    {
        switch (e.window.event)
        {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            screen.width = e.window.data1; screen.height = e.window.data2;
            roll_handler(RollFlag::IMG_ROLL_REFR);
            update_title = false;
            break;
        case SDL_WINDOWEVENT_SHOWN:
            roll_handler(RollFlag::IMG_ROLL_REFR);
            update_title = true;
            break;
        }
    };

    auto keydown_handler = [&](const SDL_Event& e)
    {
        switch (e.key.keysym.sym)
        {
        case SDLK_LEFT:
            roll_handler(RollFlag::IMG_ROLL_PREV);
            update_title = true;
            break;
        case SDLK_RIGHT:
            roll_handler(RollFlag::IMG_ROLL_NEXT);
            update_title = true;
            break;
        }
    };

    SDL_Event event;
    while (!quit) if (SDL_WaitEvent(&event))
    {
        switch (event.type)
        {
        case SDL_WINDOWEVENT:
            window_handler(event);
            break;
        case SDL_KEYDOWN:
            keydown_handler(event);
            break;
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_DROPFILE:
            img.reset_roll(std::string(event.drop.file));
            roll_handler(RollFlag::IMG_ROLL_REFR);
            update_title = true;
            break;
        }
    }
}

int main(int argc, char const *argv[])
{
    constexpr unsigned WIN_W = 1200;
    constexpr unsigned WIN_H =  900;

    Screen screen("Toy Image Viewer", WIN_W, WIN_H);
    Image img(std::vector<std::string>{ argv + 1, argv + argc });

    event_loop(screen, img);
}