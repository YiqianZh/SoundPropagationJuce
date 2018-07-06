// PropagationPlanner - classes to handled different methods of sound propagation
// Author - Nic Taylor
#include "PropagationPlanner.h"
#include "RoomGeometry.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <array>

template<class T, class Less>
class PriorityQueue
{
public:
    PriorityQueue(size_t storage_size, const Less& _comp)
        : comp(_comp)
    {
        container.reserve(storage_size);
    }

    void Push(T&& _val);
    void Pop();
    const T& Top() const
    {
        return container[0];
    }
private:
    std::vector<T> container;
    Less comp;
};

template<class T, class Less>
void PriorityQueue<T, Less>::Push(T&& _val)
{
    size_t index = container.size();
    container.emplace_back(_val);

    while (index != 0)
    {
        T& child = container[index];
        index >>= 1;
        T& parent = container[index];
        if (comp(_val, parent))
        {
            std::swap(child, parent);
        }
        else
        {
            break;
        }
    }
}

template<class T, class Less>
void PriorityQueue<T, Less>::Pop()
{
    container[0] = container.back();
    container.pop_back();

    const uint32_t heap = (uint32_t)container.size();
    uint32_t index = 0;
    uint32_t next = 0;
    while (index < heap)
    {
        const uint32_t lhs = (index << 1) + 1;
        if (lhs < heap)
        {
            if (comp(container[lhs], container[next]))
            {
                next = lhs;
            }
            const uint32_t rhs = lhs + 1;
            if (rhs < heap && comp(container[rhs], container[next]))
            {
                next = rhs;
            }
        }
        if (next != index)
        {
            std::swap(container[index], container[next]);
            index = next;
        }
        else
        {
            break;
        }
    }
}

PlannerAStar::PlannerAStar()
{
    grid = std::make_unique<GeometryGrid>();
    grid_cache = std::make_unique<GeometryGridCache>();
}

void PlannerAStar::Preprocess(std::shared_ptr<const RoomGeometry> _room)
{
    room = _room;
    std::fill_n(&(*grid)[0][0], GridResolution * GridResolution, false);

    auto& walls = room->Walls();
    for (const nMath::LineSegment& ls : walls)
    {
        nMath::Vector start = ls.start;
        nMath::Vector end = ls.end;
        // Update Grid
        const int grid_half = (int)GridResolution / 2;
        nMath::LineSegment grid_line{
            start * (float)GridCellsPerMeter + nMath::Vector{ (float)grid_half, (float)grid_half },
            end * (float)GridCellsPerMeter + nMath::Vector{ (float)grid_half, (float)grid_half }
        };

        if (fabsf(end.x - start.x) > fabsf(end.y - start.y))
        {
            if (end.x < start.x)
            {
                std::swap(grid_line.start, grid_line.end);
            }

            const int grid_start = nMath::Max(0, (int)grid_line.start.x);
            const int grid_end = nMath::Min((int)GridResolution, (int)ceilf(grid_line.end.x));

            const float inv_delta = 1.f / (grid_line.start.x - grid_line.end.x);
            const float delta_y = grid_line.start.y - grid_line.end.y;
            int prev_grid_y = -1;
            for (int i = grid_start; i <= grid_end; ++i)
            {
                float t = ((float)i - grid_line.end.x) * inv_delta;
                t = nMath::Max(0.f, nMath::Min(1.f, t)); // end points
                const float y = grid_line.end.y + t * delta_y; // prevent rounding error when start.y == end.y
                const int grid_y = nMath::Min<int>(nMath::Max(0, (int)y), (int)GridResolution - 1);
                if (prev_grid_y >= 0)
                {
                    for (int j = nMath::Min(prev_grid_y, grid_y); j <= nMath::Max(prev_grid_y, grid_y); ++j)
                    {
                        (*grid)[j][i - 1] = true;
                    }
                }
                prev_grid_y = grid_y;
            }
        }
        else
        {
            if (end.y < start.y)
            {
                std::swap(grid_line.start, grid_line.end);
            }

            const int grid_start = nMath::Max(0, (int)grid_line.start.y);
            const int grid_end = nMath::Min((int)GridResolution, (int)ceilf(grid_line.end.y));

            const float inv_delta = 1.f / (grid_line.start.y - grid_line.end.y);
            const float delta_x = grid_line.start.x - grid_line.end.x;
            int prev_grid_x = -1;
            for (int i = grid_start; i <= grid_end; ++i)
            {
                float t = ((float)i - grid_line.end.y) * inv_delta;
                t = nMath::Max(0.f, nMath::Min(1.f, t)); // end points
                const float x = grid_line.end.x + t * delta_x; // prevent rounding error when start.y == end.y
                const int grid_x = nMath::Min<int>(nMath::Max(0, (int)x), (int)GridResolution - 1);
                if (prev_grid_x >= 0)
                {
                    for (int j = nMath::Min(prev_grid_x, grid_x); j <= nMath::Max(prev_grid_x, grid_x); ++j)
                    {
                        (*grid)[i - 1][j] = true;
                    }
                }
                prev_grid_x = grid_x;
            }
        }
    }
}

void PlannerAStar::Plan(const SourceConfig& _config)
{
    std::fill_n(&(*grid_cache)[0][0], GridResolution * GridResolution, -1.f);

    //if (grid_source.x < 0 || grid_source.y < 0 ||
    //    grid_receiver.x < 0 || grid_receiver.y < 0 ||
    //    grid_source.x >= GridResolution || grid_source.y >= GridResolution ||
    //    grid_receiver.x >= GridResolution || grid_receiver.y >= GridResolution)
    //{
    //    return 0.f;
    //};

    const int grid_half = (int)GridResolution / 2;
    const nMath::Vector grid_source = _config.source * (float)GridCellsPerMeter + nMath::Vector{ (float)grid_half, (float)grid_half };
    source_coord = Coord{ (int)grid_source.y, (int)grid_source.x };
    for (int i = 0; i < GridResolution; ++i)
    {
        for (int j = 0; j < GridResolution; ++j)
        {
            FindAStarDiscrete(Coord{ i, j });
        }
    }
}

float PlannerAStar::FindAStarDiscrete(const Coord& receiver_coord)
{
    if ((*grid)[receiver_coord.row][receiver_coord.col]) // wall
    {
        (*grid_cache)[receiver_coord.row][receiver_coord.col] = 0.f;
        return 0.f;
    }

    auto heuristic = [&receiver_coord](const Coord& c)
    {
        return (uint32_t)(sqrtf((float)((c.col - receiver_coord.col)*(c.col - receiver_coord.col) +
            (c.row - receiver_coord.row)*(c.row - receiver_coord.row))) * 1000.f);
    };

    GeometryGridScore heap_score;
    const GridNode empty_node{ INT_MAX, -1, GNS_NOT_FOUND };
    std::fill_n(&heap_score[0][0], GridResolution * GridResolution, empty_node);
    typedef std::pair<uint32_t, Coord> ScoredCoord;
    auto compareScore = [](const ScoredCoord& lhs, const ScoredCoord& rhs)
    {
        return lhs.first < rhs.first;
    };

    PriorityQueue<ScoredCoord, decltype(compareScore)> prediction((size_t)(8 * M_SQRT2 * GridResolution), compareScore);

    const uint32_t ideal_distance = heuristic(source_coord);
    prediction.Push(ScoredCoord(ideal_distance, source_coord));

    // Start
    heap_score[source_coord.row][source_coord.col] = GridNode{ 0, -1, GNS_FOUND };

    static Coord neighbors[] = {
        { -1, -1 },
        { -1,  0 },
        { -1,  1 },
        { 0, -1 },
        { 0,  1 },
        { 1, -1 },
        { 1,  0 },
        { 1,  1 },
    };

    const int max_test = (int)(GridResolution * GridResolution / 2);
    uint32_t num_checked = 0;
    uint32_t num_discovered = 1;
    static const uint32_t score_diagonal = (uint32_t)(M_SQRT2*1000.f);
    while (num_discovered)
    {
        Coord next_coord = prediction.Top().second;
        GridNode& node = heap_score[next_coord.row][next_coord.col];
        if (node.state == GNS_CHECKED)
        {
            prediction.Pop();
            continue;
        }
        node.state = GNS_CHECKED;
        --num_discovered;

        if (next_coord.col == receiver_coord.col &&
            next_coord.row == receiver_coord.row)
        {
            break;
        }
        if (++num_checked > max_test)
        {
            return 0.f;
        }

        prediction.Pop();

        const uint32_t score = node.score;
        for (int i = 0; i < 8; ++i)
        {
            Coord neighbor = neighbors[i];
            const uint32_t neighbor_dist = (neighbor.row == 0 || neighbor.col == 0) ? 1000 : score_diagonal;
            neighbor.row += next_coord.row;
            neighbor.col += next_coord.col;
            if (neighbor.row >= 0 && neighbor.row < GridResolution &&
                neighbor.col >= 0 && neighbor.col < GridResolution)
            {
                GridNode& neighbor_info = heap_score[neighbor.row][neighbor.col];
                if ((*grid)[neighbor.row][neighbor.col])
                {
                    continue; // not a neighbor
                }
                if (neighbor_info.state == GNS_CHECKED)
                {
                    continue;
                }

                const uint32_t neighbor_score = score + neighbor_dist;
                if (neighbor_info.score > neighbor_score)
                {
                    if (neighbor_info.state == GNS_NOT_FOUND)
                    {
                        ++num_discovered;
                    }

                    heap_score[neighbor.row][neighbor.col] = GridNode{ neighbor_score, (int8_t)i, GNS_FOUND };
                    prediction.Push(ScoredCoord{ neighbor_score + heuristic(neighbor), neighbor });
                }
            }
        }
    }

    Coord next_coord = prediction.Top().second;
    const float distance = nMath::Max(FLT_EPSILON, heap_score[next_coord.row][next_coord.col].score / (1000.f * GridCellsPerMeter));
    const float geometric_attenuation = nMath::Min(1.f, 1.f / distance);

    (*grid_cache)[receiver_coord.row][receiver_coord.col] = geometric_attenuation;

    //if (capture_debug)
    //{
    //    const int grid_half = (int)GridResolution / 2;
    //    Coord coord = next_coord;
    //    while (heap_score[coord.row][coord.col].link_index >= 0)
    //    {
    //        GridNode& coord_info = heap_score[coord.row][coord.col];
    //        Coord incoming_coord = coord;
    //        incoming_coord.row -= neighbors[coord_info.link_index].row;
    //        incoming_coord.col -= neighbors[coord_info.link_index].col;
    //        nMath::LineSegment segment{
    //            { ((float)coord.col - grid_half) / GridCellsPerMeter, ((float)coord.row - grid_half) / GridCellsPerMeter, 0.f },
    //            { ((float)incoming_coord.col - grid_half) / GridCellsPerMeter, ((float)incoming_coord.row - grid_half) / GridCellsPerMeter, 0.f }
    //        };
    //        CaptureDebug<capture_debug>(segment);
    //        coord = incoming_coord;
    //    }
    //}
    return geometric_attenuation;
}

void PlannerAStar::Simulate(PropagationResult& result, const nMath::Vector& _receiver, const float _time_ms) const
{
    (void)_time_ms;

    const int grid_half = (int)GridResolution / 2;
    nMath::Vector grid_receiver = _receiver * (float)GridCellsPerMeter + nMath::Vector{ (float)grid_half, (float)grid_half };

    if (grid_receiver.x < 0 || grid_receiver.y < 0 ||
        grid_receiver.x >= GridResolution || grid_receiver.y >= GridResolution)
    {
        result.gain = 0.f;
        return;
    };

    const Coord receiver_coord = Coord{ (int)grid_receiver.y, (int)grid_receiver.x };

    const nMath::Vector cell_receiver{ grid_receiver.x - (float)receiver_coord.col, grid_receiver.y - (float)receiver_coord.row, 0.f };

    float bary[3];
    Coord coord_tri[3];
    // Find the three cell coordinates closest to the receiver.
    if (cell_receiver.x < 0.5f)
    {
        coord_tri[0] = { 0, 0 };
        coord_tri[1] = { 1, 0 };
        if (cell_receiver.y < 0.5f)
        {
            bary[1] = cell_receiver.y;
            bary[2] = cell_receiver.x;
            coord_tri[2] = { 0, 1 };
        }
        else
        {
            bary[2] = cell_receiver.x;
            bary[1] = cell_receiver.y - bary[2];
            coord_tri[2] = { 1, 1 };
        }
    }
    else
    {
        // To treat coord_tri[0] as 0, imagine the triangle being shifted left by 1 col.
        // This means the x/col coord is in the negative direction, but becuase we are 
        // guarunteed to be on or in the triangle, that component of the triangle needs a
        // weight of 1.f - x.
        coord_tri[0] = { 0, 1 }; // coord_tri[0] = { 0, 1 - 1 }; replace with to solve bary by hand.
        coord_tri[1] = { 1, 1 };
        if (cell_receiver.y < 0.5f)
        {
            bary[1] = cell_receiver.y;
            bary[2] = 1.f - cell_receiver.x;
            coord_tri[2] = { 0, 0 };
        }
        else
        {
            bary[2] = 1.f - cell_receiver.x;
            bary[1] = cell_receiver.y - bary[2];
            coord_tri[2] = { 1, 0 };
        }
    }

    bary[0] = 1.f - bary[1] - bary[2];

    float sum = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        Coord test_coord = receiver_coord;
        test_coord.row += coord_tri[i].row;
        test_coord.col += coord_tri[i].col;
        if (test_coord.col < GridResolution &&
            test_coord.row < GridResolution)
        {
            sum += bary[i] * (*grid_cache)[test_coord.row][test_coord.col];
        }
    }

    result.gain = sum;
}

std::shared_ptr<PropagationPlanner> PropagationPlanner::MakePlanner(const SoundPropagation::MethodType method)
{
    std::shared_ptr<PropagationPlanner> planner = nullptr;
    switch (method)
    {
    case SoundPropagation::Method_SpecularLOS:
        planner = std::make_shared<PlannerSpecularLOS>();
        break;
    case SoundPropagation::Method_RayCasts:
        planner = std::make_shared<PlannerRayCasts>();
        break;
    case SoundPropagation::Method_Pathfinding:
        planner = std::make_shared<PlannerAStar>();
        break;
    case SoundPropagation::Method_Wave:
        planner = std::make_shared<PlannerWave>();
        break;
    default:
        break;
    }

    return planner;
}

void PlannerSpecularLOS::Preprocess(std::shared_ptr<const RoomGeometry> _room)
{
    room = _room;
}

void PlannerSpecularLOS::Plan(const PropagationPlanner::SourceConfig& _config)
{
    source = _config.source;
}

void PlannerSpecularLOS::Simulate(PropagationResult& result, const nMath::Vector& _receiver, const float _time_ms) const
{
    (void)_time_ms;

    const nMath::LineSegment los = nMath::LineSegment{ source, _receiver };
    if (result.config == SoundPropagation::PRD_FULL)
    {
        result.intersections.clear();
        result.intersections.push_back(los);
    }

    if (room->Intersects(los))
    {
        result.gain = 0.f;
        return;
    }

    const float distance = nMath::Max(FLT_EPSILON, nMath::Length(source - _receiver));
    const float geometric_attenuation = nMath::Min(1.f, 1.f / distance);
    result.gain = geometric_attenuation;
}

void PlannerRayCasts::Plan(const PropagationPlanner::SourceConfig& _config)
{
    source = _config.source;
}

void PlannerRayCasts::Preprocess(std::shared_ptr<const RoomGeometry> _room)
{
    room = _room;
}

bool PlannerRayCasts::Intersects(PropagationResult& result, const nMath::Vector& start, const nMath::Vector& end) const
{
    const nMath::LineSegment ls{ start, end };
    if (result.config == SoundPropagation::PRD_FULL)
    {
        result.intersections.push_back(ls);
    }

    return room->Intersects(ls);
}

void PlannerRayCasts::Simulate(PropagationResult& result, const nMath::Vector& _receiver, const float _time_ms) const
{
    (void)_time_ms;

    nMath::Vector direction = source - _receiver;
    float distance = nMath::Length(direction);
    if (result.config == SoundPropagation::PRD_FULL)
    {
        result.intersections.clear();
    }
    if (Intersects(result, source, _receiver))
    {
        if (distance > FLT_EPSILON)
        {
            direction = direction / distance;
            nMath::Vector ray_orth = { -direction.y, direction.x, 0.f };
            nMath::Vector ray_orth_inv = -1.f * ray_orth;
            if ((!Intersects(result, ray_orth + _receiver, _receiver) &&
                !Intersects(result, ray_orth + _receiver, source)) ||
                (!Intersects(result, ray_orth_inv + _receiver, _receiver) &&
                    !Intersects(result, ray_orth_inv + _receiver, source)))
            {
                distance += 1.f;
            }
            else if ((!Intersects(result, ray_orth + source, source) &&
                !Intersects(result, ray_orth + source, _receiver)) ||
                (!Intersects(result, ray_orth_inv + source, source) &&
                    !Intersects(result, ray_orth_inv + source, _receiver)))
            {
                distance += 1.f;
            }
            else
            {
                result.gain = 0.f;
                return;
            }
        }
        else
        {
            result.gain = 0.f;
            return;
        }
    }

    const float geometric_attenuation = nMath::Min(1.f, 1.f / nMath::Max(FLT_EPSILON, distance));
    result.gain = geometric_attenuation;
}

void PlannerWave::Plan(const PropagationPlanner::SourceConfig& _config)
{
    source = _config.source;
    frequency = _config.frequency;
    time_factor = 1.f / _config.time_scale;

    first_reflections.clear();
    auto& walls = room->Walls();
    first_reflections.reserve(walls.size());
    for (const nMath::LineSegment& wall : walls)
    {
        nMath::Vector&& reflection = nMath::Project(wall, source);
        first_reflections.emplace_back(reflection);
    }
}

void PlannerWave::Preprocess(std::shared_ptr<const RoomGeometry> _room)
{
    room = _room;
}

void PlannerWave::Simulate(PropagationResult& result, const nMath::Vector& _receiver, const float _time_ms) const
{
    const nMath::LineSegment los = nMath::LineSegment{ source, _receiver };
    if (result.config == SoundPropagation::PRD_FULL)
    {
        result.intersections.clear();
        result.intersections.push_back(los);
    }
    if (room->Intersects(los))
    {
        result.gain = 0.f;
        return;
    }

    const float distance = nMath::Length(_receiver - source);
    const float angle = 2.f * (float)M_PI * frequency;
    const float shift = angle * -distance / 340.f;
    const float geometric_attenuation = nMath::Min(1.f, 1.f / distance);
    float value = geometric_attenuation * (1.f + 0.5f * cosf(angle * (_time_ms * time_factor / 1000.f) + shift)); // TODO: Normalization on gfx side

    for (const nMath::Vector& v : first_reflections)
    {
        const float first_distance = nMath::Length(_receiver - v);
        const float first_shift = angle * -(first_distance) / 340.f;
        const float first_geometric_attenuation = nMath::Min(1.f, 1.f / (first_distance + distance));
        float first_value = first_geometric_attenuation * (1.f + 0.5f * sinf(angle * (_time_ms * time_factor / 1000.f) + first_shift)); // TODO: Normalization on gfx side
        value += first_value;
    }

    const float gain = 0.9f;
    result.gain = value * gain;
}