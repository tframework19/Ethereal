/*
  Ethereal is a UCI chess playing engine authored by Andrew Grant.
  <https://github.com/AndyGrant/Ethereal>     <andrew@grantnet.us>

  Ethereal is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Ethereal is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "attacks.h"
#include "bitboards.h"
#include "masks.h"
#include "types.h"

int DistanceBetween[SQUARE_NB][SQUARE_NB];
uint64_t BitsBetweenMasks[SQUARE_NB][SQUARE_NB];
uint64_t KingAreaMasks[COLOUR_NB][SQUARE_NB];
uint64_t ForwardRanksMasks[COLOUR_NB][RANK_NB];
uint64_t AdjacentFilesMasks[FILE_NB];
uint64_t PassedPawnMasks[COLOUR_NB][SQUARE_NB];
uint64_t PawnConnectedMasks[COLOUR_NB][SQUARE_NB];
uint64_t OutpostSquareMasks[COLOUR_NB][SQUARE_NB];
uint64_t OutpostRanksMasks[COLOUR_NB];

void initMasks() {

    // Initialize a table for the distance between two given squares
    for (int sq1 = 0; sq1 < SQUARE_NB; sq1++)
        for (int sq2 = 0; sq2 < SQUARE_NB; sq2++)
            DistanceBetween[sq1][sq2] = MAX(abs(fileOf(sq1)-fileOf(sq2)), abs(rankOf(sq1)-rankOf(sq2)));

    // Initialize a table of bitmasks for the squares between two given ones (aligned on diagonal)
    for (int sq1 = 0; sq1 < SQUARE_NB; sq1++)
        for (int sq2 = 0; sq2 < SQUARE_NB; sq2++)
            if (testBit(bishopAttacks(sq1, 0ull), sq2))
                BitsBetweenMasks[sq1][sq2] = bishopAttacks(sq1, 1ull << sq2)
                                           & bishopAttacks(sq2, 1ull << sq1);

    // Initialize a table of bitmasks for the squares between two given ones (aligned on a straight)
    for (int sq1 = 0; sq1 < SQUARE_NB; sq1++)
        for (int sq2 = 0; sq2 < SQUARE_NB; sq2++)
            if (testBit(rookAttacks(sq1, 0ull), sq2))
                BitsBetweenMasks[sq1][sq2] = rookAttacks(sq1, 1ull << sq2)
                                           & rookAttacks(sq2, 1ull << sq1);

    // Initialize a table for the King Areas. Use the King's square, the King's target
    // squares, and the squares within the pawn shield. When on the A/H files, extend
    // the King Area to include an additional file, namely the C and F file respectively
    for (int sq = 0; sq < SQUARE_NB; sq++) {

        KingAreaMasks[WHITE][sq] = kingAttacks(sq) | (1ull << sq) | (kingAttacks(sq) << 8);
        KingAreaMasks[BLACK][sq] = kingAttacks(sq) | (1ull << sq) | (kingAttacks(sq) >> 8);

        KingAreaMasks[WHITE][sq] |= fileOf(sq) != 0 ? 0ull : KingAreaMasks[WHITE][sq] << 1;
        KingAreaMasks[BLACK][sq] |= fileOf(sq) != 0 ? 0ull : KingAreaMasks[BLACK][sq] << 1;

        KingAreaMasks[WHITE][sq] |= fileOf(sq) != 7 ? 0ull : KingAreaMasks[WHITE][sq] >> 1;
        KingAreaMasks[BLACK][sq] |= fileOf(sq) != 7 ? 0ull : KingAreaMasks[BLACK][sq] >> 1;
    }

    // Initialize a table of bitmasks for the ranks at or above a given rank, by colour
    for (int r = 0; r < RANK_NB; r++) {
        for (int i = r; i < RANK_NB; i++)
            ForwardRanksMasks[WHITE][r] |= Ranks[i];
        ForwardRanksMasks[BLACK][r] = ~ForwardRanksMasks[WHITE][r] | Ranks[r];
    }

    // Initialize a table for the bitboard containing the files next to a given file
    for (int f = 0; f < FILE_NB; f++) {
        AdjacentFilesMasks[f]  = Files[MAX(0, f-1)];
        AdjacentFilesMasks[f] |= Files[MIN(FILE_NB-1, f+1)];
        AdjacentFilesMasks[f] &= ~Files[f];
    }

    // Initialize a table of bitmasks to check if a given pawn has any opposition
    for (int c = 0; c < COLOUR_NB; c++)
        for (int sq = 0; sq < SQUARE_NB; sq++)
            PassedPawnMasks[c][sq] = ~forwardRanksMasks(!c, rankOf(sq))
                                   & (adjacentFilesMasks(fileOf(sq)) | Files[fileOf(sq)]);

    // Initialize a table of bitmasks to check if a square is an outpost relative
    // to opposing pawns, such that no enemy pawn may attack the square with ease
    for (int c = 0; c < COLOUR_NB; c++)
        for (int sq = 0; sq < SQUARE_NB; sq++)
            OutpostSquareMasks[c][sq] = PassedPawnMasks[c][sq] & ~Files[fileOf(sq)];

    // Initialize a pair of bitmasks to check if a square may be an outpost
    OutpostRanksMasks[WHITE] = RANK_4 | RANK_5 | RANK_6;
    OutpostRanksMasks[BLACK] = RANK_3 | RANK_4 | RANK_5;

    // Initialize a table of bitmasks to check for supports for a given pawn
    for (int s = 8 ; s < 56; s++) {
        PawnConnectedMasks[WHITE][s] = pawnAttacks(BLACK, s) | pawnAttacks(BLACK, s + 8);
        PawnConnectedMasks[BLACK][s] = pawnAttacks(WHITE, s) | pawnAttacks(WHITE, s - 8);
    }
}

int distanceBetween(int s1, int s2) {
    assert(0 <= s1 && s1 < SQUARE_NB);
    assert(0 <= s2 && s2 < SQUARE_NB);
    return DistanceBetween[s1][s2];
}

uint64_t bitsBetweenMasks(int s1, int s2) {
    assert(0 <= s1 && s1 < SQUARE_NB);
    assert(0 <= s2 && s2 < SQUARE_NB);
    return BitsBetweenMasks[s1][s2];
}

uint64_t kingAreaMasks(int colour, int sq) {
    assert(0 <= colour && colour < COLOUR_NB);
    assert(0 <= sq && sq < SQUARE_NB);
    return KingAreaMasks[colour][sq];
}

uint64_t forwardRanksMasks(int colour, int rank) {
    assert(0 <= colour && colour < COLOUR_NB);
    assert(0 <= rank && rank < RANK_NB);
    return ForwardRanksMasks[colour][rank];
}

uint64_t adjacentFilesMasks(int file) {
    assert(0 <= file && file < FILE_NB);
    return AdjacentFilesMasks[file];
}

uint64_t passedPawnMasks(int colour, int sq) {
    assert(0 <= colour && colour < COLOUR_NB);
    assert(0 <= sq && sq < SQUARE_NB);
    return PassedPawnMasks[colour][sq];
}

uint64_t pawnConnectedMasks(int colour, int sq) {
    assert(0 <= colour && colour < COLOUR_NB);
    assert(0 <= sq && sq < SQUARE_NB);
    return PawnConnectedMasks[colour][sq];
}

uint64_t outpostSquareMasks(int colour, int sq) {
    assert(0 <= colour && colour < COLOUR_NB);
    assert(0 <= sq && sq < SQUARE_NB);
    return OutpostSquareMasks[colour][sq];
}

uint64_t outpostRanksMasks(int colour) {
    assert(0 <= colour && colour < COLOUR_NB);
    return OutpostRanksMasks[colour];
}
