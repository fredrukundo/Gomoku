NAME = Gomoku

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude
CXXFLAGS += $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2)

SRC_DIR = src
OBJ_DIR = obj

SRC = $(shell find $(SRC_DIR) -name '*.cpp')
OBJ = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

TEST_SRC = $(shell find src/engine -name '*.cpp') tests/rules_tests.cpp
TEST_OBJ = $(patsubst %.cpp,$(OBJ_DIR)/test_%.o,$(TEST_SRC))

AI_TEST_SRC = $(shell find src/engine -name '*.cpp') $(shell find src/ai -name '*.cpp') tests/ai_smoke_test.cpp
AI_TEST_OBJ = $(patsubst %.cpp,$(OBJ_DIR)/aitest_%.o,$(AI_TEST_SRC))

DEP = $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

test: $(TEST_OBJ)
	$(CXX) $(TEST_OBJ) -o run_tests
	./run_tests

$(OBJ_DIR)/test_%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

ai-test: $(AI_TEST_OBJ)
	$(CXX) $(AI_TEST_OBJ) -o run_ai_test
	./run_ai_test

$(OBJ_DIR)/aitest_%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re test ai-test