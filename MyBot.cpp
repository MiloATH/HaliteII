#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"
#include <algorithm>

using namespace std;
using namespace hlt;

struct DistanceFunc
{
    DistanceFunc(const Entity& _p) : p(_p) {}
    
    bool operator()(const Entity& lhs, const Entity& rhs) const
    {
        return p.location.get_distance_to(lhs.location) < p.location.get_distance_to(rhs.location);
    }
    
private:
    Entity p;
};

bool all_on_team(const vector<Entity> &items, PlayerId owner){
    for(const Entity item: items){
        if(item.owner_id != owner){
            return false;
        }
    }
    return true;
}

/*// EFFECT: Returns true if all ships in items are
bool all_on_team(const vector<EntityId> &items, PlayerId owner){
    vector<Entity> ship
    for(docked_ship_ids)
        map.get_ship(player_id, docked_ship_ids);
    for(const Entity item: items){
        if(item.owner_id != owner){
            return false;
        }
    }
    return true;
}*/



int main() {
    const hlt::Metadata metadata = hlt::initialize("UpClose");
    const hlt::PlayerId player_id = metadata.player_id;

    const hlt::Map& initial_map = metadata.initial_map;

    // We now have 1 full minute to analyse the initial map.
    std::ostringstream initial_map_intelligence;
    initial_map_intelligence
            << "width: " << initial_map.map_width
            << "; height: " << initial_map.map_height
            << "; players: " << initial_map.ship_map.size()
            << "; my ships: " << initial_map.ship_map.at(player_id).size()
            << "; planets: " << initial_map.planets.size();
    hlt::Log::log(initial_map_intelligence.str());

    std::vector<hlt::Move> moves;
    for (;;) {
        moves.clear();
        const hlt::Map map = hlt::in::get_map();
        
        for (const hlt::Ship& ship : map.ships.at(player_id)) {
            bool hasCommand = false;
            if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
                continue;
            }
            std::vector<hlt::Planet> planets = map.planets;
            std::sort(planets.begin(), planets.end(), DistanceFunc(ship));
            for (const hlt::Planet& planet : planets) {
                if (planet.is_full() && planet.owned && planet.owner_id == player_id) {
                    continue;
                }
                
                if (ship.can_dock(planet) && !hasCommand) {
                    if ((!planet.owned || planet.owner_id == player_id)){
                        moves.push_back(hlt::Move::dock(ship.entity_id, planet.entity_id));
                        hasCommand = true;
                        break;
                    } else {
                        // Already at the planet, but currently someone else owns the planet. Thus, move to attacking
                        break;
                    }
                }
                
                const hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_to_dock(map, ship, planet, hlt::constants::MAX_SPEED);
                if (move.second && !hasCommand) {
                    moves.push_back(move.first);
                    hasCommand = true;
                    break;
                }
            }
            if (!hasCommand){
                // Attack nearest enemy ship
                
                // Find ships not on my team
                vector<Ship> enemies;
                for(const pair<PlayerId, std::vector<Ship>> ship: map.ships){
                    if(ship.first != player_id) {
                        enemies.insert(enemies.end(), ship.second.begin(), ship.second.end());
                    }
                }
                
                // Sort by distance from me (ship)
                sort(enemies.begin(), enemies.end(), DistanceFunc(ship));
                for(Ship enemy: enemies) {
                    const hlt::possibly<hlt::Move> move =
                    hlt::navigation::navigate_ship_to_dock(map, ship, enemy, hlt::constants::MAX_SPEED);
                    if (move.second && !hasCommand) {
                        moves.push_back(move.first);
                        hasCommand = true;
                        break;
                    }
                }
            }
        }

        if (!hlt::out::send_moves(moves)) {
            hlt::Log::log("send_moves failed; exiting");
            break;
        }
    }
}
