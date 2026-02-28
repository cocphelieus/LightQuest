#pragma once
#include <vector>
#include <string>

struct Question {
    std::string text;
    std::string choices[4];
    char correct; // 'a'..'d'
};

class QuestionManager
{
public:
    QuestionManager();

    // return a random question (for now there's only one)
    const Question& getRandomQuestion() const;
    bool askQuestion(const Question& q) const;

private:
    std::vector<Question> questions;
};
