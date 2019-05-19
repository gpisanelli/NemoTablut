#include "client.h"
#include "parameters.h"
#include "search.h"
#include "move_type.h"
#include "traspostition_table.h"
#include "abdada.h"
#include "move_sorting.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>


#define STATE_BUFFER_SIZE 2000

#define MOVE_BUFFER_SIZE 100

#define WHITE_SERVER_PORT 5800
#define BLACK_SERVER_PORT 5801

#define PLAYER_NAME "NEMO"


char columnToServerFormat(uint32_t column) {
	return 'A' + column;
}

uint32_t  rowToServerFormat(uint32_t row) {
	return row + 1U;
}


int jsonToState(char* stateString, BoardState* state) {
	int row = 0;
	int column = 0;
	int count = 0;
	const char delimiter[2] = "\"";
	char* token;

	initEmptyBoard(state);

	token = strtok(stateString, delimiter);

	while(token != NULL && count < 81) {
		token = strtok(NULL, delimiter);

		if(strcmp(token, "EMPTY") == 0 || strcmp(token, "THRONE") == 0) {
			state->Board[row] = state->Board[row] << 2U;
			column++;
			count++;
		}
		else if(strcmp(token, "WHITE") == 0) {
			state->Board[row] = state->Board[row] << 2U;
			state->Board[row] |= WHITE;
			column++;
			count++;
		}
		else if(strcmp(token, "BLACK") == 0) {
			state->Board[row] = state->Board[row] << 2U;
			state->Board[row] |= BLACK;
			column++;
			count++;
		}
		else if(strcmp(token, "KING") == 0) {
			state->Board[row] = state->Board[row] << 2U;
			state->Board[row] |= WHITE_KING;
			column++;
			count++;
		}

		if(column == 9) {
			column = 0;
			row++;
		}
	}

    strtok(NULL, delimiter);            // ]],
    strtok(NULL, delimiter);            // turn
    strtok(NULL, delimiter);            // :
    token = strtok(NULL, delimiter);    // TURNO
    printf("TURNO: -%s-\n", token);

    if(strcmp(token, "WHITE") == 0)
        return WHITE_TURN;
    else if(strcmp(token, "BLACK") == 0)
        return BLACK_TURN;
    else if(strcmp(token, "WHITEWIN") == 0)
        return WHITE_WIN_TURN;
    else if(strcmp(token,"BLACKWIN") == 0)
        return BLACK_WIN_TURN;
    else if(strcmp(token, "DRAW") == 0)
        return DRAW_TURN;

    return -1;
}


// {"from":"d5","to":"d4","turn":"WHITE"}
void sendMove(int sd, Move move, int playerColor) {
	int startRow, targetRow, stringLength;
	char startColumn, targetColumn;
	char moveString[MOVE_BUFFER_SIZE];

	startRow = rowToServerFormat(START_ROW(move));
	startColumn = columnToServerFormat(START_COLUMN(move));
	targetRow = rowToServerFormat(TARGET_ROW(move));
	targetColumn = columnToServerFormat(TARGET_COLUMN(move));

	if(playerColor == WHITE)
		sprintf(moveString, "{\"from\":\"%c%d\",\"to\":\"%c%d\",\"turn\":\"WHITE\"}", startColumn, startRow, targetColumn, targetRow);
	else
		sprintf(moveString, "{\"from\":\"%c%d\",\"to\":\"%c%d\",\"turn\":\"BLACK\"}", startColumn, startRow, targetColumn, targetRow);

	printf("\nInvio mossa: %s\n", moveString);

	stringLength = htonl(strlen(moveString));
	write(sd, &stringLength, sizeof(stringLength));
	write(sd, moveString, sizeof(char) * strlen(moveString));
}


void gameLoop(int playerColor, int maxTime, int numThreads) {
	int sd, port;
	struct hostent *host;
	struct sockaddr_in servaddr;
	char stateString[STATE_BUFFER_SIZE];
	char name[] = PLAYER_NAME;


    initTables();
    initMoveTable();
	HistoryArray historyArray;
	initHistoryArray(&historyArray);


	port = (playerColor == WHITE) ? WHITE_SERVER_PORT : BLACK_SERVER_PORT;
	printf("Port: %d\n", port);

	// Inizializzazione indirizzo Server
	memset((char *)&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	host = gethostbyname(SERVER_IP);

	servaddr.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
	servaddr.sin_port = htons(port);

	// Creazione Socket
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd < 0) {
	   perror("apertura socket");
	   exit(1);
	}
	printf("Client: creata la socket sd=%d\n", sd);

	// Connect (bind implicito)
	if(connect(sd,(struct sockaddr *) &servaddr, sizeof(struct sockaddr)) < 0) {
	   perror("connect");
	   exit(1);
	}
	printf("Client: connect ok\n\n");


	// Invio nome giocatore
	int length = htonl(strlen(name));
	write(sd, &length, sizeof(int));
	write(sd, name, sizeof(char) * strlen(name));


	BoardState state;

	// Game loop
	while(read(sd, stateString, 1) > 0) {       // Ricezione stato
		read(sd, stateString, 1);
		read(sd, stateString, 1);
		read(sd, stateString, 1);

		ssize_t bytesReceived = read(sd, stateString, STATE_BUFFER_SIZE);
        printf("Byte ricevuti: %ld\n", bytesReceived);
        stateString[bytesReceived] = '\0';

        //printf("\nRICEVUTA STRINGA:\n-%s-\n", stateString);
		//printf("CONVERSIONE JSON\n");
		int turn = jsonToState(stateString, &state);
		printBoard(state);


		if(turn == BLACK_TURN && playerColor == BLACK) {
			// Calcolo mossa
			iterativeDeepeningSearchBlack(state, MAX_DEPTH, historyArray, maxTime, numThreads);

			Move move = getFinalBestMoveBlack();

            if(move == MOVE_NOT_FOUND || move == 0) {
                Move movesArr[MAX_MOVES];
                BoardState currentBoard = state;
                int movesNumber = getPossibleMovesBlack(currentBoard, movesArr);
                sortMovesArrayBlack(&currentBoard, movesArr, movesNumber);

                move = movesArr[0];
            }

			printf("Best move: ");
			printMove(move);
			printf("\n");

			// Invio mossa
			sendMove(sd, move, playerColor);
		}
		else if(turn == WHITE_TURN && playerColor == WHITE) {
			// Calcolo mossa
			iterativeDeepeningSearchWhite(state, MAX_DEPTH, historyArray, maxTime, numThreads);

			Move move = getFinalBestMoveWhite();

			if(move == MOVE_NOT_FOUND || move == 0) {
                Move movesArr[MAX_MOVES];
                BoardState currentBoard = state;
                int movesNumber = getPossibleMovesWhite(currentBoard, movesArr);
                sortMovesArrayWhite(&currentBoard, movesArr, movesNumber);

                move = movesArr[0];
			}
			printf("Best move: ");
			printMove(move);
			printf("\n");

			// Invio mossa
			sendMove(sd, move, playerColor);
		}
		else if(turn == WHITE_WIN_TURN || turn == BLACK_WIN_TURN || turn == DRAW_TURN) {
		    printf("\n\n====================================\n\nFINE PARTITA\n\n");
		    continue;
		}

		addPlayedBoard(&historyArray, state);
	}
	
	close(sd);
	printf("\nCLIENT: Terminazione\n");
}
