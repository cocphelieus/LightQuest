#include "MapManager.h"
#include "../config/TesterConfig.h"
#include <SDL2/SDL_image.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <utility>
#include <limits>
#include <algorithm>
#include <random>

// Khởi tạo RNG và đặt trạng thái map về mặc định.
MapManager::MapManager()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    initializeMap();
}

MapManager::~MapManager()
{
    clean();
}

// Xóa sạch toàn bộ grid: đặt mọi ô thành FLOOR, tắt fog, reset torchState.
// Gọi lại mỗi khi bắt đầu stage mới (qua reset()).
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

// Mở fog xung quanh ô xuất phát tùy theo độ khó:
// EASY mở bán kính 1, MEDIUM/HARD chỉ mở đúng ô start.
void MapManager::revealInitialArea()
{
    int revealRadiusValue = 1;
    if (currentDifficulty == Difficulty::MEDIUM)
        revealRadiusValue = 0;
    else if (currentDifficulty == Difficulty::HARD)
        revealRadiusValue = 0;

    revealRadius(startRow, startCol, revealRadiusValue, true);
}

// Đảm bảo luôn có ít nhất một torch trong khoảng cách 1-3 ô so với điểm start.
// Được gọi sau khi sinh mạng lưới torch để không bị trường hợp không có torch khởi đầu.
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

            if (map[r][c] == FLOOR && distance < bestDistance)
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

        if (map[fallbackRow][fallbackCol] == FLOOR)
        {
            map[fallbackRow][fallbackCol] = TORCH;
            torchState[fallbackRow][fallbackCol] = TORCH_NOT_USED;
        }
        return;
    }

    map[bestRow][bestCol] = TORCH;
    torchState[bestRow][bestCol] = TORCH_NOT_USED;
}

// Đặt ngẫu nhiên `count` tile loại `tileType` vào map.
// Với MINE: dùng thuật toán score theo vùng + khoảng cách để tránh cụm mìn dày.
// Với các loại khác: chọn ngẫu nhiên thông thường.
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
                if (map[r][c] != FLOOR)
                    continue;

                candidates.push_back({r, c});
            }
        }

        if (candidates.empty())
            return;

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

        int zoneMineCount[zoneRows * zoneCols] = {0};
        for (int r = 0; r < ROWS; r++)
        {
            for (int c = 0; c < COLS; c++)
            {
                if (map[r][c] == MINE)
                    zoneMineCount[getZoneIndex(r, c)]++;
            }
        }

        static std::mt19937 mineRng(static_cast<unsigned int>(std::time(nullptr)) + 811U);
        std::uniform_int_distribution<int> tieBreaker(0, 7);

        int placed = 0;
        while (placed < count)
        {
            int bestIndex = -1;
            int bestScore = std::numeric_limits<int>::min();

            for (size_t i = 0; i < candidates.size(); i++)
            {
                int r = candidates[i].first;
                int c = candidates[i].second;

                if (map[r][c] != FLOOR)
                    continue;

                int startDistance = std::abs(r - startRow) + std::abs(c - startCol);
                int goalDistance = std::abs(r - goalRow) + std::abs(c - goalCol);
                if (startDistance <= 2 || goalDistance <= 1)
                    continue;

        // Quy tắc bắt buộc: không có mìn nào được tỏa nhô cạnh mìn khác.
                if (hasMineWithinRadius(r, c, 1))
                    continue;

                int nearestMineDistance = ROWS + COLS;
                for (int rr = 0; rr < ROWS; rr++)
                {
                    for (int cc = 0; cc < COLS; cc++)
                    {
                        if (map[rr][cc] != MINE)
                            continue;
                        int distance = std::abs(rr - r) + std::abs(cc - c);
                        if (distance < nearestMineDistance)
                            nearestMineDistance = distance;
                    }
                }
                if (nearestMineDistance == ROWS + COLS)
                    nearestMineDistance = 6;

                int zone = getZoneIndex(r, c);
                int score = 0;
                score += nearestMineDistance * 40;
                score -= zoneMineCount[zone] * 28;
                score -= tieBreaker(mineRng);

                if (score > bestScore)
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
            zoneMineCount[getZoneIndex(r, c)]++;
            placed++;
        }

        return;
    }

    int placed = 0;
    int attempts = 0;
    int maxAttempts = ROWS * COLS * 200;

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

            int nearbyMines = countAdjacentMines(r, c);
            if (nearbyMines >= 3)
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
                if (nearbyMines >= 3)
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
            if (countAdjacentMines(r, c) >= 4)
                continue;
            if (createsDenseMineCluster(r, c))
                continue;
        }

        map[r][c] = tileType;
        placed++;
    }
}

// Đặt `count` mìn sao cho mỗi torch đều có ít nhất 1 mìn gần bên.
// 4 pass: phân bố đều → đẩy lên 4 mìn/torch → bù torch thiếu → láp đầy.
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

    auto edgeDistance = [this](int row, int col) -> int
    {
        int dTop = row;
        int dBottom = ROWS - 1 - row;
        int dLeft = col;
        int dRight = COLS - 1 - col;
        return std::min(std::min(dTop, dBottom), std::min(dLeft, dRight));
    };

    auto torchNearbyMineCount = [this](int row, int col) -> int
    {
        int nearby = 0;
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
                    nearby++;
            }
        }
        return nearby;
    };

    auto canPlaceMine = [this](int row, int col, int maxAdjacent) -> bool
    {
        if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
            return false;
        if ((row == startRow && col == startCol) || (row == goalRow && col == goalCol))
            return false;
        if (map[row][col] != FLOOR)
            return false;
        if (createsDenseMineCluster(row, col))
            return false;
        if (countAdjacentMines(row, col) > maxAdjacent)
            return false;
        return true;
    };

    auto scoreCell = [&](int rr, int cc, int distance, bool strictPass) -> int
    {
        int score = 0;
        score += distance * 16; // keep mines close to torches
        score += rowMineCount(rr) * 10;
        score += colMineCount(cc) * 9;
        score += (countAdjacentMines(rr, cc) * 14);

        int edge = edgeDistance(rr, cc);
        if (edge == 0)
            score += strictPass ? 120 : 70;
        else if (edge == 1)
            score += strictPass ? 40 : 22;

        score += std::rand() % 11;
        return score;
    };

    auto placeOneNearTorch = [&](int torchRow, int torchCol, bool strictPass) -> bool
    {
        int bestRow = -1;
        int bestCol = -1;
        int bestScore = std::numeric_limits<int>::max();
        int maxAdjacent = strictPass ? 2 : 4;

        for (int dr = -2; dr <= 2; dr++)
        {
            for (int dc = -2; dc <= 2; dc++)
            {
                int distance = std::abs(dr) + std::abs(dc);
                if (distance == 0 || distance > 2)
                    continue;

                int rr = torchRow + dr;
                int cc = torchCol + dc;

                if (edgeDistance(rr, cc) == 0)
                    continue;

                if (!canPlaceMine(rr, cc, maxAdjacent))
                    continue;

                int score = scoreCell(rr, cc, distance, strictPass);
                if (score < bestScore)
                {
                    bestScore = score;
                    bestRow = rr;
                    bestCol = cc;
                }
            }
        }

        if (bestRow < 0)
            return false;

        map[bestRow][bestCol] = MINE;
        return true;
    };

    int targetCount = count;
    int placed = 0;

    // Pass 1: phân phối ít nhất 1 mìn quanh mỗi torch để trải đều trước.
    for (size_t i = 0; i < torchCells.size() && placed < targetCount; i++)
    {
        int tr = torchCells[i].first;
        int tc = torchCells[i].second;
        if (torchNearbyMineCount(tr, tc) > 0)
            continue;

        if (placeOneNearTorch(tr, tc, true) || placeOneNearTorch(tr, tc, false))
            placed++;
    }

    // Pass 2: cố gắng đưa mỗi torch lên 4 mìn lân cận.
    for (size_t i = 0; i < torchCells.size() && placed < targetCount; i++)
    {
        int tr = torchCells[i].first;
        int tc = torchCells[i].second;
        if (torchNearbyMineCount(tr, tc) >= 4)
            continue;

        if (placeOneNearTorch(tr, tc, false))
            placed++;
    }

    // Pass 2b: bức ép ít nhất 1 mìn cho torch vẫn chưa có.
    for (size_t i = 0; i < torchCells.size() && placed < targetCount; i++)
    {
        int tr = torchCells[i].first;
        int tc = torchCells[i].second;
        if (torchNearbyMineCount(tr, tc) > 0)
            continue;

        if (placeOneNearTorch(tr, tc, false))
            placed++;
    }

    // Pass 3: tiếp tục lấp đầy bằng cách chọn torch có ít mìn lân cận nhất trước.
    int guard = 0;
    int maxGuard = static_cast<int>(torchCells.size()) * 10 + 32;
    while (placed < targetCount && guard < maxGuard)
    {
        guard++;

        int pickIdx = -1;
        int pickCoverage = std::numeric_limits<int>::max();
        int pickGoalDistance = std::numeric_limits<int>::max();

        for (size_t i = 0; i < torchCells.size(); i++)
        {
            int tr = torchCells[i].first;
            int tc = torchCells[i].second;
            int coverage = torchNearbyMineCount(tr, tc);
            int goalDistance = std::abs(goalRow - tr) + std::abs(goalCol - tc);

            if (coverage < pickCoverage || (coverage == pickCoverage && goalDistance < pickGoalDistance))
            {
                pickCoverage = coverage;
                pickGoalDistance = goalDistance;
                pickIdx = static_cast<int>(i);
            }
        }

        if (pickIdx < 0)
            break;

        int tr = torchCells[static_cast<size_t>(pickIdx)].first;
        int tc = torchCells[static_cast<size_t>(pickIdx)].second;

        if (placeOneNearTorch(tr, tc, false))
        {
            placed++;
            continue;
        }

        bool added = false;
        for (size_t i = 0; i < torchCells.size() && placed < targetCount; i++)
        {
            int rr = torchCells[i].first;
            int cc = torchCells[i].second;
            if (placeOneNearTorch(rr, cc, false))
            {
                placed++;
                added = true;
                break;
            }
        }

        if (!added)
            break;
    }
}

// Đặt 1 tile đơn lẻ tại vị trí ngẫu nhiên thỏa mãn:
// - Không trùng start/goal
// - Nằm ngoài bán kính an toàn
// - Khớp quy tắc visibility (HIDDEN_ONLY / VISIBLE_ONLY / ANY)
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

// Vạch đường an toàn từ (fromRow,fromCol) đến (toRow,toCol).
// Đi theo hướng gần đích nhất, tạo vài bước vòng ngẫu nhiên để đường không quá thẳng.
// Các ô trên đường được đánh dấu safePath[] và mở fog.
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

    int safetyGuard = 0;
    int maxSteps = ROWS * COLS * 6;

    while ((r != toRow || c != toCol) && safetyGuard < maxSteps)
    {
        safetyGuard++;

        int remainDistance = std::abs(r - toRow) + std::abs(c - toCol);
        bool canTryDetour = remainDistance > 2 && (std::rand() % 100) < 12;
        if (canTryDetour)
        {
            int detourRows[4];
            int detourCols[4];
            int detourCount = 0;

            if (r != toRow)
            {
                detourRows[detourCount] = r;
                detourCols[detourCount] = c - 1;
                detourCount++;
                detourRows[detourCount] = r;
                detourCols[detourCount] = c + 1;
                detourCount++;
            }

            if (c != toCol)
            {
                detourRows[detourCount] = r - 1;
                detourCols[detourCount] = c;
                detourCount++;
                detourRows[detourCount] = r + 1;
                detourCols[detourCount] = c;
                detourCount++;
            }

            int bestDetourRow = r;
            int bestDetourCol = c;
            int bestDetourScore = std::numeric_limits<int>::max();

            for (int i = 0; i < detourCount; i++)
            {
                int nr = detourRows[i];
                int nc = detourCols[i];

                if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS)
                    continue;

                int score = 0;
                int nextDistance = std::abs(nr - toRow) + std::abs(nc - toCol);

                score += nextDistance * 8;
                if (nextDistance > remainDistance)
                    score += 18;
                if (visited[nr][nc])
                    score += 18;
                if (map[nr][nc] == MINE)
                    score += 65;

                int nearMines = countAdjacentMines(nr, nc);
                score += nearMines * 7;
                score += std::rand() % 9;

                if (score < bestDetourScore)
                {
                    bestDetourScore = score;
                    bestDetourRow = nr;
                    bestDetourCol = nc;
                }
            }

            if (!(bestDetourRow == r && bestDetourCol == c))
            {
                r = bestDetourRow;
                c = bestDetourCol;
                visited[r][c] = true;
                safePath[r][c] = true;

                if (!(r == goalRow && c == goalCol) && !(r == startRow && c == startCol))
                    revealCell(r, c);

                continue;
            }
        }

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

    while (r != toRow || c != toCol)
    {
        if (r != toRow)
            r += (toRow > r) ? 1 : -1;
        else if (c != toCol)
            c += (toCol > c) ? 1 : -1;

        if (r < 0) r = 0;
        if (r >= ROWS) r = ROWS - 1;
        if (c < 0) c = 0;
        if (c >= COLS) c = COLS - 1;

        visited[r][c] = true;
        safePath[r][c] = true;

        if (!(r == goalRow && c == goalCol) && !(r == startRow && c == startCol))
            revealCell(r, c);
    }
}

// Chọn ngẫu nhiên 1 ô FLOOR đã được mở fog, vầch đường tới đó và đặt torch.
// Dùng để spawn torch khi player vừa giải xong một torch khác.
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

// Tạo lại toàn bộ stage: đặt số mìn/torch theo difficulty và campaign stage,
// sinh mạng lưới torch, vạch đường an toàn, rồi reset fog.
void MapManager::reset(Difficulty difficulty, int campaignStage)
{
    currentDifficulty = difficulty;
    currentCampaignStage = (campaignStage < 0) ? 0 : campaignStage;
    initializeMap();

    safePath[startRow][startCol] = true;
    safePath[goalRow][goalCol] = true;

    int mineCount = 32;
    int nearTorchMineCount = 0;

    if (difficulty == Difficulty::EASY)
    {
        mineCount = 24 + (currentCampaignStage * 2);
        if (mineCount > 34)
            mineCount = 34;

        nearTorchMineCount = currentCampaignStage / 2;
        if (nearTorchMineCount > 8)
            nearTorchMineCount = 8;
    }
    else if (difficulty == Difficulty::MEDIUM)
    {
        mineCount = 32 + (currentCampaignStage * 2);
        if (mineCount > 44)
            mineCount = 44;

        nearTorchMineCount = 4 + currentCampaignStage;
        if (nearTorchMineCount > 14)
            nearTorchMineCount = 14;
    }
    else if (difficulty == Difficulty::HARD)
    {
        mineCount = 42 + (currentCampaignStage * 3);
        if (mineCount > 54)
            mineCount = 54;

        // HARD ramps sharply: more pressure around torches as stages increase.
        nearTorchMineCount = 20 + (currentCampaignStage * 3);
        if (nearTorchMineCount > 46)
            nearTorchMineCount = 46;

        // HARD stage 5 (stage cuối): ép độ bao phủ mìn quanh torch lên mức tối đa.
        if (currentCampaignStage >= 4)
            nearTorchMineCount = 46;
    }

    buildFixedTorchNetwork(difficulty, currentCampaignStage);
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

// Xây mạng lưới torch trải đều theo các "band" tiến độ từ start đến goal.
// Torch đủ xa nhau (minTorchDistance), liên kết theo cây safe path.
// Sau đó gọi ensureStarterTorch() để đảm bảo có torch gần start.
void MapManager::buildFixedTorchNetwork(Difficulty difficulty, int campaignStage)
{
    int torchTarget = 16;
    if (difficulty == Difficulty::EASY)
        torchTarget = 18;
    else if (difficulty == Difficulty::MEDIUM)
        torchTarget = 16;
    else if (difficulty == Difficulty::HARD)
        torchTarget = 14;

    int stagePenalty = campaignStage / 2;
    if (difficulty == Difficulty::HARD)
        stagePenalty += campaignStage / 3;

// Điều chỉnh campaign 5 stage: làm stage cuối khó hơn bằng cách giảm số torch.
    if (difficulty == Difficulty::HARD && campaignStage >= 4)
        stagePenalty += 2;

    torchTarget -= stagePenalty;
    if (difficulty == Difficulty::EASY && torchTarget < 14)
        torchTarget = 14;
    if (difficulty == Difficulty::MEDIUM && torchTarget < 12)
        torchTarget = 12;
    if (difficulty == Difficulty::HARD && torchTarget < 9)
        torchTarget = 9;

    int minTorchDistance = 3;
    if (difficulty != Difficulty::EASY)
        minTorchDistance = 4;
    if (difficulty == Difficulty::HARD)
        minTorchDistance = 5;

    if (campaignStage >= 4 && minTorchDistance < 6)
        minTorchDistance = 6;

    const int bandCount = (difficulty == Difficulty::EASY) ? 6 : 7;
    const int totalProgress = (goalRow - startRow) + (goalCol - startCol);
    std::vector<std::vector<std::pair<int, int>>> bandCandidates(static_cast<size_t>(bandCount));

    auto edgeDistance = [this](int row, int col) -> int
    {
        int dTop = row;
        int dBottom = ROWS - 1 - row;
        int dLeft = col;
        int dRight = COLS - 1 - col;
        return std::min(std::min(dTop, dBottom), std::min(dLeft, dRight));
    };

    for (int r = 1; r < ROWS - 1; r++)
    {
        for (int c = 1; c < COLS - 1; c++)
        {
            if ((r == startRow && c == startCol) || (r == goalRow && c == goalCol))
                continue;

            int progress = (r - startRow) + (c - startCol);
            int band = 0;
            if (totalProgress > 0)
                band = (progress * bandCount) / (totalProgress + 1);
            if (band < 0)
                band = 0;
            if (band >= bandCount)
                band = bandCount - 1;

            bandCandidates[static_cast<size_t>(band)].push_back({r, c});
        }
    }

    static std::mt19937 torchRng(static_cast<unsigned int>(std::time(nullptr)) + 313U);
    for (size_t i = 0; i < bandCandidates.size(); i++)
        std::shuffle(bandCandidates[i].begin(), bandCandidates[i].end(), torchRng);

    std::vector<std::pair<int, int>> torchNodes;
    torchNodes.reserve(static_cast<size_t>(torchTarget + 8));

    auto placeTorch = [this, &torchNodes](int row, int col)
    {
        if (map[row][col] != FLOOR)
            return;

        map[row][col] = TORCH;
        torchState[row][col] = TORCH_NOT_USED;
        torchNodes.push_back({row, col});
    };

    int placedTorches = 0;
    int basePerBand = torchTarget / bandCount;
    int extraBands = torchTarget % bandCount;

    // Pass 1: đặt torch theo từng band tiến độ để tạo cảm giác dẫn đường về goal.
    for (int band = 0; band < bandCount && placedTorches < torchTarget; band++)
    {
        int quota = basePerBand + ((band < extraBands) ? 1 : 0);
        if (quota < 1)
            quota = 1;

        int placedInBand = 0;
        int guard = 0;
        int maxGuard = static_cast<int>(bandCandidates[static_cast<size_t>(band)].size()) * 2 + 8;
        while (placedInBand < quota && placedTorches < torchTarget && guard < maxGuard)
        {
            guard++;
            int bestIdx = -1;
            int bestScore = std::numeric_limits<int>::min();

            for (size_t i = 0; i < bandCandidates[static_cast<size_t>(band)].size(); i++)
            {
                int r = bandCandidates[static_cast<size_t>(band)][i].first;
                int c = bandCandidates[static_cast<size_t>(band)][i].second;

                if (map[r][c] != FLOOR)
                    continue;
                if (hasNearbyTorch(r, c, minTorchDistance))
                    continue;

                int nearestTorchDistance = ROWS + COLS;
                for (size_t t = 0; t < torchNodes.size(); t++)
                {
                    int distance = std::abs(torchNodes[t].first - r) + std::abs(torchNodes[t].second - c);
                    if (distance < nearestTorchDistance)
                        nearestTorchDistance = distance;
                }
                if (nearestTorchDistance == ROWS + COLS)
                    nearestTorchDistance = 8;

                int diagonalBias = std::abs((r - startRow) - (c - startCol));
                int score = 0;
                score += nearestTorchDistance * 20;
                score += edgeDistance(r, c) * 8;
                score -= diagonalBias * 3;
                score -= (std::rand() % 9);

                if (score > bestScore)
                {
                    bestScore = score;
                    bestIdx = static_cast<int>(i);
                }
            }

            if (bestIdx < 0)
                break;

            int r = bandCandidates[static_cast<size_t>(band)][static_cast<size_t>(bestIdx)].first;
            int c = bandCandidates[static_cast<size_t>(band)][static_cast<size_t>(bestIdx)].second;
            placeTorch(r, c);
            placedTorches++;
            placedInBand++;
        }
    }

    // Pass 2: bù nếu vẫn chưa đủ số torch, fill toàn cục với quy tắc giẫn cách tương tự.
    for (int band = 0; band < bandCount && placedTorches < torchTarget; band++)
    {
        for (size_t i = 0; i < bandCandidates[static_cast<size_t>(band)].size() && placedTorches < torchTarget; i++)
        {
            int r = bandCandidates[static_cast<size_t>(band)][i].first;
            int c = bandCandidates[static_cast<size_t>(band)][i].second;

            if (map[r][c] != FLOOR)
                continue;
            if (hasNearbyTorch(r, c, minTorchDistance))
                continue;

            placeTorch(r, c);
            placedTorches++;
        }
    }

    if (torchNodes.size() >= 2)
    {
        std::sort(torchNodes.begin(), torchNodes.end(), [this](const std::pair<int, int>& a, const std::pair<int, int>& b)
        {
            int da = std::abs(goalRow - a.first) + std::abs(goalCol - a.second);
            int db = std::abs(goalRow - b.first) + std::abs(goalCol - b.second);
            if (da != db)
                return da > db;
            return a.first + a.second < b.first + b.second;
        });

        int minLinkDistance = (difficulty == Difficulty::EASY) ? 2 : 3;
        int maxLinkDistance = (difficulty == Difficulty::EASY) ? 7 : 8;

        // Nối mỗi torch với 1 torch "cha" gần họn về goal để tránh liên kết chéo lộn xộn.
        for (size_t i = 0; i < torchNodes.size(); i++)
        {
            int rowA = torchNodes[i].first;
            int colA = torchNodes[i].second;
            int goalDistA = std::abs(goalRow - rowA) + std::abs(goalCol - colA);

            int parentIndex = -1;
            int parentScore = std::numeric_limits<int>::max();

            for (size_t j = i + 1; j < torchNodes.size(); j++)
            {
                int rowB = torchNodes[j].first;
                int colB = torchNodes[j].second;
                int distance = std::abs(rowA - rowB) + std::abs(colA - colB);
                int goalDistB = std::abs(goalRow - rowB) + std::abs(goalCol - colB);

                if (goalDistB >= goalDistA)
                    continue;

                int progressStep = goalDistA - goalDistB;
                if (distance < minLinkDistance || distance > maxLinkDistance)
                    continue;

                int score = distance * 5 + std::abs(progressStep - 3) * 6;
                if (score < parentScore)
                {
                    parentScore = score;
                    parentIndex = static_cast<int>(j);
                }
            }

            if (parentIndex == -1)
            {
                int fallbackScore = std::numeric_limits<int>::max();
                for (size_t j = i + 1; j < torchNodes.size(); j++)
                {
                    int rowB = torchNodes[j].first;
                    int colB = torchNodes[j].second;
                    int goalDistB = std::abs(goalRow - rowB) + std::abs(goalCol - colB);
                    if (goalDistB >= goalDistA)
                        continue;

                    int distance = std::abs(rowA - rowB) + std::abs(colA - colB);
                    if (distance < fallbackScore)
                    {
                        fallbackScore = distance;
                        parentIndex = static_cast<int>(j);
                    }
                }
            }

            if (parentIndex != -1)
                carveSafePath(rowA, colA, torchNodes[static_cast<size_t>(parentIndex)].first,
                              torchNodes[static_cast<size_t>(parentIndex)].second);
        }

        carveSafePath(startRow, startCol, torchNodes.front().first, torchNodes.front().second);
        carveSafePath(torchNodes.back().first, torchNodes.back().second, goalRow, goalCol);
    }

    ensureStarterTorch();
}

// Load texture nền và icon các thực thể (mìn, torch), sau đó reset map.
bool MapManager::load(SDL_Renderer *renderer, const char *backgroundPath, Difficulty difficulty)
{
    // Hủy texture cũ nếu có từ lần load trước.
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
    // Load icon đầu đầu; nếu thiếu file, render sẽ fallback về vẽ màu.
    mineTexture = IMG_LoadTexture(renderer, "assets/images/entities/bom.png");
    torchTexture = IMG_LoadTexture(renderer, "assets/images/entities/fire.png");
    // Chưa có nh goal riêng; tạm thời để null (render sẽ vẽ màu tím).
    goalTexture = nullptr;

    reset(difficulty);
    return backgroundTexture != nullptr;
}

// Mở fog ô (row,col): đánh dấu cả visible[] lẫn shadowVisible[].
// Nếu clearMinesOnReveal đang bật (trong death sequence), xóa mìn khi mở.
void MapManager::revealCell(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return;

    if (clearMinesOnReveal && map[row][col] == MINE)
        map[row][col] = FLOOR;

    visible[row][col] = true;
    shadowVisible[row][col] = true;
}

// Mở shadowVisible[] đơn thuần (vùng bóng tối nhờ nhờ), không mở visible[].
void MapManager::revealShadowCell(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return;

    shadowVisible[row][col] = true;
}

// Hàm tiện ích trả về dấu của giá trị (-1, 0, 1).
int MapManager::sign(int value) const
{
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

// Kiểm tra o (row,col) có nằm về phía goal so với điểm cơ sở (baseRow,baseCol) không.
// Dùng để ưu tiên reveal hướng tiến thù lên goal.
bool MapManager::isForwardCell(int baseRow, int baseCol, int row, int col) const
{
    int goalVecR = goalRow - baseRow;
    int goalVecC = goalCol - baseCol;
    int testVecR = row - baseRow;
    int testVecC = col - baseCol;
    int dot = goalVecR * testVecR + goalVecC * testVecC;
    return dot > 0;
}

// Kiểm tra có ô hàng xóm nào vẫn bị ẩn (fog) quanh (row,col) không.
// Dùng để chọn vị trí spawn torch tại bỉa fog.
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

// Kiểm tra có mìn nào liền kề (8 hướng) với (row,col) không.
bool MapManager::hasAdjacentMine(int row, int col) const
{
    return countAdjacentMines(row, col) > 0;
}

// Đếm số mìn liền kề (8 hướng) xung quanh ô (row,col).
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

// Kiểm tra có mìn nào trong bán kính Manhattan `radius` xung quanh (row,col) không.
bool MapManager::hasMineWithinRadius(int row, int col, int radius) const
{
    if (radius < 1)
        return false;

    for (int dr = -radius; dr <= radius; dr++)
    {
        for (int dc = -radius; dc <= radius; dc++)
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

// Kiểm tra đặt mìn tại (row,col) có tạo ra cụm 2x2 toàn mìn không.
// Dùng để ngăn map có "mường mìn" dày khó đi qua.
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

            if (mineCount >= 4)
                return true;
        }
    }

    return false;
}

// Kiểm tra có torch nào trong khoảng cách < minDistance so với (row,col) không.
// Dùng để giữ torch đủ xa nhau khi sinh.
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

// Vẽ toàn bộ map mỗi frame:
// 1. Nền (background texture)
// 2. Từng tile: Floor/Wall/Mine/Torch/Goal với hiệu ứng glow, icon, safePath
// 3. Fog of war 3 lớp: ẩn hoàn toàn / bóng mờ / đã biết
// 4. Gợi ý torch nhấp nháy nếu hint đang hoạt động
void MapManager::render(SDL_Renderer* renderer)
{
    if (backgroundTexture)
    {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }

    int offsetX = RENDER_OFFSET_X;
    int offsetY = RENDER_OFFSET_Y;
    int iconPadding = TILE_SIZE / 14;
    if (iconPadding < 3)
        iconPadding = 3;
    Uint32 ticksNow = SDL_GetTicks();

    SDL_Rect tileRect;
    tileRect.w = TILE_SIZE;
    tileRect.h = TILE_SIZE;

    auto clampRectToTile = [](SDL_Rect& rect, const SDL_Rect& tile)
    {
        if (rect.x < tile.x)
            rect.x = tile.x;
        if (rect.y < tile.y)
            rect.y = tile.y;

        int maxRight = tile.x + tile.w;
        int maxBottom = tile.y + tile.h;
        if (rect.x + rect.w > maxRight)
            rect.w = maxRight - rect.x;
        if (rect.y + rect.h > maxBottom)
            rect.h = maxBottom - rect.y;

        if (rect.w < 1)
            rect.w = 1;
        if (rect.h < 1)
            rect.h = 1;
    };

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            tileRect.x = offsetX + c * TILE_SIZE;
            tileRect.y = offsetY + r * TILE_SIZE;
            bool alwaysRevealGoalTile = map[r][c] == GOAL;
            bool testerRevealTorchTile = kTesterEnabledBuild && testerRevealTorches && map[r][c] == TORCH;
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

                // Hiển thị đường an toàn (xanh lá nhạt) khi torch được giải xong.
                if (visible[r][c] && safePath[r][c] && map[r][c] == FLOOR)
                {
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 82, 132, 88, 110);
                    SDL_RenderFillRect(renderer, &tileRect);
                }

                if (revealDetailTile)
                {
                    if (map[r][c] == TORCH)
                    {
                        SDL_Rect iconRect = tileRect;
                        int torchInsetX = TILE_SIZE / 16;
                        int torchInsetY = TILE_SIZE / 18;
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
                        clampRectToTile(iconRect, tileRect);

                        SDL_Color glowColor = {255, 160, 30, 90};
                        if (torchState[r][c] == TORCH_SUCCESS)
                            glowColor = SDL_Color{255, 210, 85, 120};
                        else if (torchState[r][c] == TORCH_FAILED)
                            glowColor = SDL_Color{130, 45, 45, 85};

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 60, 35, 16, 35);
                        SDL_RenderFillRect(renderer, &iconRect);

                        SDL_Rect glowRect = tileRect;
                        int glowInset = TILE_SIZE / 18 + static_cast<int>((ticksNow / 160) % 2);
                        if (glowInset < 2)
                            glowInset = 2;
                        glowRect.x += glowInset;
                        glowRect.y += glowInset;
                        glowRect.w -= glowInset * 2;
                        glowRect.h -= glowInset * 2;
                        clampRectToTile(glowRect, tileRect);

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

                            // Tô màu icon torch theo trạng thái: vàng (thành công) / đỏ (thất bại) / mặc định.
                            if (torchState[r][c] == TORCH_SUCCESS)
                                SDL_SetTextureColorMod(torchTexture, 255, 225, 120);
                            else if (torchState[r][c] == TORCH_FAILED)
                                SDL_SetTextureColorMod(torchTexture, 140, 55, 55);
                            else
                                SDL_SetTextureColorMod(torchTexture, 255, 240, 190);

                            SDL_RenderCopy(renderer, torchTexture, &srcRect, &iconRect);

                            // Đặt lại color mod về mặc định sau khi render.
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
                        clampRectToTile(iconRect, tileRect);

                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer, 28, 28, 28, 110);
                        SDL_RenderFillRect(renderer, &iconRect);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                        SDL_SetRenderDrawColor(renderer, 150, 70, 70, 255);
                        SDL_RenderDrawRect(renderer, &iconRect);

                        SDL_Rect mineShadow = iconRect;
                        mineShadow.x += TILE_SIZE / 18;
                        mineShadow.y += TILE_SIZE / 18;
                        clampRectToTile(mineShadow, tileRect);
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

// Trả về loại tile tại (row,col). Trả WALL nếu toạ độ ngoài giới hạn.
int MapManager::getTile(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return WALL;

    return map[row][col];
}

// Các getter kiểm tra loại tile tại vị trí (row,col).
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

// Kiểm tra torch tại (row,col) có thể bắt đầu câu hỏi không (chưa dùng).
bool MapManager::canAttemptTorch(int row, int col) const
{
    if (!isTorch(row, col))
        return false;
    return torchState[row][col] == TORCH_NOT_USED;
}

// Cập nhật torchState theo kết quả trả lời (đúng/sai) và mở fog ô torch.
void MapManager::resolveTorch(int row, int col, bool correctAnswer)
{
    if (!isTorch(row, col))
        return;

    torchState[row][col] = correctAnswer ? TORCH_SUCCESS : TORCH_FAILED;

    revealCell(row, col);
}

// Tìm torch gần start nhất và mở fog xung quanh nó.
// Gọi 1 lần duy nhất khi bắt đầu stage để player thấy mục tiêu đầu tiên.
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

// Mở fog theo hướng goal từ vị trí torch vừa giải xong.
// Số bước: 2 (EASY/MEDIUM), 1 (HARD).
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

// Mở fog hình vuông bán kính `radius` xung quanh (row,col).
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

// Mở fog chính xác 1 ô (row,col).
void MapManager::revealAt(int row, int col)
{
    revealCell(row, col);
}

// Mở fog theo 1 bước theo hướng (dRow,dCol) từ (row,col).
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

// Mở shadowVisible lần lượt qua các bước random walk từ (row,col)
// để tạo vùng bóng mờ (nửa tối) 1 cách tự nhiên.
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

// Spawn torch trong vùng đã mở fog gần (centerRow,centerCol).
// Ưu tiên ô ở bìa fog (có hàng xóm ẩn) và gần goal.
// Nếu không tìm được, thử mở thêm fog rồi thử lại.
bool MapManager::spawnTorchInOpenedArea(int centerRow, int centerCol, int maxDistance)
{
    const int minTorchDistance = 4;

    if (maxDistance < 1)
        maxDistance = 1;

    int bestScore = std::numeric_limits<int>::max();
    std::vector<std::pair<int, int>> bestFloor;

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

                if (map[r][c] == FLOOR)
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

        if (map[rr][cc] == FLOOR && !hasNearbyTorch(rr, cc, minTorchDistance))
        {
            map[rr][cc] = TORCH;
            torchState[rr][cc] = TORCH_NOT_USED;
            revealCell(rr, cc);
            return true;
        }
    }

    return false;
}

// Tìm torch chưa giải tiếp theo hướng goal tử vị trí (fromRow,fromCol).
// Nếu không có torch nào "tiến về phía trước", fallback về torch gần nhất bất kỳ hướng.
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

// Tìm torch chưa giải gần nhất (bất kỳ hướng) tử (fromRow,fromCol).
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

// Bật gợi ý (nhấp nháy vàng) tại torch gần nhất khi player trả lời sai.
// Thời gian gợi ý: EASY 8s, MEDIUM 5s, HARD 3s.
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

// Tắt gợi ý torch (reset vị trí và thời gian).
void MapManager::clearTorchHint()
{
    hintTorchRow = -1;
    hintTorchCol = -1;
    hintUntilTick = 0;
}

// Mở đường dẫn đến torch tiếp theo sau khi giải xong 1 torch.
// EASY mở 2 nhánh, MEDIUM/HARD mở 1 nhánh.
// Nếu đây là torch cuối, lọ luôn đường về goal.
bool MapManager::revealPathToNextTorch(int fromRow, int fromCol, int solvedTorchCount)
{
    struct RevealMineGuard
    {
        bool& flag;
        explicit RevealMineGuard(bool& f) : flag(f) { flag = true; }
        ~RevealMineGuard() { flag = false; }
    } guard(clearMinesOnReveal);

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

    // EASY giữ 2 nhánh torch liên kết, nhưng mỗi nhánh reveal ít hơn.
    int desiredBranches = 1;
    if (currentDifficulty == Difficulty::EASY)
        desiredBranches = 2;

    std::vector<std::pair<int, int>> nearCandidates;
    nearCandidates.reserve(candidates.size());

    for (size_t i = 0; i < candidates.size(); i++)
    {
        int distance = std::abs(candidates[i].first - fromRow) + std::abs(candidates[i].second - fromCol);
        if (distance >= 3 && distance <= 5)
            nearCandidates.push_back(candidates[i]);
    }

    if (!nearCandidates.empty())
        candidates.swap(nearCandidates);

    if (static_cast<int>(candidates.size()) < desiredBranches)
    {
        for (int i = 0; i < 3 && static_cast<int>(candidates.size()) < desiredBranches; i++)
        {
            spawnTorchInOpenedArea(fromRow, fromCol, 1);

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
                if (distance >= 3 && distance <= 5)
                    refreshedNearCandidates.push_back(candidates[k]);
            }

            if (!refreshedNearCandidates.empty())
                candidates.swap(refreshedNearCandidates);
        }
    }

    if (candidates.empty())
    {
        // Torch cuối cùng: mở đường thẳng vào goal.
        carveSafePath(fromRow, fromCol, goalRow, goalCol);
        revealCell(goalRow, goalCol);
        return true;
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
    int targetRevealRadius = 1;
    if (currentDifficulty == Difficulty::EASY)
        targetRevealRadius = 0;

    for (size_t i = 0; i < selectedIndices.size(); i++)
    {
        int idx = selectedIndices[i];
        int targetRow = candidates[static_cast<size_t>(idx)].first;
        int targetCol = candidates[static_cast<size_t>(idx)].second;

        carveSafePath(fromRow, fromCol, targetRow, targetCol);
        revealRadius(targetRow, targetCol, targetRevealRadius, true);
        openedTargets.push_back({targetRow, targetCol});

        openedAny = true;
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

        int fromGoalDistance = std::abs(goalRow - fromRow) + std::abs(goalCol - fromCol);
        bool openedNearestGoalTorch = true;

        for (int r = 0; r < ROWS; r++)
        {
            for (int c = 0; c < COLS; c++)
            {
                if (map[r][c] != TORCH)
                    continue;
                if (torchState[r][c] != TORCH_NOT_USED)
                    continue;

                int distanceToGoal = std::abs(goalRow - r) + std::abs(goalCol - c);
                if (distanceToGoal < fromGoalDistance)
                {
                    openedNearestGoalTorch = false;
                    break;
                }
            }

            if (!openedNearestGoalTorch)
                break;
        }

        if (openedNearestGoalTorch)
        {
            carveSafePath(fromRow, fromCol, goalRow, goalCol);
            revealCell(goalRow, goalCol);
        }
        else
        {
            int forwardRevealSteps = 2;
            if (currentDifficulty == Difficulty::EASY)
                forwardRevealSteps = 1;
            revealTowardGoalPath(bestRow, bestCol, forwardRevealSteps);
        }
    }

    return openedAny;
}

// Mở fog tiến dần về hướng goal từ (fromRow,fromCol) qua `steps` bước.
// Có xác suất đi lệch sang hướng ngang (35%) để reveal tự nhiên hơn.
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

// Mở fog ngẫu nhiên theo các "random walk" từ tâm để làm vùng nhìn trông tự nhiên.
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

// Spawn thêm mìn vào vùng chưa mở fog (ngoài bán kính 2 quanh player)
// khi wave mới bắt đầu. torchToAdd hiện chưa dùng (dự phòng mở rộng).
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

// Kiểm tra có tồn tại torch chưa giải nào trên bản đồ không.
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

// Đếm tổng số torch trên bản đồ (kể cả đã giải).
int MapManager::getTotalTorchCount() const
{
    int total = 0;
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] == TORCH)
                total++;
        }
    }
    return total;
}

// Đếm số torch đã giải thành công (TORCH_SUCCESS).
int MapManager::getSolvedTorchCount() const
{
    int solved = 0;
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (map[r][c] == TORCH && torchState[r][c] == TORCH_SUCCESS)
                solved++;
        }
    }
    return solved;
}

// Kiểm tra ô (row,col) đã đi qua vùng shadowVisible (bất kỳ mức độ soi nhìn) chưa.
bool MapManager::isKnown(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return false;
    return shadowVisible[row][col];
}

// Kiểm tra ô (row,col) đang hiển thị chi tiết đầy đủ (visible[]) .
bool MapManager::isDetailVisible(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return false;
    return visible[row][col];
}

// Bật/tắt overlay hiện mìn/torch.
// Logic kiểm soát build-type do caller (PlayScene) xử lý; hàm này chỉ gán giá trị.
void MapManager::setTesterOverlay(bool revealMines, bool revealTorches)
{
    testerRevealMines = revealMines;
    testerRevealTorches = revealTorches;
}

// Hiện chưa sử dụng – texture được quản lý trực tiếp trong load()/clean().
void MapManager::cleanTextures(SDL_Renderer* renderer)
{
    (void)renderer;
    // dự phòng cho việc mở rộng sau này
}

// Giải phóng tất cả texture (background, mine, torch, goal).
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
