    #include "MapManager.h"
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <utility>
#include <limits>
#include <algorithm>
#include <random>

constexpr bool kTesterEnabledBuild = true;

MapManager::MapManager()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    initializeMap();
}

MapManager::~MapManager()
{
    clean();
}

void MapManager::initializeMap()
{
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            map[r][c] = FLOOR;
            safePath[r][c] = false;

            visible[r][c] = false;
            shadowVisible[r][c] = false;
            torchState[r][c] = TORCH_NOT_USED;
        }
    }

    map[goalRow][goalCol] = GOAL;
    hintTorchRow = -1;
    hintTorchCol = -1;
    hintUntilTick = 0;
}

void MapManager::revealInitialArea()
{
    int revealRadiusValue = 1;
    if (currentDifficulty == Difficulty::MEDIUM)
        revealRadiusValue = 0;
    else if (currentDifficulty == Difficulty::HARD)
        revealRadiusValue = 0;

    revealRadius(startRow, startCol, revealRadiusValue, true);
}

void MapManager::ensureStarterTorch()
{
    int bestRow = -1;
    int bestCol = -1;
    int bestDistance = std::numeric_limits<int>::max();

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            int distance = std::abs(r - startRow) + std::abs(c - startCol);
            if (distance == 0 || distance > 3)
                continue;
            if (r == goalRow && c == goalCol)
                continue;

            if (map[r][c] == TORCH)
            {
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestRow = r;
                    bestCol = c;
                }
                continue;
            }

            if ((map[r][c] == FLOOR || map[r][c] == MINE) && distance < bestDistance)
            {
                bestDistance = distance;
                bestRow = r;
                bestCol = c;
            }
        }
    }

    if (bestRow == -1 || bestCol == -1)
    {
        int fallbackRow = startRow;
        int fallbackCol = startCol + 1;
        if (fallbackCol >= COLS)
            fallbackCol = startCol;

        if (fallbackRow == goalRow && fallbackCol == goalCol)
            return;

        if (map[fallbackRow][fallbackCol] == FLOOR || map[fallbackRow][fallbackCol] == MINE)
        {
            map[fallbackRow][fallbackCol] = TORCH;
            torchState[fallbackRow][fallbackCol] = TORCH_NOT_USED;
        }
        return;
    }

    map[bestRow][bestCol] = TORCH;
    torchState[bestRow][bestCol] = TORCH_NOT_USED;
}

void MapManager::placeRandomTiles(int tileType, int count)
{
    if (tileType == MINE)
    {
        std::vector<std::pair<int, int>> candidates;
        candidates.reserve(ROWS * COLS);

        for (int r = 0; r < ROWS; r++)
        {
            for (int c = 0; c < COLS; c++)
            {
                if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
                    continue;
                if (safePath[r][c])
                    continue;
                if (map[r][c] != FLOOR)
                    continue;

                candidates.push_back({r, c});
            }
        }

        if (candidates.empty())
            return;

        static std::mt19937 mineRng(static_cast<unsigned int>(std::time(nullptr)) + 811U);
        std::shuffle(candidates.begin(), candidates.end(), mineRng);

        auto rowMineCount = [this](int row) -> int
        {
            int countMines = 0;
            for (int cc = 0; cc < COLS; cc++)
            {
                if (map[row][cc] == MINE)
                    countMines++;
            }
            return countMines;
        };

        auto colMineCount = [this](int col) -> int
        {
            int countMines = 0;
            for (int rr = 0; rr < ROWS; rr++)
            {
                if (map[rr][col] == MINE)
                    countMines++;
            }
            return countMines;
        };

        auto nearestTorchDistance = [this](int row, int col) -> int
        {
            int best = ROWS + COLS;
            for (int r = 0; r < ROWS; r++)
            {
                for (int c = 0; c < COLS; c++)
                {
                    if (map[r][c] != TORCH)
                        continue;
                    int d = std::abs(r - row) + std::abs(c - col);
                    if (d < best)
                        best = d;
                }
            }
            return best;
        };

        const int zoneRows = 3;
        const int zoneCols = 4;
        auto getZoneIndex = [zoneRows, zoneCols, this](int row, int col) -> int
        {
            int zr = (row * zoneRows) / ROWS;
            int zc = (col * zoneCols) / COLS;
            if (zr < 0) zr = 0;
            if (zr >= zoneRows) zr = zoneRows - 1;
            if (zc < 0) zc = 0;
            if (zc >= zoneCols) zc = zoneCols - 1;
            return zr * zoneCols + zc;
        };

        auto zoneMineCount = [this, getZoneIndex](int row, int col) -> int
        {
            int zone = getZoneIndex(row, col);
            int countMines = 0;
            for (int rr = 0; rr < ROWS; rr++)
            {
                for (int cc = 0; cc < COLS; cc++)
                {
                    if (getZoneIndex(rr, cc) != zone)
                        continue;
                    if (map[rr][cc] == MINE)
                        countMines++;
                }
            }
            return countMines;
        };

        auto nearbyMineInRadius2 = [this](int row, int col) -> int
        {
            int countMines = 0;
            for (int dr = -2; dr <= 2; dr++)
            {
                for (int dc = -2; dc <= 2; dc++)
                {
                    if (dr == 0 && dc == 0)
                        continue;
                    if (std::abs(dr) + std::abs(dc) > 2)
                        continue;

                    int rr = row + dr;
                    int cc = col + dc;
                    if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS)
                        continue;

                    if (map[rr][cc] == MINE)
                        countMines++;
                }
            }
            return countMines;
        };

        int placed = 0;
        auto placeByRule = [&](int maxAdjacentMines, int preferredTorchDistanceMax) -> void
        {
            while (placed < count)
            {
                int bestIndex = -1;
                int bestScore = std::numeric_limits<int>::max();

                for (size_t i = 0; i < candidates.size(); i++)
                {
                    int r = candidates[i].first;
                    int c = candidates[i].second;

                    if (map[r][c] != FLOOR)
                        continue;

                    int adjacent = countAdjacentMines(r, c);
                    if (adjacent > maxAdjacentMines)
                        continue;

                    if (createsDenseMineCluster(r, c))
                        continue;

                    int torchDistance = nearestTorchDistance(r, c);
                    if (preferredTorchDistanceMax >= 0 && torchDistance > preferredTorchDistanceMax)
                        continue;

                    int score = 0;
                    score += rowMineCount(r) * 18;
                    score += colMineCount(c) * 16;
                    score += zoneMineCount(r, c) * 24;
                    score += nearbyMineInRadius2(r, c) * 22;
                    if (torchDistance <= 2)
                        score += 6;
                    score += std::rand() % 11;

                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestIndex = static_cast<int>(i);
                    }
                }

                if (bestIndex < 0)
                    break;

                int r = candidates[static_cast<size_t>(bestIndex)].first;
                int c = candidates[static_cast<size_t>(bestIndex)].second;
                map[r][c] = MINE;
                placed++;
            }
        };

        placeByRule(0, 4);
        placeByRule(0, -1);
        return;
    }

    int placed = 0;
    int attempts = 0;
    int maxAttempts = ROWS * COLS * 40;

    while (placed < count && attempts < maxAttempts)
    {
        attempts++;
        int r = std::rand() % ROWS;
        int c = std::rand() % COLS;

        if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
            continue;

        if (tileType == MINE)
        {
            if (safePath[r][c])
                continue;

            int startDistance = std::abs(r - startRow) + std::abs(c - startCol);
            int goalDistance = std::abs(r - goalRow) + std::abs(c - goalCol);

            if (startDistance <= 2 || goalDistance <= 1)
                continue;

            int nearbyMines = countAdjacentMines(r, c);
            if (nearbyMines >= 2)
                continue;

            if (nearbyMines == 1 && (std::rand() % 100) < 55)
                continue;

            if (createsDenseMineCluster(r, c))
                continue;
        }

        if (map[r][c] == FLOOR)
        {
            map[r][c] = tileType;
            placed++;
        }
    }

    if (placed >= count)
        return;

    std::vector<std::pair<int, int>> fallbackCandidates;
    fallbackCandidates.reserve(ROWS * COLS);

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
                continue;
            if (map[r][c] != FLOOR)
                continue;
            if (tileType == MINE && safePath[r][c])
                continue;

            fallbackCandidates.push_back({r, c});
        }
    }

    static std::mt19937 fallbackRng(static_cast<unsigned int>(std::time(nullptr)) + 727U);
    std::shuffle(fallbackCandidates.begin(), fallbackCandidates.end(), fallbackRng);

    auto rowMineCount = [this](int row) -> int
    {
        int countMines = 0;
        for (int cc = 0; cc < COLS; cc++)
        {
            if (map[row][cc] == MINE)
                countMines++;
        }
        return countMines;
    };

    auto colMineCount = [this](int col) -> int
    {
        int countMines = 0;
        for (int rr = 0; rr < ROWS; rr++)
        {
            if (map[rr][col] == MINE)
                countMines++;
        }
        return countMines;
    };

    while (placed < count && !fallbackCandidates.empty())
    {
        int bestIdx = -1;
        int bestScore = std::numeric_limits<int>::max();

        int inspectCount = static_cast<int>(fallbackCandidates.size());
        if (inspectCount > 36)
            inspectCount = 36;

        for (int i = 0; i < inspectCount; i++)
        {
            int r = fallbackCandidates[static_cast<size_t>(i)].first;
            int c = fallbackCandidates[static_cast<size_t>(i)].second;

            if (map[r][c] != FLOOR)
                continue;

            if (tileType == MINE)
            {
                int nearbyMines = countAdjacentMines(r, c);
                if (nearbyMines >= 2)
                    continue;
                if (createsDenseMineCluster(r, c))
                    continue;

                int score = nearbyMines * 45;
                score += rowMineCount(r) * 12;
                score += colMineCount(c) * 10;
                score += std::rand() % 9;

                if (score < bestScore)
                {
                    bestScore = score;
                    bestIdx = i;
                }
            }
            else
            {
                bestIdx = i;
                break;
            }
        }

        if (bestIdx == -1)
            bestIdx = 0;

        int r = fallbackCandidates[static_cast<size_t>(bestIdx)].first;
        int c = fallbackCandidates[static_cast<size_t>(bestIdx)].second;
        fallbackCandidates.erase(fallbackCandidates.begin() + bestIdx);

        if (map[r][c] != FLOOR)
            continue;

        if (tileType == MINE)
        {
            if (countAdjacentMines(r, c) >= 3)
                continue;
            if (createsDenseMineCluster(r, c))
                continue;
        }

        map[r][c] = tileType;
        placed++;
    }
}

void MapManager::placeMinesNearTorches(int count)
{
    if (count <= 0)
        return;

    std::vector<std::pair<int, int>> torchCells;
    torchCells.reserve(ROWS * COLS / 3);

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] == TORCH)
                torchCells.push_back({r, c});
        }
    }

    if (torchCells.empty())
        return;

    auto rowMineCount = [this](int row) -> int
    {
        int countMines = 0;
        for (int cc = 0; cc < COLS; cc++)
        {
            if (map[row][cc] == MINE)
                countMines++;
        }
        return countMines;
    };

    auto colMineCount = [this](int col) -> int
    {
        int countMines = 0;
        for (int rr = 0; rr < ROWS; rr++)
        {
            if (map[rr][col] == MINE)
                countMines++;
        }
        return countMines;
    };

    auto torchHasNearbyMine = [this](int row, int col) -> bool
    {
        for (int dr = -2; dr <= 2; dr++)
        {
            for (int dc = -2; dc <= 2; dc++)
            {
                int distance = std::abs(dr) + std::abs(dc);
                if (distance == 0 || distance > 2)
                    continue;

                int rr = row + dr;
                int cc = col + dc;
                if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS)
                    continue;

                if (map[rr][cc] == MINE)
                    return true;
            }
        }
        return false;
    };

    auto isValidMineCell = [this](int row, int col, bool relaxed) -> bool
    {
        if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
            return false;
        if ((row == startRow && col == startCol) || (row == goalRow && col == goalCol))
            return false;
        if (safePath[row][col])
            return false;
        if (map[row][col] != FLOOR)
            return false;
        if (createsDenseMineCluster(row, col))
            return false;

        int adjacent = countAdjacentMines(row, col);
        if (!relaxed && adjacent > 0)
            return false;
        if (relaxed && adjacent > 1)
            return false;

        return true;
    };

    auto placeMineNearTorch = [&](int torchRow, int torchCol, bool relaxed) -> bool
    {
        int bestRow = -1;
        int bestCol = -1;
        int bestScore = std::numeric_limits<int>::max();

        for (int dr = -2; dr <= 2; dr++)
        {
            for (int dc = -2; dc <= 2; dc++)
            {
                int distance = std::abs(dr) + std::abs(dc);
                if (distance == 0 || distance > 2)
                    continue;

                int rr = torchRow + dr;
                int cc = torchCol + dc;

                if (!isValidMineCell(rr, cc, relaxed))
                    continue;

                int score = 0;
                score += rowMineCount(rr) * 16;
                score += colMineCount(cc) * 14;
                score += distance * 8;
                score += std::rand() % 9;

                if (score < bestScore)
                {
                    bestScore = score;
                    bestRow = rr;
                    bestCol = cc;
                }
            }
        }

        if (bestRow < 0 || bestCol < 0)
            return false;

        map[bestRow][bestCol] = MINE;
        return true;
    };

    int placed = 0;
    for (size_t i = 0; i < torchCells.size(); i++)
    {
        int tr = torchCells[i].first;
        int tc = torchCells[i].second;
        if (torchHasNearbyMine(tr, tc))
            continue;

        if (placeMineNearTorch(tr, tc, false) || placeMineNearTorch(tr, tc, true))
            placed++;
    }

    std::vector<std::pair<int, int>> closeCandidates;
    std::vector<std::pair<int, int>> nearCandidates;
    closeCandidates.reserve(ROWS * COLS / 2);
    nearCandidates.reserve(ROWS * COLS / 2);

    bool queued[ROWS][COLS] = {};

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;

            for (int dr = -2; dr <= 2; dr++)
            {
                for (int dc = -2; dc <= 2; dc++)
                {
                    if (dr == 0 && dc == 0)
                        continue;

                    int distance = std::abs(dr) + std::abs(dc);
                    if (distance == 0 || distance > 2)
                        continue;

                    int nr = r + dr;
                    int nc = c + dc;

                    if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                        continue;
                    if ((nr == startRow && nc == startCol) || (nr == goalRow && nc == goalCol))
                        continue;
                    if (map[nr][nc] != FLOOR)
                        continue;
                    if (safePath[nr][nc])
                        continue;
                    if (queued[nr][nc])
                        continue;

                    queued[nr][nc] = true;
                    if (distance == 1)
                        closeCandidates.push_back({nr, nc});
                    else
                        nearCandidates.push_back({nr, nc});
                }
            }
        }
    }

    if (closeCandidates.empty() && nearCandidates.empty())
        return;

    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)) + 101U);
    std::shuffle(closeCandidates.begin(), closeCandidates.end(), rng);
    std::shuffle(nearCandidates.begin(), nearCandidates.end(), rng);

    int targetCount = count;
    if (placed > targetCount)
        targetCount = placed;

    int closeTarget = count / 2;
    int rowSoftLimit = COLS / 2;
    int colSoftLimit = ROWS / 2;

    for (size_t i = 0; i < closeCandidates.size() && placed < closeTarget; i++)
    {
        int r = closeCandidates[i].first;
        int c = closeCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
            if (safePath[r][c])
                continue;

            if (rowMineCount(r) > rowSoftLimit && (std::rand() % 100) < 85)
                continue;
            if (colMineCount(c) > colSoftLimit && (std::rand() % 100) < 85)
                continue;

            int nearbyMines = countAdjacentMines(r, c);
            if (nearbyMines > 0)
                continue;

            if (createsDenseMineCluster(r, c))
                continue;

            map[r][c] = MINE;
            placed++;
        }
    }

    for (size_t i = 0; i < nearCandidates.size() && placed < targetCount; i++)
    {
        int r = nearCandidates[i].first;
        int c = nearCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
            if (safePath[r][c])
                continue;

            if (rowMineCount(r) > rowSoftLimit && (std::rand() % 100) < 75)
                continue;
            if (colMineCount(c) > colSoftLimit && (std::rand() % 100) < 75)
                continue;

            int nearbyMines = countAdjacentMines(r, c);
            if (nearbyMines > 0)
                continue;

            if (createsDenseMineCluster(r, c))
                continue;

            map[r][c] = MINE;
            placed++;
        }
    }

    for (size_t i = 0; i < closeCandidates.size() && placed < targetCount; i++)
    {
        int r = closeCandidates[i].first;
        int c = closeCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
            if (safePath[r][c])
                continue;

            if (rowMineCount(r) > rowSoftLimit + 1 && (std::rand() % 100) < 70)
                continue;
            if (colMineCount(c) > colSoftLimit + 1 && (std::rand() % 100) < 70)
                continue;

            if (countAdjacentMines(r, c) > 0)
                continue;

            if (createsDenseMineCluster(r, c))
                continue;

            map[r][c] = MINE;
            placed++;
        }
    }
}

bool MapManager::placeOneRandomTile(int tileType, int safeRow, int safeCol, int safeRadius, int visibilityRule, bool revealPlacedTile)
{
    for (int attempt = 0; attempt < 500; attempt++)
    {
        int r = std::rand() % ROWS;
        int c = std::rand() % COLS;

        if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
            continue;

        if (map[r][c] != FLOOR)
            continue;

        int dr = r - safeRow;
        int dc = c - safeCol;
        if (dr * dr + dc * dc <= safeRadius * safeRadius)
            continue;

        if (visibilityRule == HIDDEN_ONLY && visible[r][c])
            continue;

        if (visibilityRule == VISIBLE_ONLY && !visible[r][c])
            continue;

        if (tileType == MINE && safePath[r][c])
            continue;

        if (tileType == MINE)
        {
            int nearbyMines = countAdjacentMines(r, c);
            if (nearbyMines >= 2)
                continue;

            if (nearbyMines == 1 && (std::rand() % 100) < 60)
                continue;

            if (createsDenseMineCluster(r, c))
                continue;
        }

        map[r][c] = tileType;
        if (tileType == TORCH)
            torchState[r][c] = TORCH_NOT_USED;

        if (revealPlacedTile)
            revealCell(r, c);

        return true;
    }

    return false;
}

void MapManager::carveSafePath(int fromRow, int fromCol, int toRow, int toCol)
{
    int r = fromRow;
    int c = fromCol;
    bool visited[ROWS][COLS] = {};

    if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
    {
        visited[r][c] = true;
        safePath[r][c] = true;
    }

    while (r != toRow || c != toCol)
    {
        int nextRowCandidates[2] = {r, r};
        int nextColCandidates[2] = {c, c};
        int candidateCount = 0;

        if (r != toRow)
        {
            nextRowCandidates[candidateCount] = r + ((toRow > r) ? 1 : -1);
            nextColCandidates[candidateCount] = c;
            candidateCount++;
        }

        if (c != toCol)
        {
            nextRowCandidates[candidateCount] = r;
            nextColCandidates[candidateCount] = c + ((toCol > c) ? 1 : -1);
            candidateCount++;
        }

        if (candidateCount == 0)
            break;

        int bestIndex = 0;
        int bestScore = std::numeric_limits<int>::max();

        for (int i = 0; i < candidateCount; i++)
        {
            int nr = nextRowCandidates[i];
            int nc = nextColCandidates[i];

            if (nr < 0) nr = 0;
            if (nr >= ROWS) nr = ROWS - 1;
            if (nc < 0) nc = 0;
            if (nc >= COLS) nc = COLS - 1;

            int score = 0;
            if (map[nr][nc] == MINE)
                score += 70;

            int adjacentMineCount = 0;
            for (int dr = -1; dr <= 1; dr++)
            {
                for (int dc = -1; dc <= 1; dc++)
                {
                    if (dr == 0 && dc == 0)
                        continue;
                    int ar = nr + dr;
                    int ac = nc + dc;
                    if (ar < 0 || ar >= ROWS || ac < 0 || ac >= COLS)
                        continue;
                    if (map[ar][ac] == MINE)
                        adjacentMineCount++;
                }
            }

            score += adjacentMineCount * 8;
            score += (std::abs(toRow - nr) + std::abs(toCol - nc)) * 5;

            if (visited[nr][nc])
                score += 18;

            if (score < bestScore)
            {
                bestScore = score;
                bestIndex = i;
            }
        }

        r = nextRowCandidates[bestIndex];
        c = nextColCandidates[bestIndex];

        if (r < 0) r = 0;
        if (r >= ROWS) r = ROWS - 1;
        if (c < 0) c = 0;
        if (c >= COLS) c = COLS - 1;

        visited[r][c] = true;
        safePath[r][c] = true;

        if (!(r == goalRow && c == goalCol) && !(r == startRow && c == startCol))
        {
            revealCell(r, c);
        }
    }
}

bool MapManager::placeReachableVisibleTorch(int playerRow, int playerCol)
{
    const int minTorchDistance = 2;

    std::vector<std::pair<int, int>> candidates;
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (!visible[r][c])
                continue;
            if (map[r][c] != FLOOR)
                continue;
            if (r == startRow && c == startCol)
                continue;
            if (r == goalRow && c == goalCol)
                continue;

            int distance = std::abs(r - playerRow) + std::abs(c - playerCol);
            if (distance < 2)
                continue;

            candidates.push_back({r, c});
        }
    }

    if (candidates.empty())
        return false;

    int pick = std::rand() % static_cast<int>(candidates.size());
    int targetRow = candidates[pick].first;
    int targetCol = candidates[pick].second;

    if (hasNearbyTorch(targetRow, targetCol, minTorchDistance))
        return false;

    carveSafePath(playerRow, playerCol, targetRow, targetCol);
    map[targetRow][targetCol] = TORCH;
    torchState[targetRow][targetCol] = TORCH_NOT_USED;
    revealCell(targetRow, targetCol);
    return true;
}

void MapManager::reset(Difficulty difficulty)
{
    currentDifficulty = difficulty;
    initializeMap();

    safePath[startRow][startCol] = true;
    safePath[goalRow][goalCol] = true;

    int mineCount = 31;
    int nearTorchMineCount = 19;

    if (difficulty == Difficulty::EASY)
    {
        mineCount = 24;
        nearTorchMineCount = 12;
    }
    else if (difficulty == Difficulty::HARD)
    {
        mineCount = 45;
        nearTorchMineCount = 29;
    }

    buildFixedTorchNetwork(difficulty);
    carveSafePath(startRow, startCol, goalRow, goalCol);
    placeRandomTiles(MINE, mineCount);
    placeMinesNearTorches(nearTorchMineCount);

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            visible[r][c] = false;
            shadowVisible[r][c] = false;
        }
    }

    revealInitialArea();
}

void MapManager::buildFixedTorchNetwork(Difficulty difficulty)
{
    int torchTarget = 14;
    if (difficulty == Difficulty::EASY)
        torchTarget = 12;
    else if (difficulty == Difficulty::HARD)
        torchTarget = 17;

    int minTorchDistance = 2;
    if (difficulty != Difficulty::EASY)
        minTorchDistance = 3;

    const int zoneRows = 3;
    const int zoneCols = 4;
    int zoneCount[zoneRows * zoneCols] = {0};

    auto getZoneIndex = [zoneRows, zoneCols, this](int row, int col) -> int
    {
        int zr = (row * zoneRows) / ROWS;
        int zc = (col * zoneCols) / COLS;
        if (zr < 0) zr = 0;
        if (zr >= zoneRows) zr = zoneRows - 1;
        if (zc < 0) zc = 0;
        if (zc >= zoneCols) zc = zoneCols - 1;
        return zr * zoneCols + zc;
    };

    std::vector<std::pair<int, int>> candidates;
    candidates.reserve((ROWS - 2) * (COLS - 2));
    for (int r = 1; r < ROWS - 1; r++)
    {
        for (int c = 1; c < COLS - 1; c++)
        {
            if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
                continue;
            candidates.push_back({r, c});
        }
    }

    static std::mt19937 torchRng(static_cast<unsigned int>(std::time(nullptr)) + 313U);
    std::shuffle(candidates.begin(), candidates.end(), torchRng);

    std::vector<std::pair<int, int>> torchNodes;
    torchNodes.reserve(static_cast<size_t>(torchTarget + 8));

    auto placeTorchAndLink = [this, &torchNodes](int row, int col)
    {
        if (!(map[row][col] == FLOOR || map[row][col] == MINE))
            return;

        if (torchNodes.empty())
        {
            carveSafePath(startRow, startCol, row, col);
        }
        else
        {
            int bestIndex = 0;
            int bestDistance = std::numeric_limits<int>::max();
            for (size_t i = 0; i < torchNodes.size(); i++)
            {
                int distance = std::abs(torchNodes[i].first - row) + std::abs(torchNodes[i].second - col);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = static_cast<int>(i);
                }
            }

            carveSafePath(torchNodes[static_cast<size_t>(bestIndex)].first, torchNodes[static_cast<size_t>(bestIndex)].second, row, col);
        }

        map[row][col] = TORCH;
        torchState[row][col] = TORCH_NOT_USED;
        torchNodes.push_back({row, col});
    };

    int placedTorches = 0;

    for (size_t i = 0; i < candidates.size() && placedTorches < torchTarget; i++)
    {
        int r = candidates[i].first;
        int c = candidates[i].second;
        int zoneIndex = getZoneIndex(r, c);

        if (zoneCount[zoneIndex] > 0)
            continue;
        if (hasNearbyTorch(r, c, minTorchDistance))
            continue;

        placeTorchAndLink(r, c);
        zoneCount[zoneIndex]++;
        placedTorches++;
    }

    for (size_t i = 0; i < candidates.size() && placedTorches < torchTarget; i++)
    {
        int r = candidates[i].first;
        int c = candidates[i].second;
        int zoneIndex = getZoneIndex(r, c);

        if (zoneCount[zoneIndex] >= 1)
            continue;
        if (hasNearbyTorch(r, c, minTorchDistance))
            continue;

        placeTorchAndLink(r, c);
        zoneCount[zoneIndex]++;
        placedTorches++;
    }

    for (size_t i = 0; i < candidates.size() && placedTorches < torchTarget; i++)
    {
        int r = candidates[i].first;
        int c = candidates[i].second;
        if (hasNearbyTorch(r, c, minTorchDistance))
            continue;

        placeTorchAndLink(r, c);
        placedTorches++;
    }

    if (torchNodes.size() >= 2)
    {
        for (size_t i = 0; i < torchNodes.size(); i++)
        {
            int rowA = torchNodes[i].first;
            int colA = torchNodes[i].second;
            int goalDistA = std::abs(goalRow - rowA) + std::abs(goalCol - colA);

            int forwardIndex = -1;
            int forwardScore = std::numeric_limits<int>::max();
            int lateralIndex = -1;
            int lateralScore = std::numeric_limits<int>::max();

            for (size_t j = 0; j < torchNodes.size(); j++)
            {
                if (i == j)
                    continue;

                int rowB = torchNodes[j].first;
                int colB = torchNodes[j].second;
                int distance = std::abs(rowA - rowB) + std::abs(colA - colB);
                int goalDistB = std::abs(goalRow - rowB) + std::abs(goalCol - colB);
                int goalDiff = std::abs(goalDistA - goalDistB);

                if (goalDistB < goalDistA && distance >= 3 && distance <= 9)
                {
                    int score = distance * 4 + goalDistB;
                    if (score < forwardScore)
                    {
                        forwardScore = score;
                        forwardIndex = static_cast<int>(j);
                    }
                }

                if (goalDiff <= 2 && distance >= 4 && distance <= 10)
                {
                    int score = distance * 3 + goalDiff * 5;
                    if (score < lateralScore)
                    {
                        lateralScore = score;
                        lateralIndex = static_cast<int>(j);
                    }
                }
            }

            if (forwardIndex != -1)
            {
                int rowB = torchNodes[static_cast<size_t>(forwardIndex)].first;
                int colB = torchNodes[static_cast<size_t>(forwardIndex)].second;
                carveSafePath(rowA, colA, rowB, colB);
            }

            if (lateralIndex != -1)
            {
                int rowB = torchNodes[static_cast<size_t>(lateralIndex)].first;
                int colB = torchNodes[static_cast<size_t>(lateralIndex)].second;
                carveSafePath(rowA, colA, rowB, colB);
            }
        }
    }

    ensureStarterTorch();
}

bool MapManager::load(SDL_Renderer *renderer, const char *backgroundPath, Difficulty difficulty)
{
    // Destroy previous textures if any
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
    if (mineTexture)
    {
        SDL_DestroyTexture(mineTexture);
        mineTexture = nullptr;
    }
    if (torchTexture)
    {
        SDL_DestroyTexture(torchTexture);
        torchTexture = nullptr;
    }
    if (goalTexture)
    {
        SDL_DestroyTexture(goalTexture);
        goalTexture = nullptr;
    }

    backgroundTexture = IMG_LoadTexture(renderer, backgroundPath);
    // load entity icons (ignore failure, drawing will fallback to color)
    mineTexture = IMG_LoadTexture(renderer, "assets/images/entities/bom.png");
    torchTexture = IMG_LoadTexture(renderer, "assets/images/entities/fire.png");
    // there is no dedicated goal image; reuse fire for now or leave null
    goalTexture = nullptr;

    reset(difficulty);
    return backgroundTexture != nullptr;
}

void MapManager::revealCell(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return;

    visible[row][col] = true;
    shadowVisible[row][col] = true;
}

void MapManager::revealShadowCell(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return;

    shadowVisible[row][col] = true;
}

int MapManager::sign(int value) const
{
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

bool MapManager::isForwardCell(int baseRow, int baseCol, int row, int col) const
{
    int goalVecR = goalRow - baseRow;
    int goalVecC = goalCol - baseCol;
    int testVecR = row - baseRow;
    int testVecC = col - baseCol;
    int dot = goalVecR * testVecR + goalVecC * testVecC;
    return dot > 0;
}

bool MapManager::hasHiddenNeighbor(int row, int col) const
{
    for (int dr = -1; dr <= 1; dr++)
    {
        for (int dc = -1; dc <= 1; dc++)
        {
            if (dr == 0 && dc == 0)
                continue;

            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                continue;

            if (!visible[nr][nc])
                return true;
        }
    }

    return false;
}

bool MapManager::hasAdjacentMine(int row, int col) const
{
    return countAdjacentMines(row, col) > 0;
}

int MapManager::countAdjacentMines(int row, int col) const
{
    int count = 0;
    for (int dr = -1; dr <= 1; dr++)
    {
        for (int dc = -1; dc <= 1; dc++)
        {
            if (dr == 0 && dc == 0)
                continue;

            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                continue;

            if (map[nr][nc] == MINE)
                count++;
        }
    }

    return count;
}

bool MapManager::createsDenseMineCluster(int row, int col) const
{
    for (int baseRow = row - 1; baseRow <= row; baseRow++)
    {
        for (int baseCol = col - 1; baseCol <= col; baseCol++)
        {
            if (baseRow < 0 || baseCol < 0 || baseRow + 1 >= ROWS || baseCol + 1 >= COLS)
                continue;

            int mineCount = 0;
            for (int dr = 0; dr <= 1; dr++)
            {
                for (int dc = 0; dc <= 1; dc++)
                {
                    int rr = baseRow + dr;
                    int cc = baseCol + dc;
                    bool isNewMine = (rr == row && cc == col);
                    if (isNewMine || map[rr][cc] == MINE)
                        mineCount++;
                }
            }

            if (mineCount >= 3)
                return true;
        }
    }

    return false;
}

bool MapManager::hasNearbyTorch(int row, int col, int minDistance) const
{
    if (minDistance <= 0)
        return false;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;

            int distance = std::abs(r - row) + std::abs(c - col);
            if (distance < minDistance)
                return true;
        }
    }

    return false;
}

void MapManager::render(SDL_Renderer* renderer)
{
    if (backgroundTexture)
    {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }

    int offsetX = RENDER_OFFSET_X;
    int offsetY = RENDER_OFFSET_Y;
    int iconPadding = TILE_SIZE / 10;
    if (iconPadding < 3)
        iconPadding = 3;
    Uint32 ticksNow = SDL_GetTicks();

    SDL_Rect tileRect;
    tileRect.w = TILE_SIZE;
    tileRect.h = TILE_SIZE;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            tileRect.x = offsetX + c * TILE_SIZE;
            tileRect.y = offsetY + r * TILE_SIZE;
            bool alwaysRevealGoalTile = map[r][c] == GOAL;
            bool testerRevealTorchTile = kTesterEnabledBuild && testerRevealTorches && map[r][c] == TORCH;
            bool testerRevealMineTile = kTesterEnabledBuild && testerRevealMines && map[r][c] == MINE;
            bool testerRevealTile = testerRevealTorchTile || testerRevealMineTile;
            bool revealDetailTile = visible[r][c] || testerRevealTile || alwaysRevealGoalTile;

            if (map[r][c] == WALL)
            {
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                SDL_RenderFillRect(renderer, &tileRect);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
                SDL_RenderFillRect(renderer, &tileRect);

                if (revealDetailTile)
                {
                    if (map[r][c] == TORCH)
                    {
                        SDL_Rect iconRect = tileRect;
                        int torchInsetX = TILE_SIZE / 10;
                        int torchInsetY = TILE_SIZE / 12;
                        if (torchInsetX < 3)
                            torchInsetX = 3;
                        if (torchInsetY < 2)
                            torchInsetY = 2;
                        iconRect.x += torchInsetX;
                        iconRect.y += torchInsetY;
                        iconRect.w -= torchInsetX * 2;
                        iconRect.h -= torchInsetY * 2;

                        if (iconRect.w < 8)
                            iconRect.w = 8;
                        if (iconRect.h < 8)
                            iconRect.h = 8;

                        SDL_Color glowColor = {255, 160, 30, 90};
                        if (torchState[r][c] == TORCH_SUCCESS)
                            glowColor = SDL_Color{255, 210, 85, 120};
                        else if (torchState[r][c] == TORCH_FAILED)
                            glowColor = SDL_Color{130, 45, 45, 85};

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 60, 35, 16, 35);
                        SDL_RenderFillRect(renderer, &iconRect);

                        SDL_Rect glowRect = tileRect;
                        int glowInset = TILE_SIZE / 14 + static_cast<int>((ticksNow / 160) % 2);
                        if (glowInset < 2)
                            glowInset = 2;
                        glowRect.x += glowInset;
                        glowRect.y += glowInset;
                        glowRect.w -= glowInset * 2;
                        glowRect.h -= glowInset * 2;

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, glowColor.r, glowColor.g, glowColor.b, glowColor.a);
                        SDL_RenderFillRect(renderer, &glowRect);

                        if (torchTexture)
                        {
                            int texW = 0;
                            int texH = 0;
                            SDL_QueryTexture(torchTexture, nullptr, nullptr, &texW, &texH);
                            SDL_Rect srcRect = {0, 0, texW, texH};
                            if (texW > 0 && texH > 0)
                            {
                                int cutLeft = (texW * 12) / 100;
                                int cutTop = (texH * 6) / 100;
                                int cutRight = (texW * 12) / 100;
                                int cutBottom = (texH * 34) / 100;
                                srcRect.x = cutLeft;
                                srcRect.y = cutTop;
                                srcRect.w = texW - cutLeft - cutRight;
                                srcRect.h = texH - cutTop - cutBottom;

                                if (srcRect.w < 4 || srcRect.h < 4)
                                {
                                    srcRect.x = 0;
                                    srcRect.y = 0;
                                    srcRect.w = texW;
                                    srcRect.h = texH;
                                }
                            }

                            // tint icon based on state
                            if (torchState[r][c] == TORCH_SUCCESS)
                                SDL_SetTextureColorMod(torchTexture, 255, 225, 120);
                            else if (torchState[r][c] == TORCH_FAILED)
                                SDL_SetTextureColorMod(torchTexture, 140, 55, 55);
                            else
                                SDL_SetTextureColorMod(torchTexture, 255, 240, 190);

                            SDL_RenderCopy(renderer, torchTexture, &srcRect, &iconRect);

                            // reset modulation
                            SDL_SetTextureColorMod(torchTexture, 255, 255, 255);
                        }
                        else
                        {
                            if (torchState[r][c] == TORCH_SUCCESS)
                                SDL_SetRenderDrawColor(renderer, 255, 200, 60, 255);
                            else if (torchState[r][c] == TORCH_FAILED)
                                SDL_SetRenderDrawColor(renderer, 110, 35, 35, 255);
                            else
                                SDL_SetRenderDrawColor(renderer, 255, 160, 30, 255);
                            SDL_RenderFillRect(renderer, &iconRect);
                        }
                    }
                    else if (map[r][c] == MINE)
                    {
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 120, 35, 35, 45);
                        SDL_RenderFillRect(renderer, &tileRect);

                        SDL_Rect iconRect = tileRect;
                        iconRect.x += iconPadding;
                        iconRect.y += iconPadding;
                        iconRect.w -= iconPadding * 2;
                        iconRect.h -= iconPadding * 2;

                        int mineBoost = TILE_SIZE / 10;
                        if (mineBoost < 2)
                            mineBoost = 2;
                        iconRect.x -= mineBoost;
                        iconRect.y -= mineBoost;
                        iconRect.w += mineBoost * 2;
                        iconRect.h += mineBoost * 2;

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 28, 28, 28, 110);
                        SDL_RenderFillRect(renderer, &iconRect);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                        SDL_SetRenderDrawColor(renderer, 150, 70, 70, 255);
                        SDL_RenderDrawRect(renderer, &iconRect);

                        SDL_Rect mineShadow = iconRect;
                        mineShadow.x += TILE_SIZE / 18;
                        mineShadow.y += TILE_SIZE / 18;
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
                        SDL_RenderFillRect(renderer, &mineShadow);

                        if (mineTexture)
                        {
                            SDL_RenderCopy(renderer, mineTexture, nullptr, &iconRect);
                        }
                        else
                        {
                            SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
                            SDL_RenderFillRect(renderer, &iconRect);
                        }

                        SDL_Rect mineShine = iconRect;
                        mineShine.w = iconRect.w / 3;
                        mineShine.h = iconRect.h / 3;
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 55);
                        SDL_RenderFillRect(renderer, &mineShine);

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                    }
                    else if (map[r][c] == GOAL)
                    {
                        if (goalTexture)
                        {
                            SDL_RenderCopy(renderer, goalTexture, nullptr, &tileRect);
                        }
                        else
                        {
                            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                            SDL_RenderFillRect(renderer, &tileRect);
                        }
                    }
                }
            }

            if (testerRevealTile || alwaysRevealGoalTile)
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
            else if (!shadowVisible[r][c])
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
                SDL_RenderFillRect(renderer, &tileRect);
            }
            else if (visible[r][c])
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
                SDL_RenderFillRect(renderer, &tileRect);
            }
            else
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
                SDL_RenderFillRect(renderer, &tileRect);
            }

            bool hintActive = (hintTorchRow == r && hintTorchCol == c && SDL_GetTicks() < hintUntilTick);
            if (hintActive)
            {
                Uint32 phase = (SDL_GetTicks() / 180) % 2;
                if (phase == 0)
                {
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 255, 210, 70, 170);
                    SDL_RenderFillRect(renderer, &tileRect);
                }
            }

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 38, 38, 38, 255);
            SDL_RenderDrawRect(renderer, &tileRect);
        }
    }
}

int MapManager::getTile(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return WALL;

    return map[row][col];
}

bool MapManager::isWall(int row, int col) const
{
    if (getTile(row, col) == WALL)
        return true;
    return false;
}

bool MapManager::isMine(int row, int col) const
{
    return getTile(row, col) == MINE;
}

bool MapManager::isTorch(int row, int col) const
{
    return getTile(row, col) == TORCH;
}

bool MapManager::isGoal(int row, int col) const
{
    return getTile(row, col) == GOAL;
}

bool MapManager::canAttemptTorch(int row, int col) const
{
    if (!isTorch(row, col))
        return false;
    return torchState[row][col] == TORCH_NOT_USED;
}

void MapManager::resolveTorch(int row, int col, bool correctAnswer)
{
    if (!isTorch(row, col))
        return;

    torchState[row][col] = correctAnswer ? TORCH_SUCCESS : TORCH_FAILED;
    revealCell(row, col);
}

bool MapManager::activateStarterTorch()
{
    int targetRow = -1;
    int targetCol = -1;
    int bestDistance = std::numeric_limits<int>::max();

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;
            if (torchState[r][c] != TORCH_NOT_USED)
                continue;

            int distance = std::abs(r - startRow) + std::abs(c - startCol);
            if (distance == 0)
                continue;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                targetRow = r;
                targetCol = c;
            }
        }
    }

    if (targetRow == -1 || targetCol == -1)
    {
        int fallbackRow = startRow;
        int fallbackCol = startCol + 1;
        if (fallbackCol >= COLS)
            fallbackCol = startCol;

        if (fallbackRow == goalRow && fallbackCol == goalCol)
            return false;

        if (map[fallbackRow][fallbackCol] == MINE)
            map[fallbackRow][fallbackCol] = FLOOR;

        if (map[fallbackRow][fallbackCol] != FLOOR)
            return false;

        map[fallbackRow][fallbackCol] = TORCH;
        torchState[fallbackRow][fallbackCol] = TORCH_NOT_USED;
        targetRow = fallbackRow;
        targetCol = fallbackCol;
    }

    revealRadius(startRow, startCol, 1, true);
    revealCell(targetRow, targetCol);
    return true;
}

void MapManager::revealAroundTorch(int row, int col)
{
    revealCell(row, col);

    int r = row;
    int c = col;
    int steps = 2;
    if (currentDifficulty == Difficulty::HARD)
        steps = 1;

    for (int i = 0; i < steps; i++)
    {
        int dr = sign(goalRow - r);
        int dc = sign(goalCol - c);

        if (dr == 0 && dc == 0)
            break;

        bool moveRow = false;
        if (dr != 0 && dc != 0)
            moveRow = (std::rand() % 100) < 50;
        else
            moveRow = (dr != 0);

        if (moveRow)
            r += dr;
        else
            c += dc;

        if (r < 0) r = 0;
        if (r >= ROWS) r = ROWS - 1;
        if (c < 0) c = 0;
        if (c >= COLS) c = COLS - 1;

        revealCell(r, c);

        if ((std::rand() % 100) < 35)
        {
            int sideR = r;
            int sideC = c;
            if (moveRow)
                sideC += ((std::rand() % 2) == 0) ? 1 : -1;
            else
                sideR += ((std::rand() % 2) == 0) ? 1 : -1;

            if (sideR >= 0 && sideR < ROWS && sideC >= 0 && sideC < COLS)
            {
                revealCell(sideR, sideC);
            }
        }
    }
}

void MapManager::revealRadius(int row, int col, int radius, bool includeCenter)
{
    for (int dr = -radius; dr <= radius; dr++)
    {
        for (int dc = -radius; dc <= radius; dc++)
        {
            if (!includeCenter && dr == 0 && dc == 0)
                continue;

            revealCell(row + dr, col + dc);
        }
    }
}

void MapManager::revealAt(int row, int col)
{
    revealCell(row, col);
}

void MapManager::revealForwardStep(int row, int col, int dRow, int dCol)
{
    int targetRow = row + dRow;
    int targetCol = col + dCol;

    if (targetRow < 0 || targetRow >= ROWS || targetCol < 0 || targetCol >= COLS)
    {
        targetRow = row;
        targetCol = col;
    }

    revealCell(targetRow, targetCol);
}

void MapManager::revealShadowAround(int row, int col, int radius)
{
    revealShadowCell(row, col);

    int seeds = 3 + (std::rand() % 3);
    for (int i = 0; i < seeds; i++)
    {
        int r = row;
        int c = col;
        int stepCount = 1 + (std::rand() % (radius + 1));

        for (int step = 0; step < stepCount; step++)
        {
            int dir = std::rand() % 8;
            if (dir == 0) r--;
            else if (dir == 1) r++;
            else if (dir == 2) c--;
            else if (dir == 3) c++;
            else if (dir == 4) { r--; c--; }
            else if (dir == 5) { r--; c++; }
            else if (dir == 6) { r++; c--; }
            else { r++; c++; }

            if (r < 0) r = 0;
            if (r >= ROWS) r = ROWS - 1;
            if (c < 0) c = 0;
            if (c >= COLS) c = COLS - 1;

            revealShadowCell(r, c);
        }
    }
}

bool MapManager::spawnTorchInOpenedArea(int centerRow, int centerCol, int maxDistance)
{
    const int minTorchDistance = 3;

    if (maxDistance < 1)
        maxDistance = 1;

    int bestScore = std::numeric_limits<int>::max();
    std::vector<std::pair<int, int>> bestFloor;
    std::vector<std::pair<int, int>> bestMine;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (!visible[r][c])
                continue;

            int distance = std::abs(r - centerRow) + std::abs(c - centerCol);
            if (distance == 0 || distance > maxDistance)
                continue;

            if (r == startRow && c == startCol)
                continue;
            if (r == goalRow && c == goalCol)
                continue;
            if (hasNearbyTorch(r, c, minTorchDistance))
                continue;
            if (!hasHiddenNeighbor(r, c))
                continue;

            int goalDistance = std::abs(goalRow - r) + std::abs(goalCol - c);
            int ringBias = std::abs(distance - (maxDistance - 1));
            int score = goalDistance * 10 + ringBias * 3;

            if (map[r][c] == FLOOR)
            {
                if (score < bestScore)
                {
                    bestScore = score;
                    bestFloor.clear();
                    bestFloor.push_back({r, c});
                }
                else if (score == bestScore)
                {
                    bestFloor.push_back({r, c});
                }
            }
            else if (map[r][c] == MINE)
            {
                if (score < bestScore)
                {
                    bestScore = score;
                    bestMine.clear();
                    bestMine.push_back({r, c});
                }
                else if (score == bestScore)
                {
                    bestMine.push_back({r, c});
                }
            }
        }
    }

    if (!bestFloor.empty())
    {
        int pick = std::rand() % static_cast<int>(bestFloor.size());
        int r = bestFloor[pick].first;
        int c = bestFloor[pick].second;
        map[r][c] = TORCH;
        torchState[r][c] = TORCH_NOT_USED;
        revealCell(r, c);
        return true;
    }

    if (!bestMine.empty())
    {
        int pick = std::rand() % static_cast<int>(bestMine.size());
        int r = bestMine[pick].first;
        int c = bestMine[pick].second;
        map[r][c] = TORCH;
        torchState[r][c] = TORCH_NOT_USED;
        revealCell(r, c);
        return true;
    }

    for (int radius = 1; radius <= 3; radius++)
    {
        revealRadius(centerRow, centerCol, radius, false);

        for (int dr = -radius; dr <= radius; dr++)
        {
            for (int dc = -radius; dc <= radius; dc++)
            {
                if (dr == 0 && dc == 0)
                    continue;

                int r = centerRow + dr;
                int c = centerCol + dc;
                if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
                    continue;
                if (r == startRow && c == startCol)
                    continue;
                if (r == goalRow && c == goalCol)
                    continue;
                if (hasNearbyTorch(r, c, minTorchDistance))
                    continue;

                if (map[r][c] == FLOOR || map[r][c] == MINE)
                {
                    map[r][c] = TORCH;
                    torchState[r][c] = TORCH_NOT_USED;
                    revealCell(r, c);
                    return true;
                }
            }
        }
    }

    for (int attempt = 0; attempt < 20; attempt++)
    {
        int rr = centerRow + ((std::rand() % (maxDistance * 2 + 1)) - maxDistance);
        int cc = centerCol + ((std::rand() % (maxDistance * 2 + 1)) - maxDistance);

        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS)
            continue;
        if ((rr == startRow && cc == startCol) || (rr == goalRow && cc == goalCol))
            continue;

        if (map[rr][cc] == TORCH)
        {
            revealCell(rr, cc);
            return true;
        }

        if ((map[rr][cc] == FLOOR || map[rr][cc] == MINE) && !hasNearbyTorch(rr, cc, minTorchDistance))
        {
            map[rr][cc] = TORCH;
            torchState[rr][cc] = TORCH_NOT_USED;
            revealCell(rr, cc);
            return true;
        }
    }

    return false;
}

bool MapManager::findNextUnresolvedTorch(int fromRow, int fromCol, int& outRow, int& outCol) const
{
    int bestDistance = std::numeric_limits<int>::max();
    bool found = false;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;
            if (torchState[r][c] != TORCH_NOT_USED)
                continue;
            if (r == fromRow && c == fromCol)
                continue;
            if (!isForwardCell(fromRow, fromCol, r, c))
                continue;

            int distance = std::abs(r - fromRow) + std::abs(c - fromCol);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outRow = r;
                outCol = c;
                found = true;
            }
        }
    }

    if (found)
        return true;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;
            if (torchState[r][c] != TORCH_NOT_USED)
                continue;
            if (r == fromRow && c == fromCol)
                continue;

            int distance = std::abs(r - fromRow) + std::abs(c - fromCol);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outRow = r;
                outCol = c;
                found = true;
            }
        }
    }

    return found;
}

bool MapManager::findNearestUnresolvedTorch(int fromRow, int fromCol, int& outRow, int& outCol) const
{
    int bestDistance = std::numeric_limits<int>::max();
    bool found = false;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;
            if (torchState[r][c] != TORCH_NOT_USED)
                continue;
            if (r == fromRow && c == fromCol)
                continue;

            int distance = std::abs(r - fromRow) + std::abs(c - fromCol);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outRow = r;
                outCol = c;
                found = true;
            }
        }
    }

    return found;
}

void MapManager::hintNearestTorch(int fromRow, int fromCol)
{
    int targetRow = -1;
    int targetCol = -1;

    if (!findNearestUnresolvedTorch(fromRow, fromCol, targetRow, targetCol))
    {
        hintTorchRow = -1;
        hintTorchCol = -1;
        hintUntilTick = 0;
        return;
    }

    Uint32 hintDurationMs = 8000;
    if (currentDifficulty == Difficulty::MEDIUM)
        hintDurationMs = 5000;
    else if (currentDifficulty == Difficulty::HARD)
        hintDurationMs = 3000;

    hintTorchRow = targetRow;
    hintTorchCol = targetCol;
    hintUntilTick = SDL_GetTicks() + hintDurationMs;
}

void MapManager::clearTorchHint()
{
    hintTorchRow = -1;
    hintTorchCol = -1;
    hintUntilTick = 0;
}

bool MapManager::revealPathToNextTorch(int fromRow, int fromCol, int solvedTorchCount)
{
    std::vector<std::pair<int, int>> candidates;
    candidates.reserve(16);

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] != TORCH)
                continue;
            if (torchState[r][c] != TORCH_NOT_USED)
                continue;
            if (r == fromRow && c == fromCol)
                continue;

            candidates.push_back({r, c});
        }
    }

    int desiredBranches = 2;
    if (currentDifficulty == Difficulty::HARD)
        desiredBranches = 1;

    if (solvedTorchCount >= 6)
    {
        if (currentDifficulty == Difficulty::EASY)
            desiredBranches = 3;
        else if (currentDifficulty == Difficulty::MEDIUM)
            desiredBranches = 2;
    }

    std::vector<std::pair<int, int>> nearCandidates;
    nearCandidates.reserve(candidates.size());

    for (size_t i = 0; i < candidates.size(); i++)
    {
        int distance = std::abs(candidates[i].first - fromRow) + std::abs(candidates[i].second - fromCol);
        if (distance >= 2 && distance <= 4)
            nearCandidates.push_back(candidates[i]);
    }

    if (!nearCandidates.empty())
        candidates.swap(nearCandidates);

    if (static_cast<int>(candidates.size()) < desiredBranches)
    {
        for (int i = 0; i < 3 && static_cast<int>(candidates.size()) < desiredBranches; i++)
        {
            spawnTorchInOpenedArea(fromRow, fromCol, 2);

            candidates.clear();
            for (int r = 0; r < ROWS; r++)
            {
                for (int c = 0; c < COLS; c++)
                {
                    if (map[r][c] != TORCH)
                        continue;
                    if (torchState[r][c] != TORCH_NOT_USED)
                        continue;
                    if (r == fromRow && c == fromCol)
                        continue;

                    candidates.push_back({r, c});
                }
            }

            std::vector<std::pair<int, int>> refreshedNearCandidates;
            refreshedNearCandidates.reserve(candidates.size());
            for (size_t k = 0; k < candidates.size(); k++)
            {
                int distance = std::abs(candidates[k].first - fromRow) + std::abs(candidates[k].second - fromCol);
                if (distance >= 2 && distance <= 4)
                    refreshedNearCandidates.push_back(candidates[k]);
            }

            if (!refreshedNearCandidates.empty())
                candidates.swap(refreshedNearCandidates);
        }
    }

    if (candidates.empty())
    {
        int fallbackRevealSteps = (currentDifficulty == Difficulty::EASY) ? 2 : 1;
        revealTowardGoalPath(fromRow, fromCol, fallbackRevealSteps);
        return false;
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [fromRow, fromCol, this](const std::pair<int, int>& a, const std::pair<int, int>& b)
        {
            auto scoreCandidate = [this, fromRow, fromCol](const std::pair<int, int>& p) -> int
            {
                int row = p.first;
                int col = p.second;
                int score = 0;

                int fromDistance = std::abs(row - fromRow) + std::abs(col - fromCol);
                int goalDistance = std::abs(row - goalRow) + std::abs(col - goalCol);

                score += fromDistance * 6;
                score += goalDistance * 3;

                int nearbyMines = 0;
                for (int dr = -1; dr <= 1; dr++)
                {
                    for (int dc = -1; dc <= 1; dc++)
                    {
                        if (dr == 0 && dc == 0)
                            continue;
                        int nr = row + dr;
                        int nc = col + dc;
                        if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                            continue;
                        if (map[nr][nc] == MINE)
                            nearbyMines++;
                    }
                }

                score += nearbyMines * 12;
                return score;
            };

            int sa = scoreCandidate(a);
            int sb = scoreCandidate(b);
            if (sa != sb)
                return sa < sb;

            int ga = std::abs(a.first - goalRow) + std::abs(a.second - goalCol);
            int gb = std::abs(b.first - goalRow) + std::abs(b.second - goalCol);
            return ga < gb;
        }
    );

    int branchCount = std::min(desiredBranches, static_cast<int>(candidates.size()));

    auto directionBucket = [fromRow, fromCol](int row, int col) -> int
    {
        int dr = row - fromRow;
        int dc = col - fromCol;
        int sr = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
        int sc = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

        if (sr == -1 && sc == 0) return 0;
        if (sr == -1 && sc == 1) return 1;
        if (sr == 0 && sc == 1) return 2;
        if (sr == 1 && sc == 1) return 3;
        if (sr == 1 && sc == 0) return 4;
        if (sr == 1 && sc == -1) return 5;
        if (sr == 0 && sc == -1) return 6;
        if (sr == -1 && sc == -1) return 7;
        return 8;
    };

    std::vector<int> selectedIndices;
    selectedIndices.reserve(static_cast<size_t>(branchCount));
    bool usedBucket[9] = {false, false, false, false, false, false, false, false, false};

    for (size_t i = 0; i < candidates.size() && static_cast<int>(selectedIndices.size()) < branchCount; i++)
    {
        int bucket = directionBucket(candidates[i].first, candidates[i].second);
        if (bucket < 0 || bucket > 8)
            bucket = 8;

        if (usedBucket[bucket])
            continue;

        usedBucket[bucket] = true;
        selectedIndices.push_back(static_cast<int>(i));
    }

    for (size_t i = 0; i < candidates.size() && static_cast<int>(selectedIndices.size()) < branchCount; i++)
    {
        bool alreadySelected = false;
        for (size_t k = 0; k < selectedIndices.size(); k++)
        {
            if (selectedIndices[k] == static_cast<int>(i))
            {
                alreadySelected = true;
                break;
            }
        }

        if (!alreadySelected)
            selectedIndices.push_back(static_cast<int>(i));
    }

    bool openedAny = false;
    std::vector<std::pair<int, int>> openedTargets;
    openedTargets.reserve(selectedIndices.size());
    for (size_t i = 0; i < selectedIndices.size(); i++)
    {
        int idx = selectedIndices[i];
        int targetRow = candidates[static_cast<size_t>(idx)].first;
        int targetCol = candidates[static_cast<size_t>(idx)].second;

        carveSafePath(fromRow, fromCol, targetRow, targetCol);
        revealCell(targetRow, targetCol);
        openedTargets.push_back({targetRow, targetCol});

        openedAny = true;
    }

    if (openedTargets.size() >= 2)
    {
        for (size_t i = 1; i < openedTargets.size(); i++)
        {
            int r1 = openedTargets[i - 1].first;
            int c1 = openedTargets[i - 1].second;
            int r2 = openedTargets[i].first;
            int c2 = openedTargets[i].second;
            carveSafePath(r1, c1, r2, c2);
        }
    }

    if (!openedTargets.empty())
    {
        int bestRow = openedTargets[0].first;
        int bestCol = openedTargets[0].second;
        int bestGoalDistance = std::abs(goalRow - bestRow) + std::abs(goalCol - bestCol);

        for (size_t i = 1; i < openedTargets.size(); i++)
        {
            int row = openedTargets[i].first;
            int col = openedTargets[i].second;
            int goalDistance = std::abs(goalRow - row) + std::abs(goalCol - col);
            if (goalDistance < bestGoalDistance)
            {
                bestGoalDistance = goalDistance;
                bestRow = row;
                bestCol = col;
            }
        }

        int forwardRevealSteps = (currentDifficulty == Difficulty::EASY) ? 2 : 1;
        revealTowardGoalPath(bestRow, bestCol, forwardRevealSteps);
    }

    return openedAny;
}

void MapManager::revealTowardGoalPath(int fromRow, int fromCol, int steps)
{
    int r = fromRow;
    int c = fromCol;

    for (int i = 0; i < steps; i++)
    {
        int dr = sign(goalRow - r);
        int dc = sign(goalCol - c);

        if ((std::rand() % 100) < 65)
        {
            if (dr != 0 && dc != 0)
            {
                if ((std::rand() % 2) == 0)
                    r += dr;
                else
                    c += dc;
            }
            else if (dr != 0)
                r += dr;
            else if (dc != 0)
                c += dc;
        }
        else
        {
            int sideAxis = (std::rand() % 2 == 0) ? 1 : -1;
            if ((std::rand() % 2) == 0)
                r += sideAxis;
            else
                c += sideAxis;
        }

        if (r < 0) r = 0;
        if (r >= ROWS) r = ROWS - 1;
        if (c < 0) c = 0;
        if (c >= COLS) c = COLS - 1;

        if (!isForwardCell(fromRow, fromCol, r, c))
            continue;

        revealCell(r, c);

        int sideRevealCount = (currentDifficulty == Difficulty::EASY) ? 1 : 0;
        for (int side = 0; side < sideRevealCount; side++)
        {
            int nr = r;
            int nc = c;

            if ((std::rand() % 2) == 0)
                nr += ((std::rand() % 2) == 0) ? 1 : -1;
            else
                nc += ((std::rand() % 2) == 0) ? 1 : -1;

            if (nr < 0) nr = 0;
            if (nr >= ROWS) nr = ROWS - 1;
            if (nc < 0) nc = 0;
            if (nc >= COLS) nc = COLS - 1;

            if (!isForwardCell(fromRow, fromCol, nr, nc))
                continue;

            revealCell(nr, nc);
        }
    }
}

void MapManager::revealRandomCluster(int centerRow, int centerCol, int seeds, int maxStep)
{
    for (int i = 0; i < seeds; i++)
    {
        int r = centerRow;
        int c = centerCol;
        int walks = 1 + (std::rand() % (maxStep + 1));

        for (int step = 0; step < walks; step++)
        {
            int dir = std::rand() % 8;
            if (dir == 0) r--;
            else if (dir == 1) r++;
            else if (dir == 2) c--;
            else if (dir == 3) c++;
            else if (dir == 4) { r--; c--; }
            else if (dir == 5) { r--; c++; }
            else if (dir == 6) { r++; c--; }
            else { r++; c++; }

            if (r < 0) r = 0;
            if (r >= ROWS) r = ROWS - 1;
            if (c < 0) c = 0;
            if (c >= COLS) c = COLS - 1;

            revealCell(r, c);
        }
    }
}

void MapManager::spawnNextWave(int playerRow, int playerCol, int mineToAdd, int torchToAdd)
{
    for (int i = 0; i < mineToAdd; i++)
    {
        bool placed = placeOneRandomTile(MINE, playerRow, playerCol, 2, HIDDEN_ONLY, false);
        if (!placed)
            placeOneRandomTile(MINE, playerRow, playerCol, 2, ANY_VISIBILITY, false);
    }

    (void)torchToAdd;
}

bool MapManager::hasAnyUnresolvedTorch() const
{
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] == TORCH && torchState[r][c] == TORCH_NOT_USED)
                return true;
        }
    }

    return false;
}

bool MapManager::isKnown(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return false;
    return shadowVisible[row][col];
}

bool MapManager::isDetailVisible(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return false;
    return visible[row][col];
}

void MapManager::setTesterOverlay(bool revealMines, bool revealTorches)
{
    if (!kTesterEnabledBuild)
    {
        testerRevealMines = false;
        testerRevealTorches = false;
        return;
    }

    testerRevealMines = revealMines;
    testerRevealTorches = revealTorches;
}

// currently unused – textures are managed directly in load()/clean()
void MapManager::cleanTextures(SDL_Renderer* renderer)
{
    (void)renderer;
    // preserved for future extension
}

void MapManager::clean()
{
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
    if (mineTexture)
    {
        SDL_DestroyTexture(mineTexture);
        mineTexture = nullptr;
    }
    if (torchTexture)
    {
        SDL_DestroyTexture(torchTexture);
        torchTexture = nullptr;
    }
    if (goalTexture)
    {
        SDL_DestroyTexture(goalTexture);
        goalTexture = nullptr;
    }
}
