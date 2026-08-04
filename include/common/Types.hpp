#pragma once

enum class Cell { Empty, Black, White };
enum class Player { Black, White };

struct Move {
    int x;
    int y;
};

// Goal: one candidate move plus what the search scored it. Lives here rather
// than in Minimax.hpp so the UI can render these without the ui layer having
// to include the ai layer.
struct ScoredMove {
    Move move;
    int score;
};