#include <numeric>
#include <iterator>
#include <queue>
#include <vector>

#include <sim.hpp>

template <typename T>
class TD;

namespace sim
{
	std::size_t hex_distance(glm::vec2 a) {
    return hex_distance({0, 0}, a);
	}

	std::size_t hex_distance(glm::vec2 a, glm::vec2 b) {
    using std::abs;
    return (abs(a.x - b.x)
        + abs(a.x + a.y - b.x - b.y)
        + abs(a.y - b.y)) / 2;
	}

	Ability::Ability(int d_hp, int d_ap, int cost): d_hp(d_hp), d_ap(d_ap), cost(cost) {}

	Target::Target(Mob& mob) : mob_(mob) {}

	Mob& Target::mob() { return mob_; }

	Map::Map(std::size_t size): size_(size), hexes_(size, size) {}

	Matrix<HexType> Map::hexes() { return hexes_; }

  HexType& Map::operator()(glm::ivec2 c) {
    return hexes_(c);
  }


	Index<Mob> MobManager::add_mob(Mob mob) {
		mobs_.push_back(std::move(mob));
    return Index<Mob>{mobs_, mobs_.size() - 1};
	}

	Index<Team> MobManager::add_team() {
		teams_.emplace_back(teams_.size() + 1);
		return Index<Team>{teams_, teams_.size() - 1};
	}

	std::vector<Mob>& MobManager::mobs() {
		return mobs_;
	}

	std::vector<Team>& MobManager::teams() {
		return teams_;
	}

  bool MobManager::move_mob(Mob& mob, glm::vec2 to) {
    // TODO-  check that the move is only to a neighbour block
    if (mob.ap > 0) {
      mob.c = to;
      --mob.ap;

      return true;
    } else {
      return false;
    }
  }

  boost::optional<Mob&> MobManager::operator()(glm::ivec2 c) {
    boost::optional<Mob&> result;

    for (auto&& mob : mobs_) {
      if (mob.c.x == c.x && mob.c.y == c.y) {
        result = mob;
        return result;
      }
    }

    return result;
  }

	Pathfinder::Pathfinder(std::size_t size): paths_(size, size), size_(size) {}

	Matrix<Path>& Pathfinder::paths() { return paths_; }

  std::vector<Coord> Pathfinder::path_to(glm::vec2 target) {
    std::vector<Coord> result;

    glm::vec2 current = target;
    result.push_back(current);

    Path path = paths_(current);

    while (path.distance > 0) {
      if (path.source) {
        auto c = *path.source;

        result.push_back(c);
        path = paths_(c);
      } else {
        result.clear();
        break;
      }
    }

    return result;
	}

	void Pathfinder::move_as_far_as_possible(MobManager& mob_manager, Mob& mob, std::vector<Coord>& path) {
    int i = path.size() - 1;

    while (mob.ap > 0 && i > 0) {
      mob_manager.move_mob(mob, path[i]);
    }		
	}

	std::size_t Pathfinder::distance(glm::vec2 c) {
    return paths_(c).distance;
  }

	void Pathfinder::pathfind_from(glm::vec2 start, Map& map, MobManager& mob_manager) {
		std::queue<glm::vec2> queue;
		queue.push(start);

		std::vector<glm::vec2> diffs = {
			{ -1, 0 },
			{ 1, 0 },
			{ 0, -1 },
			{ 0, 1 },
			{ 1, -1 },
			{ -1, 1 }
		};

		for (std::size_t i = 0; i < paths_.m; ++i) {
			for (std::size_t j = 0; j < paths_.n; ++j) {
				auto&& p = paths_(i, j);
				
				p.source = boost::none;
				p.distance = std::numeric_limits<int>::max();
        p.reachable = false;

			}
		}

    paths_(start).distance = 0;
    paths_(start).state = VertexState::Open;
    paths_(start).reachable = true;

    int iterations = 0;

    while (!queue.empty()) {
      auto current = queue.front();
      queue.pop();

      if (iterations > 10000 || queue.size() > 1000) {
        fmt::printf("ERROR: Pathfinding stuck at %d iterations with queue size %lu\n", iterations, queue.size());
      }

      iterations++;

      Path& p = paths_(current);

      if (p.state == VertexState::Closed) continue;

      p.state = VertexState::Closed;

      // TODO - check for walls
      for (auto diff : diffs) {
        auto neighbour = current + diff;
        if (is_valid_coord(neighbour)) {
          Path& n = paths_(neighbour);

          if (n.state != VertexState::Closed) {
            if (n.distance > p.distance + 1) {
              n.distance = p.distance + 1;
              assert(n.distance > 0);

              n.source = current;
              n.reachable = true;
            }

            n.state = VertexState::Open;
            queue.push(neighbour);
          }
        }
      }
    }
	}

  bool Pathfinder::is_valid_coord(glm::vec2 c) {
    std::size_t x = std::abs(c.x);
    std::size_t y = std::abs(c.y);

    return std::max(x, y) < size_ && std::min(c.x, c.y) >= 0;
  }

  Path& Pathfinder::operator()(glm::vec2 c) {
    return paths_(c);
  }

	TurnManager::TurnManager(MobManager& mob_manager): mob_manager_(mob_manager) {}

	bool TurnManager::is_turn_done() const {
		return current_ < turn_order_.size();
	}

	void TurnManager::start_next_turn() {
		turn_order_.clear();
		for (auto&& mob : mob_manager_.mobs()) {
			// TODO - per-turn effects should be applied here
			mob.ap = mob.max_ap;

			if (mob.hp > 0) {
				turn_order_.push_back(&mob);
			}
		}

		auto ap_compare = [](auto m1, auto m2) -> bool {
			return m1->ap < m2->ap;
		};
		std::sort(turn_order_.begin(), turn_order_.end(), ap_compare);
	}

	Mob& TurnManager::current_mob() const {
		assert(current_ < turn_order_.size());
		return *turn_order_[current_];
	}

	bool TurnManager::move_next() {
		if (!is_turn_done()) {
			++current_;
		}

		return is_turn_done();
	}

	void UsableAbility::use() {
    to_.hp -= ability_.d_hp;
    from_.ap -= ability_.cost;

  }

	Game::Game(std::size_t size) : map_(size),
	                            mob_manager_{}, pathfinder_(size),
	                            turn_manager_(mob_manager_), size_(size) {}

  Map& Game::map() {
    return map_;
  }

	MobManager& Game::mob_manager() {
		return mob_manager_;
	}

	Pathfinder& Game::pathfinder() {
		return pathfinder_;
	}

	TurnManager& Game::turn_manager() {
		return turn_manager_;
	}

	Index<Mob> Game::add_mob(Mob mob) {
		return mob_manager_.add_mob(mob);
	}

	bool Game::is_finished() {
		auto mob_alive = [](auto mob) {
			return mob->hp > 0;
		};

		// TODO - introduce a faster check than manually iterate everything
		for (auto&& team : mob_manager_.teams()) {
			auto&& mobs = team.mobs();
			bool none_alive = std::none_of(mobs.begin(), mobs.end(), mob_alive);

			if (none_alive)
				return true;
		}

		return false;
	}

	std::size_t Game::size() const {
		return size_;
	}

	Index<Team> Game::add_team() {
		return mob_manager_.add_team();
	}

  void Game::refresh() {
    auto&& current_mob = turn_manager_.current_mob();
    pathfinder().pathfind_from(current_mob.c, map_, mob_manager_);
  }

	std::vector<Ability> Game::usable_abilities(Mob& mob) const {
		std::vector<Ability> result;
		for (auto&& ability : mob.abilities) {
			if (ability.cost <= mob.ap) {
				result.push_back(ability);
			}
		}

		return result;
	}

	std::vector<UsableAbility> Game::usable_abilities(Mob& mob, Target target, MobManager& mob_manager, Pathfinder& pathfinder) {
    std::vector<UsableAbility> result;

    auto distance = pathfinder.distance(target.mob().c);

    for (auto&& ability : mob.abilities) {
      if (ability.range >= distance && mob.ap >= ability.cost) {
        result.emplace_back(mob, target.mob(), ability);
      }
    }

    return result;
  }

	std::vector<Target> Game::possible_targets(Mob& mob, MobManager& mob_manager, Pathfinder& pathfinder) {
		auto&& abilities = mob.abilities;

		auto f_max = [](auto acc, auto&& ability) { return std::max(acc, ability.range); };
		auto max = std::accumulate(abilities.begin(), abilities.end(), 0, f_max);

		std::vector<Target> result;

		for (auto&& enemy : mob_manager.mobs()) {
			// if (pathfinder.distance(mob.c, enemy.c) <= max) {
      // TODO - fix this to use the actual distance
			if (hex_distance(mob.c, enemy.c) <= max) {
				result.emplace_back(enemy);
			}
		}

		return result;
	}

  int Mob::last_id_ = 0;

	Mob::Mob(int max_hp, int max_ap, abilities_t abilities, Index<Team> team)
		: id_(last_id_++),
    max_hp(max_hp),
		  max_ap(max_ap),
		  abilities(std::move(abilities)),
		  team(std::move(team)) {}

  bool Mob::operator==(const Mob& rhs) const { return id_ == rhs.id_; }
  bool Mob::operator!=(const Mob& rhs) const{ return id_ == rhs.id_; }

	Team::Team(int number) : number(number) {
		using namespace std;
		random_device rd;
		mt19937 gen(rd());
		uniform_real_distribution<float> dis(0.0f, 1.0f);

		color_ = {dis(gen), dis(gen), dis(gen)};
	}

	void Team::add_mob(Mob& mob) {
		mobs_.push_back(&mob);
	}

	int Team::id() const {
		return number;
	}

	glm::vec3& Team::color() {
		return color_;
	}

	std::vector<Mob*> Team::mobs() {
		return mobs_;
	}
}