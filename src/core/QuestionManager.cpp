#include "QuestionManager.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <vector>

#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#elif __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#else
#define LIGHTQUEST_HAS_SDL_TTF 0
#endif

#include <SDL2/SDL_image.h>

namespace
{
    // Header nhận dạng định dạng file câu hỏi (khóa/plain).
    const char* kQuestionLockHeader = "LQLOCK1";
    const char* kQuestionPlainHeader = "LQPLAIN1";
    // Khóa XOR dùng để giải mã file câu hỏi đã mã hóa (format LQLOCK1).
    const char* kQuestionLockKey = "LQ_2026_PlayerLock";

    // Câu hỏi dự phòng khi không load được file câu hỏi.
    // Đảm bảo game vẫn chạy được dù thiếu data.
    Question makeFallbackQuestion()
    {
        Question q;
        q.text = "Fallback question: Choose the correct answer to unlock the torch.";
        q.optA = "2 + 2 = 3";
        q.optB = "2 + 2 = 4";
        q.optC = "2 + 2 = 5";
        q.optD = "2 + 2 = 6";
        q.answer = 'B';
        return q;
    }

    bool startsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    // Xóa BOM UTF-8 (3 byte đầu tiên EF BB BF) nếu có.
    // Cần thiết vì file có thể được lưu bằng Notepad với BOM.
    void stripUtf8Bom(std::string& text)
    {
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }
    }

    // Các hàm hỗ trợ decoding hex và XOR transform
    // để giải mã nội dung câu hỏi đã được mã hóa.
    int hexToNibble(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    // Parse chuỗi hex thành mảng byte thực sự.
    bool decodeHex(const std::string& hex, std::string& outBytes)
    {
        if (hex.size() % 2 != 0)
            return false;

        outBytes.clear();
        outBytes.reserve(hex.size() / 2);

        for (size_t i = 0; i < hex.size(); i += 2)
        {
            int hi = hexToNibble(hex[i]);
            int lo = hexToNibble(hex[i + 1]);
            if (hi < 0 || lo < 0)
                return false;

            outBytes.push_back(static_cast<char>((hi << 4) | lo));
        }

        return true;
    }

    // XOR từng byte của data với key (lặp vòng). Dùng để mã hóa và giải mã.
    std::string xorTransform(const std::string& data, const std::string& key)
    {
        if (key.empty())
            return data;

        std::string output = data;
        for (size_t i = 0; i < output.size(); ++i)
            output[i] = static_cast<char>(output[i] ^ key[i % key.size()]);

        return output;
    }

    // Parse văn bản thô thành danh sách câu hỏi theo định dạng:
    // dòng 1: câu hỏi | 4 dòng tiếp: A B C D | dòng cuối: đáp án
    bool parseQuestionsFromText(const std::string& text, std::vector<Question>& questions)
    {
        std::istringstream in(text);
        std::string line;

        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                continue;

            Question q;
            q.text = line;

            if (!std::getline(in, q.optA)) break;
            if (!std::getline(in, q.optB)) break;
            if (!std::getline(in, q.optC)) break;
            if (!std::getline(in, q.optD)) break;

            if (!q.optA.empty() && q.optA.back() == '\r') q.optA.pop_back();
            if (!q.optB.empty() && q.optB.back() == '\r') q.optB.pop_back();
            if (!q.optC.empty() && q.optC.back() == '\r') q.optC.pop_back();
            if (!q.optD.empty() && q.optD.back() == '\r') q.optD.pop_back();

            if (!std::getline(in, line)) break;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.empty())
                q.answer = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));
            else
                q.answer = 'A';

            if (q.answer < 'A' || q.answer > 'D')
                q.answer = 'A';

            questions.push_back(q);
        }

        return !questions.empty();
    }

    // Kiểm tra header file và giải mã nếu là format LQLOCK1.
    // Nếu là plain text (LQPLAIN1) hoặc không có header, giữ nguyên.
    bool tryUnlockQuestionText(std::string& text)
    {
        stripUtf8Bom(text);
        std::string lockPrefixLf = std::string(kQuestionLockHeader) + "\n";
        std::string lockPrefixCrlf = std::string(kQuestionLockHeader) + "\r\n";
        std::string lockPrefixLegacy = std::string(kQuestionLockHeader) + "`n";

        size_t lockPrefixSize = 0;
        if (startsWith(text, lockPrefixCrlf))
            lockPrefixSize = lockPrefixCrlf.size();
        else if (startsWith(text, lockPrefixLf))
            lockPrefixSize = lockPrefixLf.size();
        else if (startsWith(text, lockPrefixLegacy))
            lockPrefixSize = lockPrefixLegacy.size();
        else
            return true;

        std::string hexCipher = text.substr(lockPrefixSize);
        while (!hexCipher.empty() && (hexCipher.back() == '\n' || hexCipher.back() == '\r'))
            hexCipher.pop_back();

        std::string cipherBytes;
        if (!decodeHex(hexCipher, cipherBytes))
            return false;

        std::string plain = xorTransform(cipherBytes, kQuestionLockKey);
        std::string plainPrefixLf = std::string(kQuestionPlainHeader) + "\n";
        std::string plainPrefixCrlf = std::string(kQuestionPlainHeader) + "\r\n";
        std::string plainPrefixLegacy = std::string(kQuestionPlainHeader) + "`n";

        if (startsWith(plain, plainPrefixCrlf))
            text = plain.substr(plainPrefixCrlf.size());
        else if (startsWith(plain, plainPrefixLf))
            text = plain.substr(plainPrefixLf.size());
        else if (startsWith(plain, plainPrefixLegacy))
            text = plain.substr(plainPrefixLegacy.size());
        else
            return false;

        return true;
    }

#if LIGHTQUEST_HAS_SDL_TTF
    SDL_Texture* createWrappedTextTexture(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color, int wrapLength, int& outW, int& outH)
    {
        outW = 0;
        outH = 0;
        if (!renderer || !font || text.empty())
            return nullptr;

        SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, static_cast<Uint32>(wrapLength));
        if (!surface)
            return nullptr;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture)
        {
            outW = surface->w;
            outH = surface->h;
        }

        SDL_FreeSurface(surface);
        return texture;
    }

    void drawWrappedCenteredInRect(
        SDL_Renderer* renderer,
        TTF_Font* font,
        const std::string& text,
        SDL_Color color,
        const SDL_Rect& rect,
        int wrapLength,
        int topBias = 0)
    {
        int textW = 0;
        int textH = 0;
        SDL_Texture* texture = createWrappedTextTexture(renderer, font, text, color, wrapLength, textW, textH);
        if (!texture)
            return;

        int drawX = rect.x + (rect.w - textW) / 2;
        int drawY = rect.y + (rect.h - textH) / 2 + topBias;
        if (drawX < rect.x + 4)
            drawX = rect.x + 4;
        if (drawY < rect.y + 4)
            drawY = rect.y + 4;

        SDL_Rect dst = {drawX, drawY, textW, textH};
        SDL_RenderSetClipRect(renderer, &rect);
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_RenderSetClipRect(renderer, nullptr);
        SDL_DestroyTexture(texture);
    }

    void drawWrappedLeftInRect(
        SDL_Renderer* renderer,
        TTF_Font* font,
        const std::string& text,
        SDL_Color color,
        const SDL_Rect& rect,
        int wrapLength,
        int topBias = 0)
    {
        int textW = 0;
        int textH = 0;
        SDL_Texture* texture = createWrappedTextTexture(renderer, font, text, color, wrapLength, textW, textH);
        if (!texture)
            return;

        int drawX = rect.x + 6;
        int drawY = rect.y + (rect.h - textH) / 2 + topBias;
        if (drawY < rect.y + 4)
            drawY = rect.y + 4;

        SDL_Rect dst = {drawX, drawY, textW, textH};
        SDL_RenderSetClipRect(renderer, &rect);
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_RenderSetClipRect(renderer, nullptr);
        SDL_DestroyTexture(texture);
    }
#endif
}

// Đọc file data/question.txt, giải mã nếu cần, parse thành danh sách câu hỏi.
// Nếu thất bại, đặt 1 câu hỏi dự phòng để game không bị bloc.
bool QuestionManager::loadFromFile(const char* path)
{
    questions.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        questions.push_back(makeFallbackQuestion());
        return false;
    }

    std::string rawText((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (!tryUnlockQuestionText(rawText))
    {
        questions.push_back(makeFallbackQuestion());
        return false;
    }

    parseQuestionsFromText(rawText, questions);

    if (questions.empty())
        questions.push_back(makeFallbackQuestion());

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    return true;
}

// Chọn ngẫu nhiên chỉ số câu hỏi từ danh sách.
int QuestionManager::randomIndex() const
{
    if (questions.empty()) return -1;
    return std::rand() % static_cast<int>(questions.size());
}

// Hiển thị overlay câu hỏi trắc nghiệm, chờ player chọn đáp án.
// Trả về true nếu player chọn đúng (dùng để quyết định torch có bật không).
bool QuestionManager::askQuestion(int index, SDL_Renderer* renderer, bool showCorrectAnswer)
{
    if (index < 0 || index >= static_cast<int>(questions.size())) return false;
    const Question &q = questions[index];

    if (!renderer)
        return false;

#if !LIGHTQUEST_HAS_SDL_TTF
    std::ostringstream oss;
    oss << q.text << "\n\n";
    oss << "A: " << q.optA << "\n";
    oss << "B: " << q.optB << "\n";
    oss << "C: " << q.optC << "\n";
    oss << "D: " << q.optD << "\n";

    if (showCorrectAnswer)
    {
        oss << "\nCorrect answer: " << q.answer << "\n";
    }

    std::string message = oss.str();
    const SDL_MessageBoxButtonData buttons[] = {
        {0, 0, "A"}, {0, 1, "B"}, {0, 2, "C"}, {0, 3, "D"}
    };

    SDL_MessageBoxData boxdata;
    boxdata.flags = SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    boxdata.window = nullptr;
    boxdata.title = "Question";
    boxdata.message = message.c_str();
    boxdata.numbuttons = 4;
    boxdata.buttons = buttons;
    boxdata.colorScheme = nullptr;

    int buttonid = -1;
    if (SDL_ShowMessageBox(&boxdata, &buttonid) < 0)
        return false;

    char chosen = 'A';
    if (buttonid == 1) chosen = 'B';
    else if (buttonid == 2) chosen = 'C';
    else if (buttonid == 3) chosen = 'D';

    return chosen == q.answer;
#else
    bool ttfInitByQuestion = false;
    if (TTF_WasInit() == 0)
    {
        if (TTF_Init() != 0)
            return false;
        ttfInitByQuestion = true;
    }

    TTF_Font* titleFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 30);
    TTF_Font* bodyFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 23);
    TTF_Font* optFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 18);

    if (!titleFont || !bodyFont || !optFont)
    {
        if (titleFont) TTF_CloseFont(titleFont);
        if (bodyFont) TTF_CloseFont(bodyFont);
        if (optFont) TTF_CloseFont(optFont);
        if (ttfInitByQuestion)
            TTF_Quit();
        return false;
    }

    TTF_SetFontStyle(titleFont, TTF_STYLE_BOLD);
    TTF_SetFontStyle(optFont, TTF_STYLE_NORMAL);

    SDL_Texture* backgroundTexture = IMG_LoadTexture(renderer, "assets/images/background/map_level_1.png");
    SDL_Texture* questionBgTexture = IMG_LoadTexture(renderer, "assets/images/background/question_bg.png");
    SDL_Texture* questionFrameTexture = IMG_LoadTexture(renderer, "assets/images/button/khung_cau_hoi.png");
    SDL_Texture* answerFrameTexture = IMG_LoadTexture(renderer, "assets/images/button/khung_dap_an.png");
    SDL_Texture* torchTexture = IMG_LoadTexture(renderer, "assets/images/entities/fire.png");
    SDL_Texture* mineTexture = IMG_LoadTexture(renderer, "assets/images/entities/bom.png");

    int screenW = 0;
    int screenH = 0;
    SDL_RenderGetLogicalSize(renderer, &screenW, &screenH);
    if (screenW <= 0 || screenH <= 0)
    {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }
    if (screenW <= 0 || screenH <= 0)
    {
        screenW = 1280;
        screenH = 720;
    }

    // Crop transparent margins from ornamental frames so visual frame and text areas line up.
    const SDL_Rect questionFrameSrc = {70, 67, 475, 235};
    const SDL_Rect answerFrameSrc = {57, 17, 859, 163};

    SDL_Rect panel = {screenW / 2 - 520, screenH / 2 - 320, 1040, 640};
    if (panel.x < 20) panel.x = 20;
    if (panel.y < 20) panel.y = 20;
    if (panel.w > screenW - 40) panel.w = screenW - 40;
    if (panel.h > screenH - 40) panel.h = screenH - 40;

    int optionSpacingX = 40;
    int optionSpacingY = 26;
    int optionW = (panel.w - 180 - optionSpacingX) / 2;
    if (optionW < 320)
        optionW = 320;
    int optionH = (optionW * answerFrameSrc.h) / answerFrameSrc.w;
    if (optionH < 74)
        optionH = 74;

    int questionW = optionW + 110;
    int questionH = (questionW * questionFrameSrc.h) / questionFrameSrc.w;
    SDL_Rect questionFrameRect = {
        panel.x + (panel.w - questionW) / 2,
        panel.y + 92,
        questionW,
        questionH
    };

    SDL_Rect questionTextRect = {
        questionFrameRect.x + 48,
        questionFrameRect.y + 54,
        questionFrameRect.w - 96,
        questionFrameRect.h - 96
    };

    std::string optionText[4];
    optionText[0] = std::string("A. ") + q.optA;
    optionText[1] = std::string("B. ") + q.optB;
    optionText[2] = std::string("C. ") + q.optC;
    optionText[3] = std::string("D. ") + q.optD;

    SDL_Rect optionRect[4];
    int innerX = panel.x + (panel.w - (optionW * 2 + optionSpacingX)) / 2;
    int innerY = questionFrameRect.y + questionFrameRect.h + 34;
    optionRect[0] = {innerX, innerY, optionW, optionH};
    optionRect[1] = {innerX + optionW + optionSpacingX, innerY, optionW, optionH};
    optionRect[2] = {innerX, innerY + optionH + optionSpacingY, optionW, optionH};
    optionRect[3] = {innerX + optionW + optionSpacingX, innerY + optionH + optionSpacingY, optionW, optionH};

    int hoverIndex = -1;
    int chosenIndex = -1;
    bool waiting = true;
    SDL_Cursor* arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_Cursor* handCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    auto mapToLogical = [renderer, screenW, screenH](int inX, int inY, int& outX, int& outY)
    {
        outX = inX;
        outY = inY;

        if (!renderer || screenW <= 0 || screenH <= 0)
            return;

#if SDL_VERSION_ATLEAST(2, 0, 18)
        float logicalX = static_cast<float>(inX);
        float logicalY = static_cast<float>(inY);
    SDL_RenderWindowToLogical(renderer, inX, inY, &logicalX, &logicalY);
    outX = static_cast<int>(logicalX);
    outY = static_cast<int>(logicalY);
    return;
#endif

        SDL_Rect viewport;
        SDL_RenderGetViewport(renderer, &viewport);
        if (viewport.w <= 0 || viewport.h <= 0)
            return;

        int clampedX = inX;
        int clampedY = inY;
        if (clampedX < viewport.x)
            clampedX = viewport.x;
        if (clampedX > viewport.x + viewport.w)
            clampedX = viewport.x + viewport.w;
        if (clampedY < viewport.y)
            clampedY = viewport.y;
        if (clampedY > viewport.y + viewport.h)
            clampedY = viewport.y + viewport.h;

        outX = ((clampedX - viewport.x) * screenW) / viewport.w;
        outY = ((clampedY - viewport.y) * screenH) / viewport.h;
    };

    while (waiting)
    {
        int mouseX = 0;
        int mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        mapToLogical(mouseX, mouseY, mouseX, mouseY);
        int liveHoverIndex = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mouseX >= optionRect[i].x && mouseX <= optionRect[i].x + optionRect[i].w &&
                mouseY >= optionRect[i].y && mouseY <= optionRect[i].y + optionRect[i].h)
            {
                liveHoverIndex = i;
                break;
            }
        }
        hoverIndex = liveHoverIndex;

        if (hoverIndex >= 0)
        {
            if (handCursor)
                SDL_SetCursor(handCursor);
        }
        else
        {
            if (arrowCursor)
                SDL_SetCursor(arrowCursor);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                waiting = false;
                chosenIndex = -1;
                break;
            }

            if (event.type == SDL_KEYDOWN)
            {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE)
                {
                    chosenIndex = -1;
                    waiting = false;
                }
            }

            if (event.type == SDL_MOUSEMOTION)
            {
                hoverIndex = -1;
                int mx = event.motion.x;
                int my = event.motion.y;
                mapToLogical(mx, my, mx, my);
                for (int i = 0; i < 4; i++)
                {
                    if (mx >= optionRect[i].x && mx <= optionRect[i].x + optionRect[i].w &&
                        my >= optionRect[i].y && my <= optionRect[i].y + optionRect[i].h)
                    {
                        hoverIndex = i;
                        break;
                    }
                }

                if (hoverIndex >= 0)
                {
                    if (handCursor)
                        SDL_SetCursor(handCursor);
                }
                else
                {
                    if (arrowCursor)
                        SDL_SetCursor(arrowCursor);
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                int mx = 0;
                int my = 0;
                SDL_GetMouseState(&mx, &my);
                mapToLogical(mx, my, mx, my);
                for (int i = 0; i < 4; i++)
                {
                    if (mx >= optionRect[i].x && mx <= optionRect[i].x + optionRect[i].w &&
                        my >= optionRect[i].y && my <= optionRect[i].y + optionRect[i].h)
                    {
                        chosenIndex = i;
                        waiting = false;
                        break;
                    }
                }
            }
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (questionBgTexture)
            SDL_RenderCopy(renderer, questionBgTexture, nullptr, nullptr);
        else if (backgroundTexture)
            SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
        else
        {
            SDL_SetRenderDrawColor(renderer, 16, 18, 24, 255);
            SDL_RenderClear(renderer);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 105);
        SDL_Rect full = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 10, 10, 12, 70);
        SDL_RenderFillRect(renderer, &panel);

        if (torchTexture)
        {
            SDL_Rect icon = {panel.x + 38, panel.y + 14, 44, 44};
            SDL_RenderCopy(renderer, torchTexture, nullptr, &icon);
        }
        if (mineTexture)
        {
            SDL_Rect icon = {panel.x + panel.w - 82, panel.y + 16, 40, 40};
            SDL_RenderCopy(renderer, mineTexture, nullptr, &icon);
        }

        if (questionFrameTexture)
            SDL_RenderCopy(renderer, questionFrameTexture, &questionFrameSrc, &questionFrameRect);
        else
        {
            SDL_SetRenderDrawColor(renderer, 20, 20, 26, 220);
            SDL_RenderFillRect(renderer, &questionFrameRect);
            SDL_SetRenderDrawColor(renderer, 233, 189, 98, 240);
            SDL_RenderDrawRect(renderer, &questionFrameRect);
        }

        SDL_Rect titleRect = {panel.x, panel.y + 16, panel.w, 54};
        drawWrappedCenteredInRect(renderer, titleFont, "TORCH TRIAL", SDL_Color{132, 84, 36, 255}, titleRect, panel.w - 140);
        drawWrappedCenteredInRect(renderer, bodyFont, q.text, SDL_Color{64, 41, 19, 255}, questionTextRect, questionTextRect.w, -1);

        for (int i = 0; i < 4; i++)
        {
            if (answerFrameTexture)
            {
                SDL_RenderCopy(renderer, answerFrameTexture, &answerFrameSrc, &optionRect[i]);
            }
            else
            {
                SDL_Color fill = {44, 46, 64, 245};
                SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
                SDL_RenderFillRect(renderer, &optionRect[i]);
                SDL_SetRenderDrawColor(renderer, 230, 230, 230, 220);
                SDL_RenderDrawRect(renderer, &optionRect[i]);
            }

            if (i == hoverIndex)
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 255, 214, 120, 88);
                SDL_Rect glowRect = optionRect[i];
                int glowPadX = optionRect[i].w / 8;
                int glowPadY = optionRect[i].h / 5;
                glowRect.x += glowPadX;
                glowRect.y += glowPadY;
                glowRect.w -= glowPadX * 2;
                glowRect.h -= glowPadY * 2;
                SDL_RenderFillRect(renderer, &glowRect);
                SDL_SetRenderDrawColor(renderer, 166, 106, 22, 220);
                SDL_RenderDrawRect(renderer, &glowRect);
            }

            SDL_Rect optionTextRect = optionRect[i];
            int textPadX = 24;
            int textPadY = 14;
            optionTextRect.x += textPadX;
            optionTextRect.y += textPadY;
            optionTextRect.w -= textPadX * 2;
            optionTextRect.h -= textPadY * 2;
            drawWrappedLeftInRect(renderer, optFont, optionText[i], SDL_Color{55, 36, 18, 255}, optionTextRect, optionTextRect.w, 0);
        }

        std::string footer = "Choose an answer with the mouse.";
        if (showCorrectAnswer)
        {
            footer += "  Correct answer: ";
            footer.push_back(q.answer);
        }
        SDL_Rect footerRect = {panel.x + 12, panel.y + panel.h - 34, panel.w - 24, 24};
        drawWrappedCenteredInRect(renderer, optFont, footer, SDL_Color{242, 232, 203, 255}, footerRect, footerRect.w);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (backgroundTexture)
        SDL_DestroyTexture(backgroundTexture);
    if (questionBgTexture)
        SDL_DestroyTexture(questionBgTexture);
    if (questionFrameTexture)
        SDL_DestroyTexture(questionFrameTexture);
    if (answerFrameTexture)
        SDL_DestroyTexture(answerFrameTexture);
    if (torchTexture)
        SDL_DestroyTexture(torchTexture);
    if (mineTexture)
        SDL_DestroyTexture(mineTexture);

    if (arrowCursor)
    {
        SDL_SetCursor(arrowCursor);
        SDL_FreeCursor(arrowCursor);
    }
    if (handCursor)
        SDL_FreeCursor(handCursor);

    TTF_CloseFont(titleFont);
    TTF_CloseFont(bodyFont);
    TTF_CloseFont(optFont);

    if (ttfInitByQuestion)
        TTF_Quit();

    if (chosenIndex < 0 || chosenIndex > 3)
        return false;

    char chosen = static_cast<char>('A' + chosenIndex);
    return chosen == q.answer;
#endif
}
