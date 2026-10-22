// Chess-2.c Fully Functional, Playable Chess.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define SIZE 8

typedef enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING } PieceType;
typedef enum { NONE, WHITE, BLACK } Colour;

typedef struct {
    PieceType type;
    Colour    colour;
} Piece;

typedef struct {
    Piece board[SIZE][SIZE];
    Colour turn;

    // Castling
    int w_king_moved;
    int w_rook_a_moved;
    int w_rook_h_moved;
    int b_king_moved;
    int b_rook_a_moved;
    int b_rook_h_moved;

    // En-Passant target
    int ep_col;
} Game;

// Declarations
static void clear_screen(void);
static void init_board(Game *g);
static void print_board(const Game *g);
static int  is_attacked(const Game *g, int r, int c, Colour attacker);
static int  is_in_check(const Game *g, Colour col);
static int  is_pseudo_valid_move(const Game *g, int sr, int sc, int dr, int dc);
static int  simulate_move(const Game *g, Game *out, int sr, int sc, int dr, int dc, char promo_ch);
static int  has_legal_moves(const Game *g);
static int  make_move(Game *g, const char *mv);

int main(void) {
    Game game;
    init_board(&game);

    char line[32];
    char status_msg[128] = "";

    while (1) {
        clear_screen();
        print_board(&game);

        if (status_msg[0] != '\0') {
            printf("%s\n", status_msg);
            status_msg[0] = '\0';
        }

        int in_check = is_in_check(&game, game.turn);
        if (!has_legal_moves(&game)) {
            if (in_check) {
                Colour winner = (game.turn == WHITE) ? BLACK : WHITE;
                printf("*** CHECKMATE! %s wins the game! ***\n\n", (winner == WHITE) ? "White" : "Black");
            } else {
                printf("*** STALEMATE! The game ends in a draw. ***\n\n");
            }
            break;
        }

        if (in_check) {
            printf("** CHECK! **\n");
        }

        printf("%s to move (e.g. e2e4, e7e8q)> ", (game.turn == WHITE) ? "White" : "Black");
        if (!fgets(line, sizeof line, stdin)) break;

        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "quit") == 0) break;

        if (!make_move(&game, line)) {
            snprintf(status_msg, sizeof status_msg, "Invalid move. Use format e2e4 (or e7e8q for promotion) or 'quit'.");
        }
    }
    return 0;
}

// Definitions

static void clear_screen(void) {
    // VT100 escape sequence
    printf("\033[H\033[2J");
    fflush(stdout);
}

static void init_board(Game *g) {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            g->board[r][c].type   = EMPTY;
            g->board[r][c].colour = NONE;
        }
    }

    Colour colours[2] = { WHITE, BLACK };
    int pawnRow[2]    = { 6, 1 };
    int majorRow[2]   = { 7, 0 };

    for (int i = 0; i < 2; ++i) {
        Colour col = colours[i];
        int pr = pawnRow[i];
        int mr = majorRow[i];

        for (int c = 0; c < SIZE; ++c) {
            g->board[pr][c].type   = PAWN;
            g->board[pr][c].colour = col;
        }

        PieceType order[8] = {
            ROOK, KNIGHT, BISHOP, QUEEN,
            KING, BISHOP, KNIGHT, ROOK
        };
        for (int c = 0; c < SIZE; ++c) {
            g->board[mr][c].type   = order[c];
            g->board[mr][c].colour = col;
        }
    }

    g->turn = WHITE;
    g->w_king_moved = g->w_rook_a_moved = g->w_rook_h_moved = 0;
    g->b_king_moved = g->b_rook_a_moved = g->b_rook_h_moved = 0;
    g->ep_col = -1;
}

static void print_board(const Game *g) {
    const char *symbols = ".PNBRQKpnbrqk";
    
    printf("\n    a   b   c   d   e   f   g   h\n");
    printf("  +---+---+---+---+---+---+---+---+\n");
    
    for (int r = 0; r < SIZE; ++r) {
        printf("%d |", 8 - r);
        for (int c = 0; c < SIZE; ++c) {
            const Piece *p = &g->board[r][c];
            char ch = '.';
            if (p->type != EMPTY) {
                int idx = p->type + (p->colour == WHITE ? 0 : 6);
                ch = symbols[idx];
            }
            printf(" %c |", ch);
        }
        printf(" %d\n", 8 - r);
        printf("  +---+---+---+---+---+---+---+---+\n");
    }
    printf("    a   b   c   d   e   f   g   h\n\n");
}

// Check if attacked by opponent
static int is_attacked(const Game *g, int r, int c, Colour attacker) {
    int pawn_dir = (attacker == WHITE) ? 1 : -1;
    
    // Pawn attacks
    if (r + pawn_dir >= 0 && r + pawn_dir < SIZE) {
        if (c - 1 >= 0 && g->board[r + pawn_dir][c - 1].type == PAWN && g->board[r + pawn_dir][c - 1].colour == attacker) return 1;
        if (c + 1 < SIZE && g->board[r + pawn_dir][c + 1].type == PAWN && g->board[r + pawn_dir][c + 1].colour == attacker) return 1;
    }

    // Knight attacks
    int knight_moves[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (int i = 0; i < 8; ++i) {
        int nr = r + knight_moves[i][0];
        int nc = c + knight_moves[i][1];
        if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE) {
            if (g->board[nr][nc].type == KNIGHT && g->board[nr][nc].colour == attacker) return 1;
        }
    }

    // King attacks
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            int kr = r + dr, kc = c + dc;
            if (kr >= 0 && kr < SIZE && kc >= 0 && kc < SIZE) {
                if (g->board[kr][kc].type == KING && g->board[kr][kc].colour == attacker) return 1;
            }
        }
    }

    // Rook, Queen
    int straight[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    for (int i = 0; i < 4; ++i) {
        int step = 1;
        while (1) {
            int nr = r + straight[i][0] * step;
            int nc = c + straight[i][1] * step;
            if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) break;
            const Piece *p = &g->board[nr][nc];
            if (p->type != EMPTY) {
                if (p->colour == attacker && (p->type == ROOK || p->type == QUEEN)) return 1;
                break;
            }
            step++;
        }
    }

    // Bishop, Queen
    int diag[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
    for (int i = 0; i < 4; ++i) {
        int step = 1;
        while (1) {
            int nr = r + diag[i][0] * step;
            int nc = c + diag[i][1] * step;
            if (nr < 0 || nr >= SIZE || nc < 0 || nc >= SIZE) break;
            const Piece *p = &g->board[nr][nc];
            if (p->type != EMPTY) {
                if (p->colour == attacker && (p->type == BISHOP || p->type == QUEEN)) return 1;
                break;
            }
            step++;
        }
    }

    return 0;
}

// King is in check
static int is_in_check(const Game *g, Colour col) {
    int kr = -1, kc = -1;
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (g->board[r][c].type == KING && g->board[r][c].colour == col) {
                kr = r; kc = c;
                break;
            }
        }
    }
    if (kr == -1) return 0;
    Colour enemy = (col == WHITE) ? BLACK : WHITE;
    return is_attacked(g, kr, kc, enemy);
}

static int is_pseudo_valid_move(const Game *g, int sr, int sc, int dr, int dc) {
    const Piece *src = &g->board[sr][sc];
    const Piece *dst = &g->board[dr][dc];

    if (src->type == EMPTY || src->colour != g->turn) return 0;
    if (dst->colour == g->turn) return 0;

    int dr_abs = dr - sr;
    int dc_abs = dc - sc;

    // Pawn 
    if (src->type == PAWN) {
        int forward = (src->colour == WHITE) ? -1 : 1;
        
        if (dc_abs == 0 && dr_abs == forward && dst->type == EMPTY) return 1;
        if (dc_abs == 0 && dr_abs == 2 * forward &&
            ((src->colour == WHITE && sr == 6) || (src->colour == BLACK && sr == 1)) &&
            g->board[sr + forward][sc].type == EMPTY && dst->type == EMPTY)
            return 1;
        if (abs(dc_abs) == 1 && dr_abs == forward && dst->type != EMPTY) return 1;

        // En-Passant capture
        if (abs(dc_abs) == 1 && dr_abs == forward && dst->type == EMPTY && dc == g->ep_col) {
            if ((src->colour == WHITE && sr == 3) || (src->colour == BLACK && sr == 4))
                return 1;
        }

        return 0;
    }

    // Knight
    if (src->type == KNIGHT) {
        return (abs(dr_abs) == 2 && abs(dc_abs) == 1) ||
               (abs(dr_abs) == 1 && abs(dc_abs) == 2);
    }

    // Sliding pieces
    if (src->type == BISHOP || src->type == ROOK || src->type == QUEEN) {
        if (src->type == BISHOP && abs(dr_abs) != abs(dc_abs)) return 0;
        if (src->type == ROOK && !(dr_abs == 0 || dc_abs == 0)) return 0;
        if (src->type == QUEEN && !(dr_abs == 0 || dc_abs == 0 || abs(dr_abs) == abs(dc_abs))) return 0;

        int dr_sgn = (dr_abs > 0) - (dr_abs < 0);
        int dc_sgn = (dc_abs > 0) - (dc_abs < 0);
        int steps  = (abs(dr_abs) > abs(dc_abs)) ? abs(dr_abs) : abs(dc_abs);

        for (int step = 1; step < steps; ++step) {
            int r = sr + dr_sgn * step;
            int c = sc + dc_sgn * step;
            if (g->board[r][c].type != EMPTY) return 0;
        }
        return 1;
    }

    // King & Castling
    if (src->type == KING) {
        if (abs(dr_abs) <= 1 && abs(dc_abs) <= 1) return 1;

        Colour enemy = (g->turn == WHITE) ? BLACK : WHITE;
        if (dr_abs == 0 && abs(dc_abs) == 2 && !is_in_check(g, g->turn)) {
            if (g->turn == WHITE && sr == 7 && sc == 4) {
                if (dc == 6 && !g->w_king_moved && !g->w_rook_h_moved &&
                    g->board[7][5].type == EMPTY && g->board[7][6].type == EMPTY &&
                    !is_attacked(g, 7, 5, enemy) && !is_attacked(g, 7, 6, enemy))
                    return 1;
                if (dc == 2 && !g->w_king_moved && !g->w_rook_a_moved &&
                    g->board[7][1].type == EMPTY && g->board[7][2].type == EMPTY && g->board[7][3].type == EMPTY &&
                    !is_attacked(g, 7, 3, enemy) && !is_attacked(g, 7, 2, enemy))
                    return 1;
            } else if (g->turn == BLACK && sr == 0 && sc == 4) {
                if (dc == 6 && !g->b_king_moved && !g->b_rook_h_moved &&
                    g->board[0][5].type == EMPTY && g->board[0][6].type == EMPTY &&
                    !is_attacked(g, 0, 5, enemy) && !is_attacked(g, 0, 6, enemy))
                    return 1;
                if (dc == 2 && !g->b_king_moved && !g->b_rook_a_moved &&
                    g->board[0][1].type == EMPTY && g->board[0][2].type == EMPTY && g->board[0][3].type == EMPTY &&
                    !is_attacked(g, 0, 3, enemy) && !is_attacked(g, 0, 2, enemy))
                    return 1;
            }
        }
    }

    return 0;
}

static int simulate_move(const Game *g, Game *out, int sr, int sc, int dr, int dc, char promo_ch) {
    if (!is_pseudo_valid_move(g, sr, sc, dr, dc)) return 0;

    *out = *g;

    // En-Passant capture 
    if (out->board[sr][sc].type == PAWN && sc != dc && out->board[dr][dc].type == EMPTY) {
        out->board[sr][dc].type   = EMPTY;
        out->board[sr][dc].colour = NONE;
    }

    out->board[dr][dc] = out->board[sr][sc];
    out->board[sr][sc].type   = EMPTY;
    out->board[sr][sc].colour = NONE;

    // Castling Rook
    if (out->board[dr][dc].type == KING && abs(dc - sc) == 2) {
        if (dc == 6) {
            out->board[dr][5] = out->board[dr][7];
            out->board[dr][7].type = EMPTY; out->board[dr][7].colour = NONE;
        } else if (dc == 2) {
            out->board[dr][3] = out->board[dr][0];
            out->board[dr][0].type = EMPTY; out->board[dr][0].colour = NONE;
        }
    }

    // Pawn Promotion
    if (out->board[dr][dc].type == PAWN && (dr == 0 || dr == 7)) {
        PieceType pt = QUEEN;
        if (promo_ch == 'r') pt = ROOK;
        else if (promo_ch == 'b') pt = BISHOP;
        else if (promo_ch == 'n') pt = KNIGHT;
        out->board[dr][dc].type = pt;
    }

    // Disallow self-check
    if (is_in_check(out, g->turn)) {
        return 0;
    }

    return 1;
}

static int has_legal_moves(const Game *g) {
    Game temp;
    for (int sr = 0; sr < SIZE; ++sr) {
        for (int sc = 0; sc < SIZE; ++sc) {
            if (g->board[sr][sc].colour != g->turn) continue;
            for (int dr = 0; dr < SIZE; ++dr) {
                for (int dc = 0; dc < SIZE; ++dc) {
                    if (simulate_move(g, &temp, sr, sc, dr, dc, 'q')) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int make_move(Game *g, const char *mv) {
    if (strlen(mv) < 4) return 0;
    int sc = mv[0] - 'a';
    int sr = 8 - (mv[1] - '0');
    int dc = mv[2] - 'a';
    int dr = 8 - (mv[3] - '0');

    char promo_ch = (strlen(mv) >= 5) ? (char)tolower((unsigned char)mv[4]) : 'q';

    if (!(0 <= sr && sr < SIZE && 0 <= sc && sc < SIZE &&
          0 <= dr && dr < SIZE && 0 <= dc && dc < SIZE))
        return 0;

    Game temp;
    if (!simulate_move(g, &temp, sr, sc, dr, dc, promo_ch)) return 0;

    *g = temp;

    // Update En-Passant state 
    if (g->board[dr][dc].type == PAWN && abs(dr - sr) == 2) {
        g->ep_col = sc;
    } else {
        g->ep_col = -1;
    }

    // Castling
    if (sr == 7 && sc == 4) g->w_king_moved = 1;
    if (sr == 7 && sc == 0) g->w_rook_a_moved = 1;
    if (sr == 7 && sc == 7) g->w_rook_h_moved = 1;
    if (sr == 0 && sc == 4) g->b_king_moved = 1;
    if (sr == 0 && sc == 0) g->b_rook_a_moved = 1;
    if (sr == 0 && sc == 7) g->b_rook_h_moved = 1;

    g->turn = (g->turn == WHITE) ? BLACK : WHITE;

    return 1;
}
