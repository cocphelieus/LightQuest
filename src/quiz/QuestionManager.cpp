#include "QuestionManager.h"
#include <iostream>
#include <cstdlib>
#include <cctype>

QuestionManager::QuestionManager()
{
    // mock data: one question where correct answer is 'b'
    Question q;
    q.text = "What is the capital of France?";
    q.choices[0] = "London";
    q.choices[1] = "Paris";
    q.choices[2] = "Berlin";
    q.choices[3] = "Madrid";
    q.correct = 'b';
    questions.push_back(q);
}

const Question& QuestionManager::getRandomQuestion() const
{
    // just return first for now
    return questions[0];
}

bool QuestionManager::askQuestion(const Question& q) const
{
    std::cout << "Question: " << q.text << "\n";
    char option = 'a';
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "  " << (char)(option + i) << ") " << q.choices[i] << "\n";
    }
    std::cout << "Your answer: ";
    char ans;
    std::cin >> ans;
    ans = std::tolower(ans);
    bool correct = (ans == q.correct);
    std::cout << (correct ? "Correct!" : "Wrong!") << "\n";
    return correct;
}