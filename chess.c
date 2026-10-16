/*
 * Libraries: Raylib for graphics/input, Emscripten for the browser game loop.
 * Board representation: board[y][x], 0 = empty, positive = white, negative = black.
 * This is a local two-player game, not a chess AI.
 */

#include <stdio.h>

#include <stdbool.h>

#include <stdlib.h>

#include <string.h>

#include "raylib.h"

#include <emscripten/emscripten.h>



#define PLAYER_WHITE 0

#define PLAYER_BLACK 1

#define BOARD 8

#define TILE 80

#define HUD 72



/* Piece values: pawn=1, bishop=2, knight=3, rook=4, queen=5, king=6.

   Positive pieces are white, negative pieces are black. */

static const int START_BOARD[BOARD][BOARD] = {

    {-4,-3,-2,-5,-6,-2,-3,-4},

    {-1,-1,-1,-1,-1,-1,-1,-1},

    { 0, 0, 0, 0, 0, 0, 0, 0},

    { 0, 0, 0, 0, 0, 0, 0, 0},

    { 0, 0, 0, 0, 0, 0, 0, 0},

    { 0, 0, 0, 0, 0, 0, 0, 0},

    { 1, 1, 1, 1, 1, 1, 1, 1},

    { 4, 3, 2, 5, 6, 2, 3, 4}

};



typedef struct {

    int board[BOARD][BOARD];

    int turn;

    bool castle[2][2]; /* [colour][0=queen-side,1=king-side] */

    int enPassantX, enPassantY; /* Destination square for next en passant move */

} Position;



static Position game;

static int selectedX = -1, selectedY = -1;

static int gameResult = 0; /* 0 playing, 1 white wins, 2 black wins, 3 draw */

static char statusMessage[100] = "";



static bool Inside(int x, int y) { return x >= 0 && x < BOARD && y >= 0 && y < BOARD; }

static int Colour(int piece) { return piece > 0 ? PLAYER_WHITE : PLAYER_BLACK; }

static bool SameColour(int a, int b) { return a && b && ((a > 0) == (b > 0)); }



/* RESET AND INITIALISATION
 * Copy the constant opening position into the mutable game state. memcpy copies
 * the entire 8x8 integer array, avoiding 64 individual assignments.
 * White plays first; both sides initially retain queen-side and king-side
 * castling rights. (-1,-1) means no en-passant target is currently available.
 * Clear selection, terminal result and status text for a fresh game.
 */
static void ResetGame(void)

{

    memcpy(game.board, START_BOARD, sizeof(game.board));

    game.turn = PLAYER_WHITE;

    game.castle[0][0] = game.castle[0][1] = true;

    game.castle[1][0] = game.castle[1][1] = true;

    game.enPassantX = game.enPassantY = -1;

    selectedX = selectedY = -1;

    gameResult = 0;

    statusMessage[0] = '\0';

}



/* SLIDING PIECE PATH CHECK
 * Calculate a unit step in x/y toward the destination, then visit every
 * intermediate square (not the destination). A bishop/rook/queen can only
 * travel if these squares are empty. Caller first checks that the requested
 * direction is valid for the piece; this helper alone does not do so.
 */
static bool ClearPath(const Position *p, int fx, int fy, int tx, int ty)

{

    int dx = (tx > fx) - (tx < fx);

    int dy = (ty > fy) - (ty < fy);

    int x = fx + dx, y = fy + dy;

    while (x != tx || y != ty) {

        if (p->board[y][x] != 0) return false;

        x += dx;

        y += dy;

    }

    return true;

}



/* Attacks are independent of whose turn it is, and exclude castling. */

/* ATTACK MAP / THREAT DETECTION
 * Scan every enemy piece and ask whether it attacks square (x,y).
 * Pawns attack diagonally (not where they move straight); knights jump;
 * sliding pieces require ClearPath; kings attack adjacent squares.
 * Ignore whose turn it is: threat detection is needed when checking whether
 * a king is safe and whether castling crosses an attacked square.
 * Castling is not an attacking move and is intentionally excluded.
 */
static bool SquareAttacked(const Position *p, int x, int y, int byColour)

{

    for (int fy = 0; fy < BOARD; fy++) {

        for (int fx = 0; fx < BOARD; fx++) {

            int piece = p->board[fy][fx];

            if (!piece || Colour(piece) != byColour) continue;

            int dx = x - fx, dy = y - fy;

            int ax = abs(dx), ay = abs(dy);

            switch (abs(piece)) {

                case 1:

                    if (ay == 1 && ax == 1 && dy == (byColour == PLAYER_WHITE ? -1 : 1)) return true;

                    break;

                case 2:

                    if (ax == ay && ax != 0 && ClearPath(p, fx, fy, x, y)) return true;

                    break;

                case 3:

                    if ((ax == 1 && ay == 2) || (ax == 2 && ay == 1)) return true;

                    break;

                case 4:

                    if (((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) && ClearPath(p, fx, fy, x, y)) return true;

                    break;

                case 5:

                    if (((ax == ay && ax != 0) || (dx == 0 && dy != 0) || (dy == 0 && dx != 0)) && ClearPath(p, fx, fy, x, y)) return true;

                    break;

                case 6:

                    if (ax <= 1 && ay <= 1 && (ax || ay)) return true;

                    break;

            }

        }

    }

    return false;

}



/* KING SAFETY
 * Find the requested colour's king and ask whether an opposing piece attacks
 * its square. A missing king is treated as an invalid/unsafe position.
 * Note: the rest of the engine disallows direct captures of kings.
 */
static bool KingInCheck(const Position *p, int colour)

{

    int king = colour == PLAYER_WHITE ? 6 : -6;

    for (int y = 0; y < BOARD; y++)

        for (int x = 0; x < BOARD; x++)

            if (p->board[y][x] == king)

                return SquareAttacked(p, x, y, 1 - colour);

    return true; /* An invalid position with no king must never be accepted. */

}



/* PIECE MOVEMENT RULES (WITHOUT FINAL KING-SAFETY TEST)
 * Reject out-of-bounds/no-op moves, empty source, friendly captures, and
 * attempts to capture the opponent's king. Then switch on the piece type.
 * Pawn: forward one/two (from start), diagonal capture, en passant.
 * Bishop/rook/queen: geometric direction plus unobstructed path.
 * Knight: L-shaped jump. King: adjacent step or legal castling geometry.
 * Castling additionally requires rights, the original rook, a clear path,
 * and no attack on the start, transit or destination king squares.
 * IMPORTANT: a pseudo-legal move can still expose your own king. LegalMove
 * performs the final safety check by simulating the move.
 */
static bool PseudoLegal(const Position *p, int fx, int fy, int tx, int ty)

{

    if (!Inside(fx, fy) || !Inside(tx, ty) || (fx == tx && fy == ty)) return false;

    int piece = p->board[fy][fx], target = p->board[ty][tx];

    if (!piece || SameColour(piece, target) || abs(target) == 6) return false;

    int colour = Colour(piece);

    int dx = tx - fx, dy = ty - fy, ax = abs(dx), ay = abs(dy);

    switch (abs(piece)) {

        case 1: {

            int dir = colour == PLAYER_WHITE ? -1 : 1;

            int start = colour == PLAYER_WHITE ? 6 : 1;

            if (dx == 0 && target == 0) {

                if (dy == dir) return true;

                if (fy == start && dy == 2 * dir && p->board[fy + dir][fx] == 0) return true;

            }

            if (ax == 1 && dy == dir) {

                if (target && Colour(target) != colour) return true;

                if (!target && tx == p->enPassantX && ty == p->enPassantY &&

                    p->board[fy][tx] == (colour == PLAYER_WHITE ? -1 : 1)) return true;

            }

            return false;

        }

        case 2: return ax == ay && ax != 0 && ClearPath(p, fx, fy, tx, ty);

        case 3: return (ax == 1 && ay == 2) || (ax == 2 && ay == 1);

        case 4: return ((dx == 0 && dy != 0) || (dy == 0 && dx != 0)) && ClearPath(p, fx, fy, tx, ty);

        case 5: return ((ax == ay && ax != 0) || (dx == 0 && dy != 0) || (dy == 0 && dx != 0)) && ClearPath(p, fx, fy, tx, ty);

        case 6:

            if (ax <= 1 && ay <= 1) return true;

            if (dy != 0 || ax != 2 || fx != 4 || fy != (colour == PLAYER_WHITE ? 7 : 0) || target) return false;

            {

                int side = dx > 0 ? 1 : 0;

                int rookX = side ? 7 : 0;

                int rook = colour == PLAYER_WHITE ? 4 : -4;

                int step = side ? 1 : -1;

                if (!p->castle[colour][side] || p->board[fy][rookX] != rook) return false;

                if (!ClearPath(p, fx, fy, rookX, fy)) return false;

                if (KingInCheck(p, colour) ||

                    SquareAttacked(p, fx + step, fy, 1 - colour) ||

                    SquareAttacked(p, tx, ty, 1 - colour)) return false;

                return true;

            }

    }

    return false;

}



/* STATE TRANSITION FOR A MOVE ALREADY VALIDATED BY THE CALLER
 * Mutates a Position: update rook/king castling rights; move the rook for
 * castling; remove the captured pawn for en passant; move the piece; auto-
 * promote a pawn reaching the last rank to a queen; record the one-turn
 * en-passant destination after a two-square pawn advance; swap the turn.
 * This function does NOT itself check legality, so callers must validate.
 * It is also used on a temporary copy by LegalMove to test king safety.
 */
static void ApplyMove(Position *p, int fx, int fy, int tx, int ty)

{

    int piece = p->board[fy][fx], captured = p->board[ty][tx];

    int colour = Colour(piece);

    /* Capturing a rook on its original square removes the corresponding right. */

    if (abs(captured) == 4 && (ty == 0 || ty == 7)) {

        int owner = Colour(captured);

        if (ty == (owner == PLAYER_WHITE ? 7 : 0)) {

            if (tx == 0) p->castle[owner][0] = false;

            if (tx == 7) p->castle[owner][1] = false;

        }

    }

    if (abs(piece) == 6) {

        p->castle[colour][0] = p->castle[colour][1] = false;

        if (abs(tx - fx) == 2) {

            int rookFrom = tx > fx ? 7 : 0;

            int rookTo = tx > fx ? 5 : 3;

            p->board[fy][rookTo] = p->board[fy][rookFrom];

            p->board[fy][rookFrom] = 0;

        }

    }

    if (abs(piece) == 4 && fy == (colour == PLAYER_WHITE ? 7 : 0)) {

        if (fx == 0) p->castle[colour][0] = false;

        if (fx == 7) p->castle[colour][1] = false;

    }

    if (abs(piece) == 1 && tx != fx && !captured)

        p->board[fy][tx] = 0; /* En passant: remove pawn passed over. */



    p->board[ty][tx] = piece;

    p->board[fy][fx] = 0;

    if (abs(piece) == 1 && (ty == 0 || ty == 7))

        p->board[ty][tx] = colour == PLAYER_WHITE ? 5 : -5; /* Auto-queen. */



    p->enPassantX = p->enPassantY = -1;

    if (abs(piece) == 1 && abs(ty - fy) == 2) {

        p->enPassantX = fx;

        p->enPassantY = (fy + ty) / 2;

    }

    p->turn = 1 - p->turn;

}



/* FULL MOVE LEGALITY
 * Ensure the source belongs to the player whose turn it is and passes the
 * piece-specific PseudoLegal rules. Copy Position by value, simulate the move
 * on that copy, then reject if the moving side's king is attacked afterward.
 * This catches pinned-piece moves and moving into check without changing the
 * actual game board during validation.
 */
static bool LegalMove(const Position *p, int fx, int fy, int tx, int ty)

{

    if (!Inside(fx, fy) || !Inside(tx, ty)) return false;

    int piece = p->board[fy][fx];

    if (!piece || Colour(piece) != p->turn || !PseudoLegal(p, fx, fy, tx, ty)) return false;

    Position next = *p;

    ApplyMove(&next, fx, fy, tx, ty);

    return !KingInCheck(&next, p->turn);

}



/* SEARCH FOR ANY LEGAL RESPONSE
 * For each piece belonging to the current player, test all 64 possible
 * destinations. Return immediately on the first legal move. If none exists,
 * RefreshStatus distinguishes checkmate from stalemate via KingInCheck.
 * Simple brute-force search is fine for a small two-player chess game;
 * more advanced engines generate candidate moves instead of trying all 64.
 */
static bool HasLegalMove(const Position *p)

{

    for (int fy = 0; fy < BOARD; fy++)

        for (int fx = 0; fx < BOARD; fx++)

            if (p->board[fy][fx] && Colour(p->board[fy][fx]) == p->turn)

                for (int ty = 0; ty < BOARD; ty++)

                    for (int tx = 0; tx < BOARD; tx++)

                        if (LegalMove(p, fx, fy, tx, ty)) return true;

    return false;

}



/* GAME STATUS / END CONDITIONS
 * If the current player has no legal moves and is in check -> checkmate,
 * with the opposing player declared winner. No moves but not in check ->
 * stalemate. Otherwise display whose turn it is and whether they are checked.
 * This implementation does not include repetition, the fifty-move rule,
 * or insufficient-material draw detection.
 */
static void RefreshStatus(void)

{

    bool check = KingInCheck(&game, game.turn);

    if (!HasLegalMove(&game)) {

        if (check) {

            gameResult = game.turn == PLAYER_WHITE ? 2 : 1;

            snprintf(statusMessage, sizeof(statusMessage), "Checkmate! %s wins. Press R to restart.",

                     gameResult == 1 ? "White" : "Black");

        } else {

            gameResult = 3;

            snprintf(statusMessage, sizeof(statusMessage), "Stalemate - draw. Press R to restart.");

        }

    } else {

        snprintf(statusMessage, sizeof(statusMessage), "%s to move%s  |  R: restart",

                 game.turn == PLAYER_WHITE ? "White" : "Black", check ? " - CHECK!" : "");

    }

}



/* RENDERING LABEL LOOKUP
 * Convert the absolute piece ID to a single letter. Colour is encoded by
 * sign elsewhere, so one mapping serves both sides. This game draws letters
 * rather than sprite textures for the chess pieces.
 */
static char PieceChar(int piece)

{

    switch (abs(piece)) {

        case 1: return 'P'; case 2: return 'B'; case 3: return 'N';

        case 4: return 'R'; case 5: return 'Q'; case 6: return 'K';

        default: return '?';

    }

}



/* FRAME RENDERING USING RAYLIB
 * Draw alternating 80px board tiles, highlight the selected square, and
 * display dots for every LegalMove destination from the selected piece.
 * Draw each occupied square as a piece letter with colour-dependent ink.
 * Draw a HUD underneath with game status and control instructions.
 * Note: checking all destinations during drawing is straightforward but can
 * be optimised by caching legal destinations when selection/state changes.
 */
static void DrawBoard(void)

{

    for (int y = 0; y < BOARD; y++) {

        for (int x = 0; x < BOARD; x++) {

            Color square = ((x + y) % 2 == 0) ? BEIGE : BROWN;

            DrawRectangle(x * TILE, y * TILE, TILE, TILE, square);

            if (x == selectedX && y == selectedY)

                DrawRectangle(x * TILE, y * TILE, TILE, TILE, Fade(YELLOW, 0.50f));

            if (selectedX >= 0 && selectedY >= 0 &&

                LegalMove(&game, selectedX, selectedY, x, y)) {

                DrawCircle(x * TILE + TILE / 2, y * TILE + TILE / 2, 11, Fade(DARKGREEN, 0.65f));

            }

            int piece = game.board[y][x];

            if (piece) {

                char label[2] = { PieceChar(piece), '\0' };

                Color ink = piece > 0 ? BLACK : WHITE;

                DrawText(label, x * TILE + TILE / 2 - 13, y * TILE + TILE / 2 - 22, 44, ink);

            }

        }

    }

    DrawRectangle(0, BOARD * TILE, BOARD * TILE, HUD, DARKGRAY);

    DrawText(statusMessage, 12, BOARD * TILE + 12, 19, RAYWHITE);

    DrawText("Click a piece, then its destination. Pawns promote to queens.",

             12, BOARD * TILE + 41, 14, LIGHTGRAY);

}



/* INPUT + RENDER LOOP (CALLED EACH FRAME BY EMSCRIPTEN)
 * R resets the game. A mouse click selects a piece belonging to the current
 * player; clicking it again deselects; clicking a legal destination applies
 * the move and recalculates status; clicking another own piece reselects.
 * Once gameResult is nonzero, board moves are disabled until reset.
 * Every frame finishes with BeginDrawing/DrawBoard/EndDrawing.
 */
static void UpdateDrawFrame(void)

{

    if (IsKeyPressed(KEY_R)) ResetGame();

    if (!gameResult && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

        Vector2 mouse = GetMousePosition();

        int x = (int)mouse.x / TILE, y = (int)mouse.y / TILE;

        if (mouse.x >= 0 && mouse.y >= 0 && Inside(x, y)) {

            int piece = game.board[y][x];

            if (selectedX < 0) {

                if (piece && Colour(piece) == game.turn) {

                    selectedX = x; selectedY = y;

                }

            } else if (x == selectedX && y == selectedY) {

                selectedX = selectedY = -1;

            } else if (LegalMove(&game, selectedX, selectedY, x, y)) {

                ApplyMove(&game, selectedX, selectedY, x, y);

                selectedX = selectedY = -1;

                RefreshStatus();

            } else if (piece && Colour(piece) == game.turn) {

                selectedX = x; selectedY = y;

            } else {

                selectedX = selectedY = -1;

            }

        }

    }

    BeginDrawing();

    ClearBackground(LIGHTGRAY);

    DrawBoard();

    EndDrawing();

}



/* PROGRAM ENTRY POINT
 * Create the Raylib window, target 60 frames/second, initialise game state,
 * then register UpdateDrawFrame as the browser-friendly main-loop callback.
 * Emscripten owns the recurring callback scheduling; do not add a separate
 * blocking while loop here. Native desktop builds would typically use a
 * normal WindowShouldClose loop instead.
 */
int main(void)

{

    InitWindow(BOARD * TILE, BOARD * TILE + HUD, "Chess Web");

    SetTargetFPS(60);

    ResetGame();

    RefreshStatus();

    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);

    return 0;

}


   
