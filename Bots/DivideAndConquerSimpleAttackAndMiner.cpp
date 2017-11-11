#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"
#include <algorithm>

using namespace std;
using namespace hlt;

int DENOMINATOR_OF_FRACTION_OF_ATTACKER = 4;

static vector<Move> moves;
static PlayerId player_id; //const

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

void miner(const Ship &ship, Map &map, vector<Ship> &docked_enemies) {
    bool hasCommand = false;
    if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
        return;
    }
    std::vector<hlt::Planet> &planets = map.planets;
    std::sort(planets.begin(), planets.end(), DistanceFunc(ship));
    for (const hlt::Planet& planet : planets) {
        // Skip over this planet if it is owned by an opponent, or I own it and it is full
        // This will prioritize docking not owned planets
        if (planet.owned && (planet.owner_id != player_id || planet.is_full())) {
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
        vector<Ship> enemies = docked_enemies;
        
        // Find all enemy ships
        /* for(const pair<PlayerId, std::vector<Ship>> players_ships: map.ships){
         if(players_ships.first != player_id) {
         enemies.insert(enemies.end(), players_ships.second.begin(), players_ships.second.end());
         }
         } */
        
        
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


void attacker(const Ship &ship, const Map &map, vector<Ship> &docked_enemies) {
    bool hasCommand = false;
    if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
        moves.push_back(Move::undock(ship.entity_id));
        hasCommand = true;
        return;
    }
    
    if (!hasCommand){
        // Attack nearest enemy ship
        
        if(!docked_enemies.empty()){
            // harass docked enemy ships
            vector<Ship> enemies = docked_enemies;
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
            return;
        }
        else {
            // Find all enemy ships
            vector<Ship> enemies;
            for(const pair<PlayerId, std::vector<Ship>> players_ships: map.ships){
                if(players_ships.first != player_id) {
                    enemies.insert(enemies.end(), players_ships.second.begin(), players_ships.second.end());
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
            return;
        }
    }
}

int main() {
    const hlt::Metadata metadata = hlt::initialize("DivideAndConquer");
    player_id = metadata.player_id;
    
    const hlt::Map& initial_map = metadata.initial_map;

    // Decide on number of attackers to miners
    if(initial_map.ship_map.size() == 4){
        // Play a more econ game when there are a lot of players.
        DENOMINATOR_OF_FRACTION_OF_ATTACKER = 10000;
    } else {
        // Calculate the proportion of attackers to miners based on map size
        DENOMINATOR_OF_FRACTION_OF_ATTACKER = 4 * ((initial_map.map_height * initial_map.map_width)/(240*160));
    }
    
    // We now have 1 full minute to analyse the initial map.
    std::ostringstream initial_map_intelligence;
    initial_map_intelligence
            << "width: " << initial_map.map_width
            << "; height: " << initial_map.map_height
            << "; players: " << initial_map.ship_map.size()
            << "; my ships: " << initial_map.ship_map.at(player_id).size()
            << "; planets: " << initial_map.planets.size();
    hlt::Log::log(initial_map_intelligence.str());

    for (;;) {
        moves.clear();
        hlt::Map map = hlt::in::get_map();
        
        // Computation for all ships
        vector<Ship> docked_enemies;
        for(const hlt::Planet& planet : map.planets){
            if(planet.owned && planet.owner_id != player_id) {
                vector<Ship> ships;
                for(const EntityId& id : planet.docked_ships) {
                    ships.push_back(map.get_ship(planet.owner_id, id));
                }
                docked_enemies.insert(docked_enemies.end(), ships.begin(), ships.end());
            }
        }
        
        const vector<Ship> &my_ships = map.ships.at(player_id);
        for (int i = 0; i < (int) my_ships.size(); ++i) {
            // Send a fraction of the ships to be attackers, and the rest to be miners
            if(my_ships[i].entity_id % DENOMINATOR_OF_FRACTION_OF_ATTACKER == 0){
                // Be an attacker
                attacker(my_ships[i], map, docked_enemies);
            } else {
                // Be a miner
                miner(my_ships[i], map, docked_enemies);
            }
        }

        if (!hlt::out::send_moves(moves)) {
            hlt::Log::log("send_moves failed; exiting");
            break;
        }
    }
}
