// PropagationPlanner - classes to handled different methods of sound propagation from
// geometric approximations to frequency dependent methods based on the wave equation.
// Author - Nic Taylor
#pragma once

#include "PropagationPlanner.h"

class PlannerAStar : public PropagationPlanner
{
private:
    const static uint32_t GridDistance = 60; // meters    
    const static uint32_t GridCellsPerMeter = 2;
    const static uint32_t GridResolution = GridCellsPerMeter * GridDistance;
    typedef std::array<std::array<bool, GridResolution>, GridResolution> GeometryGrid;
    typedef std::array<std::array<float, GridResolution>, GridResolution> GeometryGridCache;
    enum GridNodeState : int8_t
    {
        GNS_NOT_FOUND = 0,
        GNS_FOUND,
        GNS_CHECKED
    };
    struct GridNode
    {
        uint32_t score;
        int8_t link_index;
        GridNodeState state;
    };
    typedef std::array<std::array<GridNode, GridResolution>, GridResolution> GeometryGridScore;

    struct Coord
    {
        int row;
        int col;
    };

public:
    PlannerAStar();
    void Preprocess(std::shared_ptr<const RoomGeometry> _room) override;
    void Plan(const PropagationPlanner::SourceConfig& _config) override;
    void Simulate(PropagationResult& result, const nMath::Vector& _receiver, const float _time_ms) const override;

    const std::unique_ptr<GeometryGrid>& Grid() const
    {
        return grid;
    }
private:
    float FindAStarDiscrete(const Coord& receiver_coord);

    std::shared_ptr<const RoomGeometry> room;
    std::unique_ptr<GeometryGrid> grid;
    std::unique_ptr<GeometryGridCache> grid_cache;
    Coord source_coord;
};