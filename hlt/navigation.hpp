#pragma once

#include "collision.hpp"
#include "map.hpp"
#include "move.hpp"
#include "util.hpp"

namespace hlt {
    namespace navigation {
        
        static std::vector<Location> intended_locations;
        
        static bool there_will_be_my_ship_at(Location &want_to_go) {
            for(Location loc : intended_locations) {
                if(loc.get_distance_to(want_to_go) < constants::FORECAST_FUDGE_FACTOR ) {
                    return true;
                }
            }
            return false;
        }
        
        static bool is_in_map(const Map &map, const Location location) {
           return 0 <= location.pos_x && location.pos_x <= map.map_width
            && 0 <= location.pos_y && location.pos_y < map.map_height;
        }
        
        static Location toLocation(const Location &start, const int thrust, const int rad) {
            Location result;
            result.pos_x = start.pos_x + (thrust * cos(rad));
            result.pos_y = start.pos_y + (thrust * sin(rad));
            return result;
        }
        
        static void check_and_add_entity_between(
                std::vector<const Entity *>& entities_found,
                const Location& start,
                const Location& target,
                const Entity& entity_to_check)
        {
            const Location &location = entity_to_check.location;
            if (location == start || location == target) {
                return;
            }
            if (collision::segment_circle_intersect(start, target, entity_to_check, constants::FORECAST_FUDGE_FACTOR)) {
                entities_found.push_back(&entity_to_check);
            }
        }

        static std::vector<const Entity *> objects_between(const Map& map, const Location& start, const Location& target) {
            std::vector<const Entity *> entities_found;

            for (const Planet& planet : map.planets) {
                check_and_add_entity_between(entities_found, start, target, planet);
            }

            for (const auto& player_ship : map.ships) {
                for (const Ship& ship : player_ship.second) {
                    check_and_add_entity_between(entities_found, start, target, ship);
                }
            }

            return entities_found;
        }

        static possibly<Move> navigate_ship_towards_target(
                const Map& map,
                const Ship& ship,
                const Location& target,
                const int max_thrust,
                const bool avoid_obstacles,
                const int max_corrections,
                const double angular_step_rad)
        {
            if (max_corrections <= 0) {
                return { Move::noop(), false };
            }

            const double distance = ship.location.get_distance_to(target);
            const double angle_rad = ship.location.orient_towards_in_rad(target);
            
            int thrust;
            if (distance < max_thrust) {
                // Do not round up, since overshooting might cause collision.
                thrust = (int) distance;
            } else {
                thrust = max_thrust;
            }
            
            const int angle_deg = util::angle_rad_to_deg_clipped(angle_rad);
            Location result = toLocation(ship.location, thrust, angle_deg);

            if (avoid_obstacles && (!objects_between(map, ship.location, target).empty()
                || !is_in_map(map, result) || there_will_be_my_ship_at(result))) {
                if(there_will_be_my_ship_at(result)) {
                    std::ostringstream str;
                    str << "THERE WILL BE MY SHIP: " << ship.entity_id << " LOCATION: " << ship.location;
                    Log::log(str.str());
                }
                const double new_target_dx = cos(angle_rad + angular_step_rad) * distance;
                const double new_target_dy = sin(angle_rad + angular_step_rad) * distance;
                const Location new_target = { ship.location.pos_x + new_target_dx, ship.location.pos_y + new_target_dy };

                return navigate_ship_towards_target(
                        map, ship, new_target, max_thrust, true, (max_corrections - 1), angular_step_rad);
            }
            
            intended_locations.push_back(result);
            
            return { Move::thrust(ship.entity_id, thrust, angle_deg), true };
        }

        static possibly<Move> navigate_ship_to_dock(
                const Map& map,
                const Ship& ship,
                const Entity& dock_target,
                const int max_thrust)
        {
            const int max_corrections = constants::MAX_NAVIGATION_CORRECTIONS;
            const bool avoid_obstacles = true;
            const double angular_step_rad = M_PI / 180.0;
            const Location& target = ship.location.get_closest_point(dock_target.location, dock_target.radius);

            return navigate_ship_towards_target(
                    map, ship, target, max_thrust, avoid_obstacles, max_corrections, angular_step_rad);
        }
        
        static possibly<Move> navigate_ship_to_location(const Map& map, const Ship& ship, const Location& target) {
            const int max_corrections = constants::MAX_NAVIGATION_CORRECTIONS;
            const bool avoid_obstacles = true;
            const double angular_step_rad = M_PI / 180.0;
            const int max_thrust = constants::MAX_SPEED;
            
            return navigate_ship_towards_target(map, ship, target, max_thrust, avoid_obstacles, max_corrections, angular_step_rad);
        }
    }
}
