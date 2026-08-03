#include "engine/Board.hpp"
#include <iostream>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, label) do { \
    if (cond) { g_passed++; } \
    else { g_failed++; std::cout << "FAILED: " << label << " (line " << __LINE__ << ")\n"; } \
} while (0)

// Goal: place a sequence of moves for alternating/explicit players without going
// through legality checks — used to construct exact board states for testing,
// bypassing the console loop entirely.
void setup(Board& b, std::initializer_list<std::pair<Move, Player>> moves) {
    for (const auto& [m, p] : moves)
        b.placeStone(m, p);
}

void test_horizontal_win() {
    Board b;
    setup(b, {
        {{5,5}, Player::Black}, {{6,5}, Player::Black}, {{7,5}, Player::Black},
        {{8,5}, Player::Black}, {{9,5}, Player::Black}
    });
    auto result = b.checkWinConditions({9,5}, Player::Black);
    CHECK(result.won && result.reason == Board::WinReason::Alignment, "horizontal win detected");
}

void test_no_false_positive_four() {
    Board b;
    setup(b, {
        {{5,5}, Player::Black}, {{6,5}, Player::Black},
        {{7,5}, Player::Black}, {{8,5}, Player::Black}
    });
    auto result = b.checkWinConditions({8,5}, Player::Black);
    CHECK(!result.won, "4-in-a-row does not trigger a win");
}

void test_basic_capture() {
    Board b;
    setup(b, {
        {{5,5}, Player::Black}, {{6,5}, Player::White},
        {{7,5}, Player::White}, {{8,5}, Player::Black}
    });
    int captured = b.checkAndApplyCaptures({8,5}, Player::Black);
    CHECK(captured == 2, "captures exactly 2 stones");
    CHECK(b.get(6,5) == Cell::Empty && b.get(7,5) == Cell::Empty, "captured cells are cleared");
}

void test_safe_move_into_capture() {
    Board b;
    setup(b, {
        {{5,5}, Player::Black}, {{6,5}, Player::White},
        {{9,5}, Player::Black}, {{7,5}, Player::White}
    });
    int captured = b.checkAndApplyCaptures({7,5}, Player::White); // placing the 2nd White doesn't self-capture
    CHECK(captured == 0, "no capture triggers when placing 2 of 3 in a row");

    b.placeStone({8,5}, Player::White); // completes 3 White in a row between two Black
    int captured2 = b.checkAndApplyCaptures({8,5}, Player::White);
    CHECK(captured2 == 0, "moving into a would-be-captured position stays safe");
}

void test_ten_capture_win() {
    Board b;
    // Directly exercises the win condition without needing 5 real capture sequences —
    // this replaces the test that was too tedious to hand-play in Step 5.
    setup(b, {
        {{5,5}, Player::Black}, {{6,5}, Player::White}, {{7,5}, Player::White}, {{8,5}, Player::Black}
    });
    b.checkAndApplyCaptures({8,5}, Player::Black); // Black captured = 2
    // repeat 4 more times in different spots to reach 10 total
    for (int row = 1; row <= 4; row++) {
        setup(b, {
            {{5, 5+row}, Player::Black}, {{6, 5+row}, Player::White}, {{7, 5+row}, Player::White}, {{8, 5+row}, Player::Black}
        });
        b.checkAndApplyCaptures({8, 5+row}, Player::Black);
    }
    CHECK(b.capturedBy(Player::Black) == 10, "captured count reaches 10");
    auto result = b.checkWinConditions({8,9}, Player::Black);
    CHECK(result.won && result.reason == Board::WinReason::Capture, "10-capture win detected");
}

void test_free_three_straight() {
    Board b;
    setup(b, { {{5,5}, Player::Black}, {{6,5}, Player::Black}, {{7,5}, Player::Black} });
    CHECK(b.countFreeThrees({7,5}, Player::Black) == 1, "straight open three detected");
}

void test_free_three_blocked_no_trigger() {
    Board b;
    setup(b, {
        {{4,5}, Player::White},
        {{5,5}, Player::Black}, {{6,5}, Player::Black}, {{7,5}, Player::Black}
    });
    CHECK(b.countFreeThrees({7,5}, Player::Black) == 0, "blocked three is not free");
}

void test_double_three_rejected() {
    Board b;
    setup(b, {
        {{9,10}, Player::Black}, {{11,10}, Player::Black},
        {{10,9}, Player::Black}, {{10,11}, Player::Black}
    });
    auto eval = b.evaluateMove({10,10}, Player::Black);
    CHECK(eval.freeThrees == 2 && !eval.wouldCapture && !eval.legal, "pure double-three rejected");
}

int main() {
    test_horizontal_win();
    test_no_false_positive_four();
    test_basic_capture();
    test_safe_move_into_capture();
    test_ten_capture_win();
    test_free_three_straight();
    test_free_three_blocked_no_trigger();
    test_double_three_rejected();

    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}