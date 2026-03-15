#include "Window.h"
#include <SDL2/SDL_image.h>

// Khởi tạo cửa sổ và renderer SDL.
// Đặt logical size cố định 1280x720 để toàn bộ scene không cần quan tâm
// đến kích thước cửa sổ thực tế - SDL tự scale.
bool Window::init(const char* title, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return false;

    logicalWidth = w;
    logicalHeight = h;

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    // Gán icon cửa sổ từ file ảnh logo (thử nhiều tên file phòng trường hợp thiếu).
    if (window)
    {
        SDL_Surface* iconSurface = IMG_Load("assets/images/entities/logo_new.png");
        if (!iconSurface)
            iconSurface = IMG_Load("assets/images/entities/logo_256.png");
        if (!iconSurface)
            iconSurface = IMG_Load("assets/images/entities/logo.ico");

        if (iconSurface)
        {
            SDL_SetWindowIcon(window, iconSurface);
            SDL_FreeSurface(iconSurface);
        }
    }

    // Tạo renderer tăng tốc phần cứng, bật VSync để tránh xé hình.
    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!window || !renderer)
        return false;

    // Bật lọc ảnh tuyến tính để texture scale trông mượt hơn.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(renderer, logicalWidth, logicalHeight);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

    return true;
}

// Xóa frame buffer bằng màu đen trước khi vẽ frame mới.
void Window::clear() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

// Trình bày frame đã vẽ lên màn hình (swap buffer).
void Window::present() {
    SDL_RenderPresent(renderer);
}

// Trả về renderer dùng chung cho tất cả các scene.
SDL_Renderer* Window::getRenderer() {
    return renderer;
}

// Chuyển đổi giữa fullscreen và windowed.
// Khi về windowed, khôi phục về đúng kích thước logical và căn giữa màn hình.
void Window::toggleFullscreen()
{
    if (!window)
        return;

    if (!isFullscreen)
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        isFullscreen = true;
    }
    else
    {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, logicalWidth, logicalHeight);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        isFullscreen = false;
    }
}

void Window::clean() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
