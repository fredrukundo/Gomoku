#pragma once

enum class Cell { Empty, Black, White };
enum class Player { Black, White };

struct Move {
    int x;
    int y;
};