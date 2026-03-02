#include "QuestionManager.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cctype>

namespace
{
    Question makeFallbackQuestion()
    {
        Question q;
        q.text = "LightQuest: Chon dap an dung de mo duoc.";
        q.optA = "2 + 2 = 3";
        q.optB = "2 + 2 = 4";
        q.optC = "2 + 2 = 5";
        q.optD = "2 + 2 = 6";
        q.answer = 'B';
        return q;
    }
}

bool QuestionManager::loadFromFile(const char* path)
{
    questions.clear();
    std::ifstream in(path);
    if (!in.is_open())
    {
        questions.push_back(makeFallbackQuestion());
        return true;
    }

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty()) continue; // skip stray blank lines

        Question q;
        q.text = line;

        // read 4 options
        if (!std::getline(in, q.optA)) break;
        if (!std::getline(in, q.optB)) break;
        if (!std::getline(in, q.optC)) break;
        if (!std::getline(in, q.optD)) break;

        // read answer line
        if (!std::getline(in, line)) break;
        if (!line.empty())
            q.answer = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));
        else
            q.answer = 'A';

        if (q.answer < 'A' || q.answer > 'D')
            q.answer = 'A';

        questions.push_back(q);

        // consume possible blank separator
        // next iteration will skip blanks
    }

    if (questions.empty())
        questions.push_back(makeFallbackQuestion());

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    return true;
}

int QuestionManager::randomIndex() const
{
    if (questions.empty()) return -1;
    return std::rand() % static_cast<int>(questions.size());
}

bool QuestionManager::askQuestion(int index, bool showCorrectAnswer)
{
    if (index < 0 || index >= static_cast<int>(questions.size())) return false;
    const Question &q = questions[index];

    std::ostringstream oss;
    oss << q.text << "\n\n";
    oss << "A: " << q.optA << "\n";
    oss << "B: " << q.optB << "\n";
    oss << "C: " << q.optC << "\n";
    oss << "D: " << q.optD << "\n";

    if (showCorrectAnswer)
    {
        oss << "\nDap an dung: " << q.answer << "\n";
    }

    std::string message = oss.str();

    // Setup buttons
    const SDL_MessageBoxButtonData buttons[] = {
        { 0, 0, "A" },
        { 0, 1, "B" },
        { 0, 2, "C" },
        { 0, 3, "D" }
    };

    SDL_MessageBoxData boxdata;
    boxdata.flags = SDL_MESSAGEBOX_INFORMATION;
    boxdata.window = nullptr;
    boxdata.title = "Question";
    boxdata.message = message.c_str();
    boxdata.numbuttons = 4;
    boxdata.buttons = buttons;
    boxdata.colorScheme = nullptr;

    int buttonid = -1;
    if (SDL_ShowMessageBox(&boxdata, &buttonid) < 0)
    {
        return false;
    }

    char chosen = 'A';
    if (buttonid == 1) chosen = 'B';
    else if (buttonid == 2) chosen = 'C';
    else if (buttonid == 3) chosen = 'D';

    if (chosen == q.answer || (chosen - 'A') == (q.answer - 'A'))
        return true;

    return false;
}
