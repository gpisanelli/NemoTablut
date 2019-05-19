#pragma once

#include <stdint.h>

// 2 bit * 9 caselle -> 18 bit per riga
// unsigned long (4 byte -> 32 bit)

#define EMPTY 0U			// 00
#define BLACK 1U			// 01
#define WHITE 2U			// 10
#define WHITE_KING 3U       // 11


typedef struct {
	uint32_t Board[9];
} BoardState;


void printBoard(BoardState board);
void initBoard(BoardState* board);
void initEmptyBoard(BoardState* board);
int isEqual(BoardState board1, BoardState board2);
