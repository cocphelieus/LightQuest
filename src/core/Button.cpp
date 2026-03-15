#include "Button.h"
#include <SDL2/SDL_image.h>

// Khởi tạo button: lưu vị trí, kích thước và đường dẫn ảnh.
// baseRect là vùng gốc không đổi, rect là vùng thực tế sau khi scale hover.
Button::Button(int x, int y, int width, int height, const std::string& imagePath)
    : imagePath(imagePath) {
    baseRect.x = x;
    baseRect.y = y;
    baseRect.w = width;
    baseRect.h = height;

    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
}

// Hủy button: giải phóng texture khi đối tượng bị xóa.
Button::~Button() {
    clean();
}

// Tải ảnh button từ file lên GPU thông qua renderer.
// Trả về false nếu file không tồn tại hoặc lỗi.
bool Button::load(SDL_Renderer* renderer) {

    texture = IMG_LoadTexture(renderer, imagePath.c_str());

    if (!texture) {
        SDL_Log("Failed to load button: %s - %s",
                imagePath.c_str(),
                IMG_GetError());
        return false;
    }

    return true;
}

// Vẽ button lên màn hình. Khi hover, scale lên 1.08x từ tâm để tạo hiệu ứng sống động.
void Button::render(SDL_Renderer* renderer) {
    if (texture) {
        // Tính kích thước mới sau scale rồi căn giữa so với baseRect.
        int scaledW = static_cast<int>(static_cast<float>(baseRect.w) * hoverScale);
        int scaledH = static_cast<int>(static_cast<float>(baseRect.h) * hoverScale);
        rect.w = scaledW;
        rect.h = scaledH;
        rect.x = baseRect.x + (baseRect.w - scaledW) / 2;
        rect.y = baseRect.y + (baseRect.h - scaledH) / 2;
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
    }
}

// Cập nhật trạng thái hover mỗi frame dựa trên vị trí chuột.
// Dùng lerp (nội suy tuyến tính) để scale mượt mà thay vì nhảy cóc.
void Button::update(int mouseX, int mouseY) {
    isHovered = (mouseX >= baseRect.x && mouseX <= baseRect.x + baseRect.w &&
                 mouseY >= baseRect.y && mouseY <= baseRect.y + baseRect.h);

    float targetScale = isHovered ? 1.08f : 1.0f;
    float speed = 0.20f;
    hoverScale += (targetScale - hoverScale) * speed;
}

// Giải phóng texture SDL khi button không còn dùng nữa.
void Button::clean() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

// Kiểm tra click: so sánh tọa độ chuột với baseRect (không dùng rect đã scale
// để tránh vùng click bị lệch khi hover).
bool Button::isClicked(int mouseX, int mouseY) const {
    return mouseX >= baseRect.x && mouseX <= baseRect.x + baseRect.w &&
           mouseY >= baseRect.y && mouseY <= baseRect.y + baseRect.h;
}

// Đặt lại vị trí button (cả baseRect lẫn rect để đồng bộ).
void Button::setPosition(int x, int y) {
    baseRect.x = x;
    baseRect.y = y;
    rect.x = x;
    rect.y = y;
}
