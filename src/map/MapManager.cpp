#include "MapManager.h"
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <utility>
#include <limits>
#include <algorithm>
#include <random>

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
    revealRadius(startRow, startCol, 1, true);
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
            int startDistance = std::abs(r - startRow) + std::abs(c - startCol);
            int goalDistance = std::abs(r - goalRow) + std::abs(c - goalCol);

            if (startDistance <= 2 || goalDistance <= 1)
                continue;

            if (hasAdjacentMine(r, c))
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

    for (int r = 0; r < ROWS && placed < count; r++)
    {
        for (int c = 0; c < COLS && placed < count; c++)
        {
            if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
                continue;

            if (map[r][c] == FLOOR)
            {
                if (tileType == MINE && hasAdjacentMine(r, c))
                    continue;

                map[r][c] = tileType;
                placed++;
            }
        }
    }
}

void MapManager::placeMinesNearTorches(int count)
{
    if (count <= 0)
        return;

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

    int placed = 0;
    int closeTarget = (count * 2) / 3;

    for (size_t i = 0; i < closeCandidates.size() && placed < closeTarget; i++)
    {
        int r = closeCandidates[i].first;
        int c = closeCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
            if (hasAdjacentMine(r, c) && (std::rand() % 100) < 55)
                continue;

            map[r][c] = MINE;
            placed++;
        }
    }

    for (size_t i = 0; i < nearCandidates.size() && placed < count; i++)
    {
        int r = nearCandidates[i].first;
        int c = nearCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
            if (hasAdjacentMine(r, c) && (std::rand() % 100) < 80)
                continue;

            map[r][c] = MINE;
            placed++;
        }
    }

    for (size_t i = 0; i < closeCandidates.size() && placed < count; i++)
    {
        int r = closeCandidates[i].first;
        int c = closeCandidates[i].second;

        if (map[r][c] == FLOOR)
        {
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

        if (tileType == MINE && hasAdjacentMine(r, c))
            continue;

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
        visited[r][c] = true;

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
    initializeMap();

    int mineCount = 26;
    int nearTorchMineCount = 14;

    if (difficulty == Difficulty::EASY)
    {
        mineCount = 21;
        nearTorchMineCount = 9;
    }
    else if (difficulty == Difficulty::HARD)
    {
        mineCount = 36;
        nearTorchMineCount = 20;
    }

    buildFixedTorchNetwork(difficulty);
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

        if (zoneCount[zoneIndex] >= 2)
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
        if (hasNearbyTorch(r, c, minTorchDistance - 1))
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
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }

    backgroundTexture = IMG_LoadTexture(renderer, backgroundPath);
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
            bool testerRevealTorchTile = testerRevealTorches && map[r][c] == TORCH;
            bool testerRevealMineTile = testerRevealMines && map[r][c] == MINE;
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
                        if (torchState[r][c] == TORCH_SUCCESS)
                            SDL_SetRenderDrawColor(renderer, 255, 200, 60, 255);
                        else if (torchState[r][c] == TORCH_FAILED)
                            SDL_SetRenderDrawColor(renderer, 110, 35, 35, 255);
                        else
                            SDL_SetRenderDrawColor(renderer, 255, 160, 30, 255);
                        SDL_RenderFillRect(renderer, &tileRect);
                    }
                    else if (map[r][c] == MINE)
                    {
                        SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
                        SDL_RenderFillRect(renderer, &tileRect);
                    }
                    else if (map[r][c] == GOAL)
                    {
                        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                        SDL_RenderFillRect(renderer, &tileRect);
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
            SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
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
    int steps = 3;

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

        if (!(r == startRow && c == startCol) && !(r == goalRow && c == goalCol) && map[r][c] == MINE)
            map[r][c] = FLOOR;

        revealCell(r, c);

        if ((std::rand() % 100) < 55)
        {
            int sideR = r;
            int sideC = c;
            if (moveRow)
                sideC += ((std::rand() % 2) == 0) ? 1 : -1;
            else
                sideR += ((std::rand() % 2) == 0) ? 1 : -1;

            if (sideR >= 0 && sideR < ROWS && sideC >= 0 && sideC < COLS)
            {
                if (!(sideR == startRow && sideC == startCol) && !(sideR == goalRow && sideC == goalCol) && map[sideR][sideC] == MINE)
                    map[sideR][sideC] = FLOOR;
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

    hintTorchRow = targetRow;
    hintTorchCol = targetCol;
    hintUntilTick = SDL_GetTicks() + 8000;
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
    if (solvedTorchCount >= 6)
        desiredBranches = 3;

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
        revealTowardGoalPath(fromRow, fromCol, 2);
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

        revealTowardGoalPath(bestRow, bestCol, 2);
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

        if (!(r == startRow && c == startCol) && !(r == goalRow && c == goalCol) && map[r][c] == MINE)
            map[r][c] = FLOOR;

        revealCell(r, c);

        int sideRevealCount = 1;
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

            if (!(nr == startRow && nc == startCol) && !(nr == goalRow && nc == goalCol) && map[nr][nc] == MINE)
                map[nr][nc] = FLOOR;

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
    testerRevealMines = revealMines;
    testerRevealTorches = revealTorches;
}

void MapManager::clean()
{
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
}
