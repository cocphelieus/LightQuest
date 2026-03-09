#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct Question {
    std::string text;
    std::string optA;
    std::string optB;
    std::string optC;
    std::string optD;
    char answer; // 'A'..'D'
};

class QuestionManager {
public:
    bool loadFromFile(const char* path);
    bool hasQuestions() const { return !questions.empty(); }
    int getCount() const { return static_cast<int>(questions.size()); }
    // Presents a question using an in-game overlay panel; returns true if answered correctly.
    bool askQuestion(int index, SDL_Renderer* renderer, bool showCorrectAnswer = false);
    // get a random index
    int randomIndex() const;

private:
    std::vector<Question> questions;
};
