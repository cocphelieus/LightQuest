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
    // Parse questions from text file into memory.
    bool loadFromFile(const char* path);
    bool hasQuestions() const { return !questions.empty(); }
    int getCount() const { return static_cast<int>(questions.size()); }
    // Present a question in an overlay and return whether the player answered correctly.
    // showCorrectAnswer is used by tester/debug mode.
    bool askQuestion(int index, SDL_Renderer* renderer, bool showCorrectAnswer = false);
    // Pick a random question index from loaded list.
    int randomIndex() const;

private:
    std::vector<Question> questions;
};
