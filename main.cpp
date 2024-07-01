#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <stdio.h>
#include <stdio_ext.h>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

void clear_stdout() { system("clear"); }
std::vector<std::string> board;
std::string board_line;

std::vector<std::vector<std::string>>
    tetrominos({// o    oo   ooo   o
                // ooo  o      o   o
                //      o          oo
                {"o\nooo", "oo\no\no", "ooo\n  o", "o\no\noo"},
                // oo
                // oo
                {"oo\noo"},
                // oooo o
                //      o
                //      o
                //      o
                {"oooo", "o\no\no\no"},
                //  o    o   ooo   o
                // ooo   oo   o   oo
                //       o         o
                {" o\nooo", "o\noo\no", "ooo\n o", " o\noo\n o"}});

std::mutex mutex;
std::string current_tetromino;
std::string next_tetromino;
size_t current_tetromino_x = 0;
size_t current_tetromino_y = 0;
std::atomic_bool gameover = false;
bool enable_flush_screen = true;
size_t tetromino_count = 0;
size_t score = 0;

void init_board(size_t width, size_t height) {
  assert(width > 2 and height > 1);

  board_line = "|" + std::string(width - 2, ' ') + "|";
  board = std::vector<std::string>(height - 1, board_line);
  board.push_back(std::string(width, '-'));
}

void flush_screen() {
  if (not enable_flush_screen)
    return;

  clear_stdout();
  std::cout << "tetromino index:" << tetromino_count << ", score:" << score
            << std::endl;

  std::cout << "next tetromino: \n" << next_tetromino << std::endl;
  size_t count =
      std::count(next_tetromino.cbegin(), next_tetromino.cend(), '\n');
  std::cout << std::string(4 - count, '\n');

  for (auto &&line : board) {
    std::cout << line << std::endl;
  }
}

void put_tetromino_and_draw(const std::string &tetromino, size_t left_top_x,
                            size_t left_top_y) {
  size_t x = left_top_x;
  size_t y = left_top_y;

  for (auto &&ch : tetromino) {
    if (ch == 'o') {
      board[x][y] = ch;
      y++;
    } else if (ch == '\n') {
      x++;
      y = left_top_y;
    } else if (ch == ' ') {
      y++;
    }
  }

  flush_screen();
}

void display_gameover() {
  for (auto &&line : board) {
    for (auto &&ch : line) {
      if (ch == ' ') {
        ch = 'x';
      }
    }
  }

  auto &middle_line = board[board.size() / 2];
  std::string gameover = "Game Over!";
  size_t start_index = middle_line.size() / 2 - gameover.size() / 2;
  for (size_t i = 0; i < gameover.size(); i++) {
    middle_line[start_index + i] = gameover[i];
  }
}

bool can_put(const std::string &tetromino, size_t left_top_x,
             size_t left_top_y) {
  size_t x = left_top_x;
  size_t y = left_top_y;
  for (auto &&ch : tetromino) {
    if (ch == 'o') {
      if (board[x][y] != ' ')
        return false;
      y++;
    } else if (ch == '\n') {
      x++;
      y = left_top_y;
    } else if (ch == ' ') {
      y++;
    }
  }
  return true;
}

std::string random_tetromino() {
  size_t tetromino_index = rand() % tetrominos.size();
  return tetrominos[tetromino_index]
                   [rand() % tetrominos[tetromino_index].size()];
}

void generate_new_tetromino() {
  current_tetromino = next_tetromino;
  next_tetromino = random_tetromino();
  if (current_tetromino.empty())
    current_tetromino = random_tetromino();
  tetromino_count++;

  size_t tetromino_width = current_tetromino.size();
  size_t board_mid_index = board[0].size() / 2;
  size_t left_top_y = board_mid_index - tetromino_width / 2;

  current_tetromino_x = 0;
  current_tetromino_y = left_top_y;
  if (not can_put(current_tetromino, current_tetromino_x,
                  current_tetromino_y)) {
    display_gameover();
    flush_screen();
    gameover.store(true);
    return;
  }
  put_tetromino_and_draw(current_tetromino, current_tetromino_x,
                         current_tetromino_y);
}

void clear_current_tetromino() {
  size_t x = current_tetromino_x;
  size_t y = current_tetromino_y;
  for (auto &&ch : current_tetromino) {
    if (ch == 'o') {
      board[x][y] = ' ';
      y++;
    } else if (ch == '\n') {
      x++;
      y = current_tetromino_y;
    } else if (ch == ' ') {
      y++;
    }
  }
}

void clear_full_rows() {
  for (auto &&line : board) {
    if (line.find(' ') == std::string::npos && line != board.back()) {
      line = board_line;
      score += 100;
    }
  }

  for (auto riter = board.rbegin() + 1 /* not including bottom */;
       riter != board.rend(); ++riter) {
    if (*riter == board_line)
      continue;

    if (*(riter - 1) != board_line)
      continue;

    auto iter = std::find_if(board.rbegin(), board.rend(),
                             [](auto &&line) { return line == board_line; });

    assert(iter != board.rend() && iter < riter);

    *iter = *riter;
    *riter = board_line;
  }
}

bool try_move_current_tetromino_to(size_t x, size_t y) {
  clear_current_tetromino();
  if (can_put(current_tetromino, x, y)) {
    put_tetromino_and_draw(current_tetromino, x, y);
    return true;
  } else {
    // rollback
    put_tetromino_and_draw(current_tetromino, current_tetromino_x,
                           current_tetromino_y);
    return false;
  }
}

bool try_move_current_tetromino_down() {
  if (try_move_current_tetromino_to(current_tetromino_x + 1,
                                    current_tetromino_y)) {
    current_tetromino_x++;
    return true;
  } else {
    clear_full_rows();
    generate_new_tetromino();
    return false;
  }
}

std::string get_rotate_tetromino(const std::string &tetromino) {
  for (auto &&tetromino_vector : tetrominos) {
    if (auto iter = std::find(tetromino_vector.begin(), tetromino_vector.end(),
                              tetromino);
        iter != tetromino_vector.end()) {
      auto next_iter = iter + 1;
      if (next_iter == tetromino_vector.end()) {
        next_iter = tetromino_vector.begin();
      }
      return *next_iter;
    }
  }
  assert(false);
  return "";
}

std::pair<size_t, size_t> correct_the_coordinates(size_t x, size_t y) {
  return {0, 0};
}

enum class Operation { Left, Right, Down, DropToBottom, Rotate };
bool handle_keyboard_input(Operation operation) {
  std::lock_guard<std::mutex> lock(mutex);

  switch (operation) {
  case Operation::Left: {
    if (try_move_current_tetromino_to(current_tetromino_x,
                                      current_tetromino_y - 1)) {
      current_tetromino_y--;
    }
    break;
  }
  case Operation::Right: {
    if (try_move_current_tetromino_to(current_tetromino_x,
                                      current_tetromino_y + 1)) {
      current_tetromino_y++;
    }
    break;
  }
  case Operation::Down: {
    try_move_current_tetromino_down();
    break;
  }
  case Operation::Rotate: {
    clear_current_tetromino();
    auto tetromino = get_rotate_tetromino(current_tetromino);
    if (can_put(tetromino, current_tetromino_x, current_tetromino_y)) {
      put_tetromino_and_draw(tetromino, current_tetromino_x,
                             current_tetromino_y);
      current_tetromino = tetromino;
    } else {
      put_tetromino_and_draw(current_tetromino, current_tetromino_x,
                             current_tetromino_y);
    }
    break;
  }
  case Operation::DropToBottom: {
    enable_flush_screen = false;
    while (try_move_current_tetromino_down())
      ;
    enable_flush_screen = true;
    flush_screen();
    break;
  }
  }

  return true;
}

void auto_drop_tetromino() {
  while (true) {
    {
      sleep(1);
      {
        std::lock_guard<std::mutex> lock(mutex);
        try_move_current_tetromino_down();
      }
    }
  }
}

void enable_raw_mode() {
  termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

int main() {
  enable_raw_mode();
  init_board(20, 20);
  generate_new_tetromino();

  std::jthread run([] { auto_drop_tetromino(); });

  while (true) {
    static std::map<char, Operation> operation_map = {
        {'w', Operation::Rotate},
        {'a', Operation::Left},
        {'d', Operation::Right},
        {'s', Operation::Down},
        {' ', Operation::DropToBottom}};

    char ch;
    ch = std::cin.get();
    if (gameover.load()) {
      continue;
    }

    if (operation_map.count(ch) == 1) {
      handle_keyboard_input(operation_map[ch]);
    }
  }
}